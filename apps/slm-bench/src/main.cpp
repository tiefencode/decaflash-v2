// Measurement only: language model prototype benchmark (docs/slm-prototype.md,
// Decision 8). Runs nothing but the model kernel on one core for the fixed
// prompt stored after the model image, and reports time per output piece. It
// has no audio, eye, radio or sensors, so it shows the pure compute speed of a
// core and compiler, not the behaviour of the Mainframe firmware.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_timer.h>

#include "slm/slm_kernel.h"
#include "slm/slm_prompt.h"

namespace {

constexpr int kMaxPieces = 48;
constexpr uint32_t kPauseBetweenRunsMs = 2000;
constexpr esp_partition_subtype_t kModelSubtype = static_cast<esp_partition_subtype_t>(0x40);

slm::Kernel kernel;
slm::PromptBlock prompt;
bool ready = false;
uint32_t runCount = 0;

void* psramAlloc(size_t bytes) { return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM); }
void* internalAlloc(size_t bytes) {
  return heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

const uint8_t* mapModelPartition(size_t* size) {
  const esp_partition_t* partition =
      esp_partition_find_first(ESP_PARTITION_TYPE_DATA, kModelSubtype, "model");
  if (partition == nullptr) return nullptr;
  const void* base = nullptr;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_partition_mmap_handle_t handle;
  const esp_err_t result =
      esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &base, &handle);
#else
  spi_flash_mmap_handle_t handle;
  const esp_err_t result =
      esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &base, &handle);
#endif
  if (result != ESP_OK) return nullptr;
  *size = partition->size;
  return static_cast<const uint8_t*>(base);
}

void printBuild() {
  Serial.printf("SLM_BENCH build arduino=%d.%d.%d gcc=%s optimize=%s cpu_mhz=%u\n",
                ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH,
                __VERSION__,
#if defined(__OPTIMIZE_SIZE__)
                "Os",
#elif defined(__OPTIMIZE__)
                "O2-or-O3",
#else
                "O0",
#endif
                static_cast<unsigned>(getCpuFrequencyMhz()));
}

void runOnce() {
  const int outputVocab = kernel.outputVocab();
  const bool useBlock = prompt.valid() && static_cast<int>(prompt.tableCount()) == outputVocab;
  bool fed = true;
  const int64_t startedUs = esp_timer_get_time();
  kernel.reset();
  int next = -1;
  if (useBlock) {
    for (uint32_t i = 0; i < prompt.promptCount() && fed; ++i) {
      fed = kernel.step(static_cast<int>(prompt.prompt(i))) >= 0;
    }
    next = fed ? kernel.step(static_cast<int>(prompt.bosInput())) : -1;
  } else {
    // No prompt block (random-weight image): fixed tokens, classes fed back as tokens.
    kernel.step(1);
    kernel.step(2);
    next = kernel.step(3);
  }
  const int64_t promptDoneUs = esp_timer_get_time();
  static int pieces[kMaxPieces];
  int pieceCount = 0;
  while (pieceCount < kMaxPieces && next >= 0) {
    if (useBlock && next == static_cast<int>(prompt.eosClass())) break;
    pieces[pieceCount++] = next;
    next = kernel.step(useBlock ? static_cast<int>(prompt.inputForClass(next)) : next);
  }
  const int64_t doneUs = esp_timer_get_time();

  bool match = useBlock && pieceCount == static_cast<int>(prompt.expectedCount());
  for (int i = 0; match && i < pieceCount; ++i) {
    match = pieces[i] == static_cast<int>(prompt.expected(i));
  }
  const int forwards = kernel.position();
  const float totalMs = static_cast<float>(doneUs - startedUs) / 1000.0f;
  const float generateMs = static_cast<float>(doneUs - promptDoneUs) / 1000.0f;
  Serial.printf(
      "SLM_BENCH run=%u pieces=%d forwards=%d total_ms=%.1f ms_per_forward=%.2f "
      "ms_per_piece_generating=%.2f ms_per_piece_with_prompt=%.2f match=%s\n",
      static_cast<unsigned>(++runCount), pieceCount, forwards, totalMs,
      forwards > 0 ? totalMs / forwards : 0.0f, pieceCount > 0 ? generateMs / pieceCount : 0.0f,
      pieceCount > 0 ? totalMs / pieceCount : 0.0f, useBlock ? (match ? "yes" : "NO") : "n/a");
  Serial.print("SLM_BENCH classes");
  for (int i = 0; i < pieceCount; ++i) Serial.printf(" %d", pieces[i]);
  Serial.println();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitStartedMs = millis();
  while (!Serial && millis() - waitStartedMs < 3000) delay(10);
  delay(500);
  printBuild();

  const size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t internalBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t partitionBytes = 0;
  const uint8_t* base = mapModelPartition(&partitionBytes);
  if (base == nullptr) {
    Serial.println("SLM_BENCH off: no model partition or mapping failed");
    return;
  }
  const slm::LoadResult result = kernel.load(base, partitionBytes);
  if (result != slm::LoadResult::Ok) {
    Serial.printf("SLM_BENCH off: %s\n", slm::loadResultName(result));
    return;
  }
  if (!kernel.allocate(psramAlloc, internalAlloc, psramAlloc)) {
    Serial.println("SLM_BENCH off: working memory could not be allocated");
    return;
  }
  prompt.parse(base + kernel.imageBytes(), partitionBytes - kernel.imageBytes());
  Serial.printf(
      "SLM_BENCH model image_bytes=%u vocab_in=%d vocab_out=%d dim=%d layers=%d positions=%d "
      "prompt_block=%s psram_used=%u internal_used=%u psram_free=%u internal_free=%u\n",
      static_cast<unsigned>(kernel.imageBytes()), kernel.inputVocab(), kernel.outputVocab(),
      kernel.dimension(), kernel.layers(), kernel.contextLength(), prompt.valid() ? "yes" : "no",
      static_cast<unsigned>(psramBefore - heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
      static_cast<unsigned>(internalBefore - heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  ready = true;
}

void loop() {
  if (!ready) {
    delay(1000);
    return;
  }
  runOnce();
  delay(kPauseBetweenRunsMs);
}
