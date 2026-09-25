#pragma once

#include <cstdint>

namespace decaflash::mainframe {

// Hardware-independent port of the V1 onset and tempo-estimation core.
class BeatAnalyzer {
 public:
  void feed(uint32_t nowMs, uint32_t blockLevel, uint16_t peakLevel);

  bool musicPresent() const { return musicPresent_; }
  uint16_t detectedBpm() const { return detectedBpm_; }
  uint16_t clockBpm() const { return clockBpm_; }
  uint8_t confidence() const { return confidence_; }
  uint32_t lastOnsetAtMs() const { return lastOnsetAtMs_; }

 private:
  void recordPulseFrame(uint32_t nowMs, uint32_t transientLevel, uint32_t onsetThreshold);
  void recordEnergyFrame(uint32_t blockLevel);
  void registerOnset(uint32_t nowMs, uint32_t onsetStrength, uint32_t intervalMs);
  void updateTempoEstimate();
  bool resolvePeriodicTempo(uint16_t& bpm, uint8_t& confidence) const;

  uint32_t analysisFastLevel_ = 0;
  uint32_t analysisSlowLevel_ = 0;
  uint32_t analysisFloor_ = 0;
  uint32_t onsetStrength_ = 0;
  uint32_t lastOnsetAtMs_ = 0;
  uint16_t detectedBpm_ = 0;
  uint16_t clockBpm_ = 0;
  uint16_t clockSubdivisionCandidateBpm_ = 0;
  uint8_t clockSubdivisionCandidateCount_ = 0;
  uint8_t confidence_ = 0;
  bool musicPresent_ = false;
  uint8_t onsetTimestampCount_ = 0;
  uint32_t onsetTimestampsMs_[8] = {};
  uint8_t onsetIntervalCount_ = 0;
  uint32_t onsetIntervalsMs_[8] = {};
  uint32_t tempoBucketScores_[101] = {};
  uint32_t lastAnalysisFrameAtMs_ = 0;
  uint16_t analysisFrameIntervalMs_ = 64;
  uint8_t pulseHistoryCount_ = 0;
  uint8_t pulseHistoryIndex_ = 0;
  uint8_t pulseHistory_[64] = {};
  uint16_t previousEnergyLevel_ = 0;
  uint16_t energyHistoryCount_ = 0;
  uint16_t energyHistoryIndex_ = 0;
  uint16_t fluxHistory_[256] = {};
};

}  // namespace decaflash::mainframe
