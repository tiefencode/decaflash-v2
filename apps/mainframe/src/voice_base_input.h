#pragma once

#include <Arduino.h>
#include <atomic>

#include "beat_analyzer.h"
#include "iris_vu.h"
#include "audio_mood_features.h"

namespace decaflash::mainframe {

// Voice Base I2S capture. It owns capture buffers and never changes show state.
class VoiceBaseInput {
 public:
  bool begin();
  void update(BeatAnalyzer& analyzer);

  uint8_t vuLevel(uint32_t now) const { return ready_ ? vu_.level(now) : 0; }

  bool fresh(uint32_t now) const { return ready_ && hasSamples_ && now - lastSampleAtMs_ <= 250; }

  const AudioMoodFeatures& moodFeatures() const { return moodFeatures_; }

  bool ready() const { return ready_; }

 private:
  static void onBufferReady(void* context, void* data, size_t length);
  bool queueBuffer(uint8_t index);
  void processBuffer(uint8_t index, BeatAnalyzer& analyzer);

  static constexpr size_t kSampleCount = 256;
  static constexpr uint8_t kAnalysisBlocksPerFrame = 4;
  int16_t buffers_[2][kSampleCount] = {};
  std::atomic<uint8_t> completedMask_{0};
  IrisVu vu_;
  AudioMoodFeatures moodFeatures_;
  int32_t dcEstimate_ = 0;
  uint32_t pendingLevelSum_ = 0;
  uint16_t pendingPeak_ = 0;
  uint8_t pendingBlockCount_ = 0;
  uint32_t lastSampleAtMs_ = 0;
  bool hasSamples_ = false;
  bool ready_ = false;
};

}  // namespace decaflash::mainframe
