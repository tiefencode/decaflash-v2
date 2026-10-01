#include <cassert>
#include <cstdio>

#include "audio_follower.h"

int main() {
  decaflash::mainframe::AudioFollower follower;
  decaflash::mainframe::AudioFollowInput input = {};
  input.showRunning = true;
  input.musicPresent = true;
  input.clockBpm = 118;
  input.confidence = 80;
  input.currentBpm = 120;

  decaflash::mainframe::AudioFollowOutput output = {};
  for (uint32_t onset = 500; onset <= 1500; onset += 500) {
    input.nowMs = onset;
    input.onsetAtMs = onset;
    output = follower.update(input);
  }
  assert(output.setBpm && output.acquired && output.bpm == 118);

  // A high-tempo track must be able to control the show.  The previous 170
  // BPM ceiling silently dropped a valid 180 BPM estimate.
  decaflash::mainframe::AudioFollower highTempoFollower;
  input.clockBpm = 180;
  input.currentBpm = 120;
  for (uint32_t onset = 333; onset <= 999; onset += 333) {
    input.nowMs = onset;
    input.onsetAtMs = onset;
    output = highTempoFollower.update(input);
  }
  assert(output.setBpm && output.acquired && output.bpm == 180);

  // Once locked, a confirmed song change is a re-sync event, not a
  // one-BPM-at-a-time countdown.  Two agreeing onsets at 100 BPM are enough.
  decaflash::mainframe::AudioFollower switchFollower;
  input.clockBpm = 120;
  input.currentBpm = 120;
  for (uint32_t onset = 500; onset <= 1500; onset += 500) {
    input.nowMs = onset;
    input.onsetAtMs = onset;
    output = switchFollower.update(input);
  }
  assert(output.setBpm && output.acquired && output.bpm == 120);

  input.clockBpm = 100;
  input.currentBpm = 120;
  input.nowMs = 2100;
  input.onsetAtMs = 2100;
  output = switchFollower.update(input);
  assert(!output.setBpm);
  input.nowMs = 2700;
  input.onsetAtMs = 2700;
  output = switchFollower.update(input);
  assert(output.setBpm && output.acquired && output.bpm == 100);

  // A value that has not reached the analyzer's locked confidence is still
  // ignored, so this does not trade responsiveness for arbitrary noise.
  decaflash::mainframe::AudioFollower uncertainFollower;
  input.clockBpm = 140;
  input.confidence = 61;
  input.currentBpm = 120;
  input.nowMs = 500;
  input.onsetAtMs = 500;
  output = uncertainFollower.update(input);
  assert(!output.setBpm);

  std::puts("PASS: follower locks confident tempos and re-syncs confirmed changes directly");
}
