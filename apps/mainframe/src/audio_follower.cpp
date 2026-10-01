#include "audio_follower.h"

namespace decaflash::mainframe {
namespace {

// The analyzer has already separated tempo estimation from its confidence.
// Do not make the show wait for an even stricter, second lock here: two
// accepted estimates at the analyzer's locked threshold are enough to move
// the clock, while isolated estimates still cannot change the show.
constexpr uint8_t kCandidateConfidence = 62;
constexpr uint8_t kLockedConfidence = 62;
constexpr uint8_t kRequiredOnsets = 3;
constexpr uint8_t kBpmTolerance = 4;
constexpr uint32_t kLostSignalMs = 4000;
constexpr uint16_t kMaximumFollowerBpm = 200;

uint16_t difference(uint16_t left, uint16_t right) {
  return left > right ? left - right : right - left;
}

}  // namespace

void AudioFollower::reset() {
  lastOnsetAtMs_ = 0;
  candidateBpm_ = 0;
  candidateCount_ = 0;
  followCandidateBpm_ = 0;
  followCandidateCount_ = 0;
  locked_ = false;
}

AudioFollowOutput AudioFollower::update(const AudioFollowInput& input) {
  AudioFollowOutput output = {};
  if (!input.showRunning) {
    reset();
    return output;
  }

  const bool freshOnset = input.musicPresent && input.clockBpm != 0 &&
    input.onsetAtMs != 0 && input.onsetAtMs != lastOnsetAtMs_;
  if (input.clockBpm > kMaximumFollowerBpm) return output;
  if (!freshOnset) {
    if (locked_ && input.nowMs - lastOnsetAtMs_ > kLostSignalMs) reset();
    return output;
  }
  lastOnsetAtMs_ = input.onsetAtMs;

  if (!locked_) {
    if (input.confidence < kCandidateConfidence) {
      candidateCount_ = 0;
      candidateBpm_ = 0;
      return output;
    }
    if (candidateCount_ == 0 || difference(candidateBpm_, input.clockBpm) > kBpmTolerance) {
      candidateBpm_ = input.clockBpm;
      candidateCount_ = 1;
      return output;
    }
    candidateBpm_ = static_cast<uint16_t>((candidateBpm_ + input.clockBpm + 1U) / 2U);
    if (candidateCount_ < kRequiredOnsets) ++candidateCount_;
    if (candidateCount_ < kRequiredOnsets) return output;

    locked_ = true;
    output.setBpm = true;
    output.bpm = candidateBpm_;
    output.acquired = true;
    output.onsetAtMs = input.onsetAtMs;
    followCandidateBpm_ = 0;
    followCandidateCount_ = 0;
    return output;
  }

  if (input.confidence < kLockedConfidence) return output;
  if (input.clockBpm == input.currentBpm) {
    followCandidateBpm_ = 0;
    followCandidateCount_ = 0;
    return output;
  }
  if (followCandidateCount_ == 0 ||
      difference(followCandidateBpm_, input.clockBpm) > kBpmTolerance) {
    followCandidateBpm_ = input.clockBpm;
    followCandidateCount_ = 1;
    return output;
  }
  followCandidateBpm_ = static_cast<uint16_t>(
    (followCandidateBpm_ + input.clockBpm + 1U) / 2U);
  if (++followCandidateCount_ < 2) return output;

  output.setBpm = true;
  // A one-BPM ramp required two musical onsets per step.  A normal 20 BPM
  // song change could therefore take tens of seconds or minutes.  The
  // candidate was independently confirmed above, so change in one step and
  // let main.cpp re-anchor the visual beat clock to this onset.
  output.bpm = followCandidateBpm_;
  output.acquired = true;
  output.onsetAtMs = input.onsetAtMs;
  followCandidateBpm_ = 0;
  followCandidateCount_ = 0;
  return output;
}

}  // namespace decaflash::mainframe
