#include "voice_base_input.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kSampleRateHz = 16000;
constexpr uint8_t kDcEstimateShift = 6;

uint16_t absoluteSample(int32_t value) {
  return static_cast<uint16_t>(value < 0 ? -value : value);
}

}  // namespace

bool VoiceBaseInput::begin() {
  releaseAnalysisRing();
  ready_ = false;
  captureActive_.store(false, std::memory_order_release);
  pendingQueueMask_.store(0, std::memory_order_release);
  analysisRead_.store(0, std::memory_order_release);
  analysisWrite_.store(0, std::memory_order_release);
  analysisDrops_.store(0, std::memory_order_release);
  requeueFailures_.store(0, std::memory_order_release);
  analysisBacklogHighWater_.store(0, std::memory_order_release);
  hasSamples_ = false;
  dcEstimate_ = 0;
  pendingLevelSum_ = 0;
  pendingPeak_ = 0;
  pendingBlockCount_ = 0;
  moodFeatures_ = AudioMoodFeatures{};
  vu_ = IrisVu{};
  tempoTracker_ = TempoTracker{};
  tempoMetrics_ = ExperimentalTempoMetrics{};
  sampleClockOriginMs_ = millis();
  capturedSamples_ = 0;
  capturedSequence_ = 0;
  lastAnalysisSequence_ = 0;
  hasAnalysisSequence_ = false;
  analysisRing_ = static_cast<int16_t*>(
    heap_caps_malloc(analysisRingBytes(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (analysisRing_ == nullptr) return false;
  auto config = M5.Mic.config();
  config.sample_rate = kSampleRateHz;
  config.dma_buf_count = 4;
  config.dma_buf_len = kSampleCount;
  config.over_sampling = 1;
  config.magnification = 1;
  config.noise_filter_level = 0;
  config.task_pinned_core = 0;
  M5.Mic.config(config);
  M5.Mic.setBufferReleaseCallback(this, onBufferReady);

  if (!M5.Mic.isEnabled() || !M5.Mic.begin()) {
    M5.Mic.setBufferReleaseCallback(nullptr, nullptr);
    releaseAnalysisRing();
    return false;
  }
  captureActive_.store(true, std::memory_order_release);
  if (!queueBuffer(0) || !queueBuffer(1)) {
    captureActive_.store(false, std::memory_order_release);
    M5.Mic.end();
    M5.Mic.setBufferReleaseCallback(nullptr, nullptr);
    releaseAnalysisRing();
    return false;
  }

  ready_ = true;
  return true;
}

void VoiceBaseInput::suspend() {
  ready_ = false;
  captureActive_.store(false, std::memory_order_release);
  M5.Mic.end(); // Waits for callbacks; never called from the main loop for sound.
  M5.Mic.setBufferReleaseCallback(nullptr, nullptr);
  pendingQueueMask_.store(0, std::memory_order_release);
  analysisRead_.store(0, std::memory_order_release);
  analysisWrite_.store(0, std::memory_order_release);
  releaseAnalysisRing();
  hasSamples_ = false;
}

void VoiceBaseInput::discardCompleted() {
  if (!ready_) return;
  // The capture task still owns the hardware queue.  This only drops stale
  // analysis copies, never a microphone request.
  analysisRead_.store(analysisWrite_.load(std::memory_order_acquire),
                      std::memory_order_release);
}

void VoiceBaseInput::update(BeatAnalyzer& analyzer) {
  if (!ready_) return;

  const uint8_t pending = pendingQueueMask_.exchange(0, std::memory_order_acq_rel);
  for (uint8_t index = 0; index < 2; ++index) {
    if ((pending & (1U << index)) && !queueBuffer(index)) {
      pendingQueueMask_.fetch_or(1U << index, std::memory_order_release);
    }
  }
  // Single producer (capture callback), single consumer (main loop).  The
  // producer publishes each PCM copy with release semantics after the copy.
  for (;;) {
    const uint8_t read = analysisRead_.load(std::memory_order_relaxed);
    const uint8_t write = analysisWrite_.load(std::memory_order_acquire);
    if (read == write) break;
    processBuffer(analysisRing_ + static_cast<size_t>(read) * kSampleCount,
                  analysisTimesMs_[read], analysisSequences_[read], analyzer);
    analysisRead_.store(static_cast<uint8_t>((read + 1U) % kAnalysisRingFrames),
                        std::memory_order_release);
  }
}

void VoiceBaseInput::onBufferReady(void* context, void* data, size_t length) {
  auto* self = static_cast<VoiceBaseInput*>(context);
  if (self == nullptr || length != kSampleCount ||
      !self->captureActive_.load(std::memory_order_acquire)) return;
  const uint8_t index = data == self->buffers_[0] ? 0 :
                        (data == self->buffers_[1] ? 1 : 2);
  if (index >= 2 || self->analysisRing_ == nullptr) return;

  // M5Unified releases the request slot before this callback and explicitly
  // supports record() here at the current sample rate.  Requeue first-class
  // capture buffers here, rather than waiting for display work in loop().
  const uint8_t write = self->analysisWrite_.load(std::memory_order_relaxed);
  const uint8_t next = static_cast<uint8_t>((write + 1U) % kAnalysisRingFrames);
  const uint8_t read = self->analysisRead_.load(std::memory_order_acquire);
  const uint32_t sequence = self->capturedSequence_++;
  self->capturedSamples_ += kSampleCount;
  const uint32_t audioNowMs = self->sampleClockOriginMs_ + static_cast<uint32_t>(
    (self->capturedSamples_ * 1000ULL) / kSampleRateHz);
  if (next == read) {
    self->analysisDrops_.fetch_add(1, std::memory_order_relaxed);
  } else {
    memcpy(self->analysisRing_ + static_cast<size_t>(write) * kSampleCount, data,
           kSampleCount * sizeof(int16_t));
    self->analysisTimesMs_[write] = audioNowMs;
    self->analysisSequences_[write] = sequence;
    self->analysisWrite_.store(next, std::memory_order_release);
    const uint8_t backlog = static_cast<uint8_t>(
      (next + kAnalysisRingFrames - read) % kAnalysisRingFrames);
    uint8_t highWater = self->analysisBacklogHighWater_.load(std::memory_order_relaxed);
    while (backlog > highWater && !self->analysisBacklogHighWater_.compare_exchange_weak(
             highWater, backlog, std::memory_order_relaxed)) {}
  }
  if (!self->queueBuffer(index)) {
    self->requeueFailures_.fetch_add(1, std::memory_order_relaxed);
    self->pendingQueueMask_.fetch_or(1U << index, std::memory_order_release);
  }
}

bool VoiceBaseInput::queueBuffer(uint8_t index) {
  return M5.Mic.record(buffers_[index], kSampleCount, kSampleRateHz, false);
}

void VoiceBaseInput::processBuffer(const int16_t* samples, uint32_t audioNowMs,
                                   uint32_t sequence, BeatAnalyzer& analyzer) {
  // The capture callback derives time from continuous 16 kHz sample count;
  // foreground scheduling no longer changes the audio clock.  A full analysis
  // ring is a real discontinuity, so discard tempo history rather than making
  // a false BPM claim from non-contiguous PCM.
  const uint32_t foregroundNowMs = millis();
  if (hasAnalysisSequence_ && sequence != lastAnalysisSequence_ + 1U) {
    tempoTracker_.reset();
  }
  lastAnalysisSequence_ = sequence;
  hasAnalysisSequence_ = true;

  moodFeatures_.feed(audioNowMs, samples, kSampleCount);
  const uint32_t tempoStartedAtUs = micros();
  tempoTracker_.feed(audioNowMs, samples, kSampleCount);
  const uint32_t tempoElapsedUs = micros() - tempoStartedAtUs;
  ++tempoMetrics_.processedFrames;
  tempoMetrics_.totalMicros += tempoElapsedUs;
  if (tempoElapsedUs > tempoMetrics_.maxMicros) tempoMetrics_.maxMicros = tempoElapsedUs;
  uint32_t absoluteSum = 0;
  uint16_t peak = 0;
  for (size_t i = 0; i < kSampleCount; ++i) {
    const int16_t sample = samples[i];
    // This is the same continuous DC estimate used by the V1 PDM input.
    // It preserves the envelope across I2S buffer boundaries.
    dcEstimate_ += (static_cast<int32_t>(sample) - dcEstimate_) >> kDcEstimateShift;
    const int32_t centered = static_cast<int32_t>(sample) - dcEstimate_;
    const uint16_t magnitude = absoluteSample(centered);
    absoluteSum += magnitude;
    if (magnitude > peak) peak = magnitude;
  }
  const uint32_t blockLevel = absoluteSum / kSampleCount;
  lastSampleAtMs_ = foregroundNowMs;
  hasSamples_ = true;
  vu_.feed(lastSampleAtMs_, blockLevel);

  // V1 analysed four 256-sample I2S reads together.  At 16 kHz that yields
  // a roughly 64 ms frame, which keeps its envelope and onset thresholds
  // meaningful on the Voice Base as well.
  pendingLevelSum_ += blockLevel;
  if (peak > pendingPeak_) pendingPeak_ = peak;
  ++pendingBlockCount_;
  if (pendingBlockCount_ < kAnalysisBlocksPerFrame) return;

  analyzer.feed(audioNowMs, pendingLevelSum_ / pendingBlockCount_, pendingPeak_);
  pendingLevelSum_ = 0;
  pendingPeak_ = 0;
  pendingBlockCount_ = 0;
}

void VoiceBaseInput::releaseAnalysisRing() {
  if (analysisRing_ == nullptr) return;
  heap_caps_free(analysisRing_);
  analysisRing_ = nullptr;
}

}  // namespace decaflash::mainframe
