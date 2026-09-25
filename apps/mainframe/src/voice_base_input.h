#pragma once

#include <Arduino.h>
#include <atomic>

#include "beat_analyzer.h"

namespace decaflash::mainframe {

// Voice Base I2S capture. It owns capture buffers and never changes show state.
class VoiceBaseInput {
 public:
  bool begin();
  void update(BeatAnalyzer& analyzer);

  bool ready() const { return ready_; }

 private:
  static void onBufferReady(void* context, void* data, size_t length);
  bool queueBuffer(uint8_t index);
  void processBuffer(uint8_t index, BeatAnalyzer& analyzer);

  static constexpr size_t kSampleCount = 256;
  static constexpr uint8_t kAnalysisBlocksPerFrame = 4;
  int16_t buffers_[2][kSampleCount] = {};
  std::atomic<uint8_t> completedMask_{0};
  int32_t dcEstimate_ = 0;
  uint32_t pendingLevelSum_ = 0;
  uint16_t pendingPeak_ = 0;
  uint8_t pendingBlockCount_ = 0;
  bool ready_ = false;
};

}  // namespace decaflash::mainframe
