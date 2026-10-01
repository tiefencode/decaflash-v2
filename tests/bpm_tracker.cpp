#include <cassert>
#include <cstdio>

#include "bpm_tracker.h"

namespace {

void feedPulseTrain(decaflash::mainframe::BpmTracker& tracker, uint16_t bpm,
                    uint32_t startMs, uint32_t durationMs) {
  const uint32_t periodMs = 60000UL / bpm;
  for (uint32_t now = startMs; now < startMs + durationMs; now += 16U) {
    const uint32_t phase = now % periodMs;
    const uint32_t level = phase < 32U ? 1800U : 500U;
    tracker.feed(now, level);
  }
}

void assertLocks(uint16_t bpm) {
  decaflash::mainframe::BpmTracker tracker;
  feedPulseTrain(tracker, bpm, 0U, 9000U);
  const auto& estimate = tracker.estimate();
  assert(estimate.bpm >= bpm - 3U && estimate.bpm <= bpm + 3U);
  assert(estimate.confidence >= 45U);
}

}  // namespace

int main() {
  assertLocks(100);
  assertLocks(120);
  assertLocks(160);
  assertLocks(180);

  decaflash::mainframe::BpmTracker tracker;
  feedPulseTrain(tracker, 120, 0U, 7000U);
  assert(tracker.estimate().bpm >= 117U && tracker.estimate().bpm <= 123U);
  feedPulseTrain(tracker, 100, 7000U, 6000U);
  assert(tracker.estimate().bpm >= 97U && tracker.estimate().bpm <= 103U);

  std::puts("PASS: BPM tracker locks 100/120/160/180 and changes 120->100");
}
