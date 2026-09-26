#include "voice_base_input.h"

#include <M5Unified.h>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kSampleRateHz = 16000;
constexpr uint8_t kDcEstimateShift = 6;

uint16_t absoluteSample(int32_t value) {
  return static_cast<uint16_t>(value < 0 ? -value : value);
}

}  // namespace

bool VoiceBaseInput::begin() {
  // The ES8311 Voice Base shares its I2S path between speaker and microphone.
  // M5Unified's own microphone example stops the speaker before capture.
  M5.Speaker.end();

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

  if (!M5.Mic.isEnabled() || !M5.Mic.begin()) return false;
  if (!queueBuffer(0) || !queueBuffer(1)) {
    M5.Mic.end();
    M5.Mic.setBufferReleaseCallback(nullptr, nullptr);
    return false;
  }

  ready_ = true;
  return true;
}

void VoiceBaseInput::update(BeatAnalyzer& analyzer) {
  if (!ready_) return;

  const uint8_t completed = completedMask_.exchange(0, std::memory_order_acq_rel);
  for (uint8_t index = 0; index < 2; ++index) {
    if ((completed & (1U << index)) == 0) continue;
    processBuffer(index, analyzer);
    if (!queueBuffer(index)) ready_ = false;
  }
}

void VoiceBaseInput::onBufferReady(void* context, void* data, size_t length) {
  auto* self = static_cast<VoiceBaseInput*>(context);
  if (self == nullptr || length != kSampleCount) return;
  const uint8_t index = data == self->buffers_[0] ? 0 :
                        (data == self->buffers_[1] ? 1 : 2);
  if (index < 2) self->completedMask_.fetch_or(1U << index, std::memory_order_release);
}

bool VoiceBaseInput::queueBuffer(uint8_t index) {
  return M5.Mic.record(buffers_[index], kSampleCount, kSampleRateHz, false);
}

void VoiceBaseInput::processBuffer(uint8_t index, BeatAnalyzer& analyzer) {
  moodFeatures_.feed(millis(), buffers_[index], kSampleCount);
  uint32_t absoluteSum = 0;
  uint16_t peak = 0;
  for (const int16_t sample : buffers_[index]) {
    // This is the same continuous DC estimate used by the V1 PDM input.
    // It preserves the envelope across I2S buffer boundaries.
    dcEstimate_ += (static_cast<int32_t>(sample) - dcEstimate_) >> kDcEstimateShift;
    const int32_t centered = static_cast<int32_t>(sample) - dcEstimate_;
    const uint16_t magnitude = absoluteSample(centered);
    absoluteSum += magnitude;
    if (magnitude > peak) peak = magnitude;
  }
  const uint32_t blockLevel = absoluteSum / kSampleCount;
  lastSampleAtMs_ = millis();
  hasSamples_ = true;
  vu_.feed(lastSampleAtMs_, blockLevel);

  // V1 analysed four 256-sample I2S reads together.  At 16 kHz that yields
  // a roughly 64 ms frame, which keeps its envelope and onset thresholds
  // meaningful on the Voice Base as well.
  pendingLevelSum_ += blockLevel;
  if (peak > pendingPeak_) pendingPeak_ = peak;
  ++pendingBlockCount_;
  if (pendingBlockCount_ < kAnalysisBlocksPerFrame) return;

  analyzer.feed(millis(), pendingLevelSum_ / pendingBlockCount_, pendingPeak_);
  pendingLevelSum_ = 0;
  pendingPeak_ = 0;
  pendingBlockCount_ = 0;
}

}  // namespace decaflash::mainframe
