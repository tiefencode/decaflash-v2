#pragma once

#include <cstdint>

namespace decaflash::mainframe {

struct AudioFollowInput {
  bool showRunning = false;
  bool musicPresent = false;
  uint16_t clockBpm = 0;
  uint8_t confidence = 0;
  uint32_t onsetAtMs = 0;
  uint16_t currentBpm = 0;
  uint32_t nowMs = 0;
};

struct AudioFollowOutput {
  bool setBpm = false;
  uint16_t bpm = 0;
  bool acquired = false;
  uint32_t onsetAtMs = 0;
};

// Conservative V1-derived lock and one-BPM-at-a-time follow policy.
class AudioFollower {
 public:
  AudioFollowOutput update(const AudioFollowInput& input);
  void reset();

 private:
  uint32_t lastOnsetAtMs_ = 0;
  uint16_t candidateBpm_ = 0;
  uint8_t candidateCount_ = 0;
  uint16_t followCandidateBpm_ = 0;
  uint8_t followCandidateCount_ = 0;
  bool locked_ = false;
};

}  // namespace decaflash::mainframe
