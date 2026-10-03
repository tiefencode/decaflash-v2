#pragma once

#include <cstdint>

namespace decaflash::mainframe {

// A bounded show-side confirmation step. It samples only new tempo
// evaluations, so two matching observations cost about 500 ms, not a long
// song-change lock. A return value of zero means keep the current show rate.
class V2TempoAdoption {
 public:
  uint16_t observe(uint32_t evaluation, uint16_t currentBpm, uint16_t bpm,
                   uint8_t confidence, uint32_t onsetAtMs, uint32_t nowMs) {
    if (evaluation == lastEvaluation_) return 0;
    lastEvaluation_ = evaluation;
    if (bpm < 80U || bpm > 180U || confidence < 30U || onsetAtMs == 0U ||
        nowMs - onsetAtMs > 1500U) {
      clearCandidate();
      return 0;
    }
    if (bpm == currentBpm) {
      clearCandidate();
      return 0;
    }
    const uint16_t difference = bpm > candidateBpm_ ? bpm - candidateBpm_
                                                      : candidateBpm_ - bpm;
    if (candidateBpm_ == 0U || difference > 4U) {
      candidateBpm_ = bpm;
      candidateCount_ = 1;
      return 0;
    }
    candidateBpm_ = static_cast<uint16_t>((candidateBpm_ + bpm + 1U) / 2U);
    ++candidateCount_;
    if (candidateCount_ < 2U) return 0;
    const uint16_t accepted = candidateBpm_;
    clearCandidate();
    return accepted;
  }

  void reset() {
    lastEvaluation_ = 0;
    clearCandidate();
  }

 private:
  void clearCandidate() {
    candidateBpm_ = 0;
    candidateCount_ = 0;
  }

  uint32_t lastEvaluation_ = 0;
  uint16_t candidateBpm_ = 0;
  uint8_t candidateCount_ = 0;
};

}  // namespace decaflash::mainframe
