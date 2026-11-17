#include "slm_task.h"

#if DECAFLASH_SLM

// The whole kernel is compiled in this file, so its optimisation level is set
// here. The firmware builds with -Os; the kernel is 1.7 times faster at -O3 on
// the device (docs/slm-prototype.md). Measurement builds only: GCC documents
// this pragma as not meant for production code.
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
#include <freertos/task.h>

#include "slm_kernel.h"
#include "slm_prompt.h"

namespace decaflash {
namespace mainframe {
namespace {

constexpr int kMaxPieces = 48;
constexpr BaseType_t kModelCore = 0;     // the radio's core; loop() runs on core 1
constexpr UBaseType_t kModelPriority = 1;  // lowest application priority, below the radio stack
constexpr uint32_t kStackBytes = 8192;
// Matrix products per layer in llm_forward. After that many the task sleeps
// one tick so the idle task of core 0 runs and its watchdog stays quiet.
constexpr uint32_t kMatvecsPerYield = 7;
constexpr esp_partition_subtype_t kModelSubtype = static_cast<esp_partition_subtype_t>(0x40);

slm::Kernel kernel;
slm::PromptBlock prompt;
TaskHandle_t taskHandle = nullptr;
const char* state = "not started";
uint32_t matvecsSinceYield = 0;

std::atomic<uint32_t> forwardCount{0};
std::atomic<uint32_t> forwardSumUs{0};
std::atomic<uint32_t> forwardMaxUs{0};
std::atomic<uint32_t> pieceCount{0};
std::atomic<uint32_t> runCount{0};
std::atomic<uint32_t> mismatchCount{0};

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

int timedStep(int token) {
  const int64_t startedUs = esp_timer_get_time();
  const int next = kernel.step(token);
  const uint32_t us = static_cast<uint32_t>(esp_timer_get_time() - startedUs);
  forwardCount.fetch_add(1, std::memory_order_relaxed);
  forwardSumUs.fetch_add(us, std::memory_order_relaxed);
  if (us > forwardMaxUs.load(std::memory_order_relaxed)) {
    forwardMaxUs.store(us, std::memory_order_relaxed);
  }
  return next;
}

// Generates without pause: the worst case for the rest of the firmware.
void modelTask(void*) {
  const int outputVocab = kernel.outputVocab();
  const bool useBlock = prompt.valid() && static_cast<int>(prompt.tableCount()) == outputVocab;
  for (;;) {
    kernel.reset();
    int next = -1;
    if (useBlock) {
      bool fed = true;
      for (uint32_t i = 0; i < prompt.promptCount() && fed; ++i) {
        fed = timedStep(static_cast<int>(prompt.prompt(i))) >= 0;
      }
      next = fed ? timedStep(static_cast<int>(prompt.bosInput())) : -1;
    } else {
      // No prompt block (random-weight image): fixed tokens, classes fed back as tokens.
      timedStep(1);
      timedStep(2);
      next = timedStep(3);
    }
    int pieces = 0;
    bool match = useBlock;
    while (pieces < kMaxPieces && next >= 0) {
      if (useBlock && next == static_cast<int>(prompt.eosClass())) break;
      if (useBlock && (pieces >= static_cast<int>(prompt.expectedCount()) ||
                       next != static_cast<int>(prompt.expected(pieces)))) {
        match = false;
      }
      ++pieces;
      pieceCount.fetch_add(1, std::memory_order_relaxed);
      next = timedStep(useBlock ? static_cast<int>(prompt.inputForClass(next)) : next);
    }
    if (useBlock && (!match || pieces != static_cast<int>(prompt.expectedCount()))) {
      mismatchCount.fetch_add(1, std::memory_order_relaxed);
    }
    runCount.fetch_add(1, std::memory_order_relaxed);
    vTaskDelay(1);
  }
}

}  // namespace

void slmBegin() {
  const size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t internalBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  const esp_partition_t* partition =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, kModelSubtype, "model");
  if (partition == nullptr) {
    state = "no model partition";
    Serial.printf("SLM off: %s\n", state);
    return;
  }
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
  if (mapped != ESP_OK) {
    state = "mapping failed";
    Serial.printf("SLM off: %s\n", state);
    return;
  }
  const uint8_t* image = static_cast<const uint8_t*>(base);
  const slm::LoadResult result = kernel.load(image, partition->size);
  if (result != slm::LoadResult::Ok) {
    state = slm::loadResultName(result);
    Serial.printf("SLM off: %s\n", state);
    return;
  }
  if (!kernel.allocate(psramAlloc, internalAlloc, psramAlloc)) {
    state = "working memory unavailable";
    Serial.printf("SLM off: %s\n", state);
    return;
  }
  prompt.parse(image + kernel.imageBytes(), partition->size - kernel.imageBytes());
  kernel.setLayerMatvec(layerMatvec);
  if (xTaskCreatePinnedToCore(modelTask, "slm", kStackBytes, nullptr, kModelPriority, &taskHandle,
                              kModelCore) != pdPASS) {
    taskHandle = nullptr;
    state = "task not created";
    Serial.printf("SLM off: %s\n", state);
    return;
  }
  state = "running";
  Serial.printf(
      "SLM on: image_bytes=%u dim=%d layers=%d vocab_in=%d vocab_out=%d prompt_block=%s "
      "psram_used=%u internal_used=%u o3=%d\n",
      static_cast<unsigned>(kernel.imageBytes()), kernel.dimension(), kernel.layers(),
      kernel.inputVocab(), kernel.outputVocab(), prompt.valid() ? "yes" : "no",
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
  report.mismatches = mismatchCount.load(std::memory_order_relaxed);
  report.stackFreeBytes = taskHandle == nullptr ? 0 : uxTaskGetStackHighWaterMark(taskHandle);
  return report;
}

}  // namespace mainframe
}  // namespace decaflash

#endif  // DECAFLASH_SLM
