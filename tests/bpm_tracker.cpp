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
    tracker.feed(now, level, level);
  }
}

void assertLocks(uint16_t bpm) {
  decaflash::mainframe::BpmTracker tracker;
  feedPulseTrain(tracker, bpm, 0U, 9000U);
  const auto& estimate = tracker.estimate();
  assert(estimate.bpm >= bpm - 3U && estimate.bpm <= bpm + 3U);
  assert(estimate.confidence >= 45U);
}

void assertSilenceIsSafe() {
  decaflash::mainframe::BpmTracker tracker;
  for (uint32_t now = 0; now < 5000U; now += 16U) {
    tracker.feed(now, 0U, 0U);
  }
  const auto& estimate = tracker.estimate();
  assert(estimate.bpm == 0U);
  assert(estimate.confidence == 0U);
}

}  // namespace

int main() {
  assertLocks(100);
  assertLocks(120);
  assertLocks(160);
  assertLocks(180);
  assertSilenceIsSafe();

  std::puts("PASS: BPM tracker estimates reference pulses and safely rejects silence");
}
