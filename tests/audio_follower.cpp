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
  std::puts("PASS: audio follower waits for three confident matching onsets");
}
