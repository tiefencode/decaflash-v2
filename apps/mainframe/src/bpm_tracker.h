#pragma once

#include <cstdint>

namespace decaflash::mainframe {

// A bounded, device-local tempo estimator.  It operates on the 16 ms level
// frames already calculated by VoiceBaseInput, so it neither retains PCM nor
// introduces an FFT/library dependency.
class BpmTracker {
 public:
  struct Estimate {
    uint16_t bpm = 0;
    uint8_t confidence = 0;
    uint16_t rawBpm = 0;
    uint8_t rawConfidence = 0;
    uint16_t periodicityPermille = 0;
    uint16_t directSupportPermille = 0;
    uint8_t onsetCount = 0;
    uint32_t analyzedFrames = 0;
  };

  void reset();
  void feed(uint32_t timestampMs, uint32_t level);
  const Estimate& estimate() const { return estimate_; }

 private:
  static constexpr uint16_t kMinimumBpm = 80;
  static constexpr uint16_t kMaximumBpm = 180;
  static constexpr uint16_t kHistorySize = 512;  // just over eight seconds at 16 ms
  static constexpr uint8_t kEventHistorySize = 16;

  uint16_t historyAt(uint16_t chronologicalIndex) const;
  uint16_t scoreCandidate(uint16_t bpm, uint16_t& periodicity,
                          uint16_t& directSupport) const;
  void registerOnset(uint32_t timestampMs);
  void evaluate(uint32_t timestampMs);

  uint16_t pulseHistory_[kHistorySize] = {};
  uint16_t historyCount_ = 0;
  uint16_t historyWrite_ = 0;
  uint16_t previousLevel_ = 0;
  uint32_t fluxNoiseFloor_ = 0;
  uint16_t frameIntervalMs_ = 16;
  uint32_t previousFrameAtMs_ = 0;
  uint32_t lastEvaluationAtMs_ = 0;
  uint32_t lastOnsetAtMs_ = 0;
  uint32_t onsetTimesMs_[kEventHistorySize] = {};
  uint8_t onsetCount_ = 0;
  uint16_t pendingBpm_ = 0;
  uint8_t pendingCount_ = 0;
  uint16_t lockedBpm_ = 0;
  uint16_t switchBpm_ = 0;
  uint8_t switchCount_ = 0;
  Estimate estimate_ = {};
};

}  // namespace decaflash::mainframe
