#include "slm_task.h"

#if DECAFLASH_SLM

// The whole kernel is compiled in this file, so its optimisation level is set
// here. The firmware builds with -Os; the kernel is 1.7 times faster at -O3 on
// the device (docs/slm-prototype.md). GCC documents this pragma as not meant
// for production code; it stays until the move into the normal build (task 9.1).
#ifndef DECAFLASH_SLM_O3
#define DECAFLASH_SLM_O3 1
#endif
#if DECAFLASH_SLM_O3
#pragma GCC optimize("O3")
#endif

#include <Arduino.h>
#include <atomic>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>

#include "slm_kernel.h"
#include "text_writer.h"

namespace decaflash {
namespace mainframe {
namespace {

namespace vocab = text_model;

constexpr BaseType_t kModelCore = 0;     // the radio's core; loop() runs on core 1
constexpr UBaseType_t kModelPriority = 1;  // lowest application priority, below the radio stack
constexpr uint32_t kStackBytes = 8192;
// Emoji, at most 24 words within 47 characters, end.
constexpr UBaseType_t kPieceQueueLength = 32;
// Matrix products per layer in llm_forward. After that many the task sleeps
// one tick so the idle task of core 0 runs and its watchdog stays quiet.
constexpr uint32_t kMatvecsPerYield = 7;
constexpr esp_partition_subtype_t kModelSubtype = static_cast<esp_partition_subtype_t>(0x40);

slm::Kernel kernel;
TaskHandle_t taskHandle = nullptr;
QueueHandle_t pieceQueue = nullptr;
const char* state = "not started";
uint32_t matvecsSinceYield = 0;

portMUX_TYPE requestLock = portMUX_INITIALIZER_UNLOCKED;
TextModelRequest pendingRequest;
bool requestPending = false;
uint32_t pendingRequestedUs = 0;
std::atomic<uint32_t> activeRequestId{0};  // 0: no request or cancelled

std::atomic<uint32_t> forwardCount{0};
std::atomic<uint32_t> forwardSumUs{0};
std::atomic<uint32_t> forwardMaxUs{0};
std::atomic<uint32_t> pieceCount{0};
std::atomic<uint32_t> runCount{0};

void* psramAlloc(size_t bytes) { return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM); }
void* internalAlloc(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void layerMatvec(const QT* tensor, const float* x, float* y) {
  matvec_q8(tensor, x, y);
  if (++matvecsSinceYield >= kMatvecsPerYield) {
    matvecsSinceYield = 0;
    vTaskDelay(1);
  }
}

// The kernel with timing counters, as the writer's model.
struct TimedKernel {
  const float* forward(int token, int firstRow, int rowCount) {
    const int64_t startedUs = esp_timer_get_time();
    const float* logits = kernel.forward(token, firstRow, rowCount);
    const uint32_t us = static_cast<uint32_t>(esp_timer_get_time() - startedUs);
    forwardCount.fetch_add(1, std::memory_order_relaxed);
    forwardSumUs.fetch_add(us, std::memory_order_relaxed);
    if (us > forwardMaxUs.load(std::memory_order_relaxed)) {
      forwardMaxUs.store(us, std::memory_order_relaxed);
    }
    return logits;
  }
};

// Delivers pieces to the queue and records the times for the TEXT line.
struct QueueSink {
  uint32_t requestId = 0;
  uint32_t startedUs = 0;
  uint32_t emojiMs = 0;
  uint32_t firstWordMs = 0;
  char text[200] = {};
  size_t textBytes = 0;

  uint32_t elapsedMs() const {
    return (static_cast<uint32_t>(esp_timer_get_time()) - startedUs) / 1000U;
  }
  bool cancelled() const {
    return activeRequestId.load(std::memory_order_relaxed) != requestId;
  }
  void deliver(const TextModelPiece& piece) {
    // The loop takes pieces every pass; a full queue only waits for it.
    while (!cancelled() && xQueueSend(pieceQueue, &piece, pdMS_TO_TICKS(10)) != pdTRUE) {
    }
    pieceCount.fetch_add(1, std::memory_order_relaxed);
  }
  void append(const char* utf8) {
    const size_t bytes = strlen(utf8);
    if (textBytes + bytes + 2 > sizeof(text)) return;
    if (textBytes) text[textBytes++] = ' ';
    memcpy(text + textBytes, utf8, bytes);
    textBytes += bytes;
    text[textBytes] = '\0';
  }
  void emoji(uint16_t token) {
    emojiMs = elapsedMs();
    TextModelPiece piece;
    piece.requestId = requestId;
    piece.kind = TextModelPiece::Kind::Emoji;
    piece.emoji = vocab::kTokens[token].glyph;
    append(vocab::kTokens[token].utf8);
    deliver(piece);
  }
  void word(const char* utf8) {
    if (!firstWordMs) firstWordMs = elapsedMs();
    TextModelPiece piece;
    piece.requestId = requestId;
    piece.kind = TextModelPiece::Kind::Word;
    strncpy(piece.word, utf8, sizeof(piece.word) - 1);
    append(utf8);
    deliver(piece);
  }
};

void writeRequest(const TextModelRequest& request, uint32_t requestedUs) {
  uint16_t condition[slm::kConditionTokens];
  condition[0] = vocab::kStartToken;
  condition[1] = static_cast<uint16_t>(vocab::kFirstEventToken + static_cast<uint8_t>(request.event));
  for (int value = 0; value < 5; ++value) {
    condition[2 + value] = static_cast<uint16_t>(vocab::kFirstMoodToken + 3 * value +
                                                 static_cast<uint8_t>(request.mood[value]));
  }
  QueueSink sink;
  sink.requestId = request.id;
  sink.startedUs = requestedUs;
  const uint32_t forwardsBefore = forwardCount.load(std::memory_order_relaxed);
  kernel.reset();
  TimedKernel model;
  const slm::WriteEnd end = slm::writeText(model, condition, request.seed, sink);
  if (end == slm::WriteEnd::Done || end == slm::WriteEnd::LengthLimit) {
    TextModelPiece piece;
    piece.requestId = request.id;
    piece.kind = TextModelPiece::Kind::End;
    sink.deliver(piece);
  }
  runCount.fetch_add(1, std::memory_order_relaxed);
  // Times from the request call (the panel's acceptance of the event) on.
  Serial.printf(
      "TEXT id=%lu event=%u mood=%u%u%u%u%u seed=%lu emoji_ms=%lu first_word_ms=%lu "
      "done_ms=%lu end=%s forwards=%lu text=\"%s\"\n",
      static_cast<unsigned long>(request.id), static_cast<unsigned>(request.event),
      static_cast<unsigned>(request.mood[0]), static_cast<unsigned>(request.mood[1]),
      static_cast<unsigned>(request.mood[2]), static_cast<unsigned>(request.mood[3]),
      static_cast<unsigned>(request.mood[4]), static_cast<unsigned long>(request.seed),
      static_cast<unsigned long>(sink.emojiMs), static_cast<unsigned long>(sink.firstWordMs),
      static_cast<unsigned long>(sink.elapsedMs()), slm::writeEndName(end),
      static_cast<unsigned long>(forwardCount.load(std::memory_order_relaxed) - forwardsBefore),
      sink.text);
}

// Sleeps until a request arrives, writes it, and takes the next one.
void modelTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (;;) {
      TextModelRequest request;
      uint32_t requestedUs = 0;
      portENTER_CRITICAL(&requestLock);
      const bool pending = requestPending;
      if (pending) {
        request = pendingRequest;
        requestedUs = pendingRequestedUs;
        requestPending = false;
      }
      portEXIT_CRITICAL(&requestLock);
      if (!pending) break;
      if (activeRequestId.load(std::memory_order_relaxed) != request.id) continue;  // cancelled
      writeRequest(request, requestedUs);
    }
  }
}

class SlmTextModel : public TextModel {
 public:
  bool ready() const override { return taskHandle != nullptr; }
  void request(const TextModelRequest& request) override {
    if (taskHandle == nullptr) return;
    portENTER_CRITICAL(&requestLock);
    pendingRequest = request;
    pendingRequestedUs = static_cast<uint32_t>(esp_timer_get_time());
    requestPending = true;
    activeRequestId.store(request.id, std::memory_order_relaxed);
    portEXIT_CRITICAL(&requestLock);
    xTaskNotifyGive(taskHandle);
  }
  void cancel() override { activeRequestId.store(0, std::memory_order_relaxed); }
  bool poll(TextModelPiece& piece) override {
    return pieceQueue != nullptr && xQueueReceive(pieceQueue, &piece, 0) == pdTRUE;
  }
};

SlmTextModel textModel;

void off(const char* reason) {
  state = reason;
  Serial.printf("SLM off: %s\n", state);
}

}  // namespace

TextModel& slmTextModel() { return textModel; }

void slmBegin() {
  const size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t internalBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const esp_partition_t* partition =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, kModelSubtype, "model");
  if (partition == nullptr) return off("no model partition");
  const void* base = nullptr;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_partition_mmap_handle_t handle;
  const esp_err_t mapped =
      esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &base, &handle);
#else
  spi_flash_mmap_handle_t handle;
  const esp_err_t mapped =
      esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &base, &handle);
#endif
  if (mapped != ESP_OK) return off("mapping failed");
  const uint8_t* image = static_cast<const uint8_t*>(base);
  const slm::LoadResult result = kernel.load(image, partition->size);
  if (result != slm::LoadResult::Ok) return off(slm::loadResultName(result));
  if (kernel.inputVocab() != vocab::kInputTokens || kernel.outputVocab() != vocab::kOutputTokens) {
    return off("image vocabulary does not match");
  }
  if (!kernel.allocate(psramAlloc, internalAlloc, psramAlloc)) {
    return off("working memory unavailable");
  }
  kernel.setLayerMatvec(layerMatvec);
  pieceQueue = xQueueCreate(kPieceQueueLength, sizeof(TextModelPiece));
  if (pieceQueue == nullptr) return off("piece queue unavailable");
  TaskHandle_t created = nullptr;
  if (xTaskCreatePinnedToCore(modelTask, "slm", kStackBytes, nullptr, kModelPriority, &created,
                              kModelCore) != pdPASS) {
    return off("task not created");
  }
  taskHandle = created;
  state = "running";
  Serial.printf(
      "SLM on: image_bytes=%u dim=%d layers=%d vocab_in=%d vocab_out=%d "
      "psram_used=%u internal_used=%u o3=%d\n",
      static_cast<unsigned>(kernel.imageBytes()), kernel.dimension(), kernel.layers(),
      kernel.inputVocab(), kernel.outputVocab(),
      static_cast<unsigned>(psramBefore - heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
      static_cast<unsigned>(internalBefore - heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
      DECAFLASH_SLM_O3);
}

SlmReport slmTakeReport() {
  SlmReport report = {};
  report.state = state;
  report.forwards = forwardCount.exchange(0, std::memory_order_relaxed);
  const uint32_t sumUs = forwardSumUs.exchange(0, std::memory_order_relaxed);
  report.forwardAvgUs = report.forwards == 0 ? 0 : sumUs / report.forwards;
  report.forwardMaxUs = forwardMaxUs.exchange(0, std::memory_order_relaxed);
  report.pieces = pieceCount.load(std::memory_order_relaxed);
  report.runs = runCount.load(std::memory_order_relaxed);
  report.stackFreeBytes = taskHandle == nullptr ? 0 : uxTaskGetStackHighWaterMark(taskHandle);
  return report;
}

}  // namespace mainframe
}  // namespace decaflash

#endif  // DECAFLASH_SLM
