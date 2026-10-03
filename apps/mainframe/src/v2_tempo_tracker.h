#pragma once

#include <cstddef>
#include <cstdint>

namespace decaflash::mainframe {

// V2's bounded, device-local PCM tempo estimator. It consumes consecutive
// 16 kHz PCM blocks and provides the productive show-tempo input.
class V2TempoTracker {
 public:
  struct Estimate {
    uint16_t bpm = 0;
    uint8_t confidence = 0;
    uint16_t rawBpm = 0;
    uint8_t rawConfidence = 0;
    uint16_t directBpm = 0;
    uint16_t harmonicBpm = 0;
    uint16_t periodicityPermille = 0;
    uint16_t harmonicPeriodicityPermille = 0;
    uint16_t onsetSupport = 0;
    uint16_t harmonicOnsetSupport = 0;
    uint16_t fastRms = 0;
    uint16_t slowRms = 0;
    int16_t levelTrendPermille = 0;
    uint16_t onsetStrengthPermille = 0;
    uint32_t lastOnsetAtMs = 0;
    uint32_t analysisFrames = 0;
    uint32_t tempoEvaluations = 0;
  };

  // timestampMs must advance according to the audio sample clock, rather than
  // the time at which the foreground loop happened to consume the buffer.
  void feed(uint32_t timestampMs, const int16_t* samples, size_t count);
  // Replays the V2 onset-strength trace used by the device capture fixture.
  // This tests V2's timing/tempo decision without pretending the trace is PCM.
  void feedOnsetTrace(uint32_t timestampMs, uint16_t onsetStrengthPermille);
  void reset();

  const Estimate& estimate() const { return estimate_; }

 private:
  static constexpr uint16_t kHistorySize = 512;
  static constexpr uint16_t kCorrelationHistorySize = kHistorySize / 2;

  void recordOnsetStrength(uint32_t timestampMs, float strength);
  void processOnsetStrength(uint32_t timestampMs, float strength);
  void updateTempo();
  float historyAt(uint16_t age) const;
  bool directOnsetTempo(uint16_t& bpm) const;
  uint16_t onsetTempoSupport(uint16_t bpm) const;

  float dcEstimate_ = 0;
  float bassLow1_ = 0;
  float bassLow2_ = 0;
  float bandLow_[4] = {};
  float previousBandRms_[4] = {};
  float previousWideRms_ = 0;
  float previousBassRms_ = 0;
  float fastRms_ = 0;
  float slowRms_ = 0;
  float onsetMean_ = 0;
  float onsetDeviation_ = 0;
  float previousPreviousOnset_ = 0;
  float previousOnset_ = 0;
  uint32_t previousOnsetAtMs_ = 0;
  uint32_t previousTimestampMs_ = 0;
  uint32_t lastTempoUpdateAtMs_ = 0;
  uint16_t historyIndex_ = 0;
  uint16_t historyCount_ = 0;
  float onsetHistory_[kHistorySize] = {};
  // A decimated, chronological working view avoids modulo arithmetic in the
  // correlation inner loop.  It is only used when a tempo update is due.
  float correlationHistory_[kCorrelationHistorySize] = {};
  uint8_t onsetTimestampIndex_ = 0;
  uint8_t onsetTimestampCount_ = 0;
  uint32_t onsetTimestampsMs_[16] = {};
  Estimate estimate_;
};

}  // namespace decaflash::mainframe
