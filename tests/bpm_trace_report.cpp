#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "bpm_tracker.h"

namespace {

bool within(uint16_t observed, uint16_t reference, uint16_t tolerance) {
  return observed >= reference - tolerance && observed <= reference + tolerance;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: %s TRACE.csv EXPECTED_BPM\n", argv[0]);
    return 2;
  }

  const uint16_t expectedBpm = static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10));
  std::ifstream input(argv[1]);
  if (!input) {
    std::fprintf(stderr, "cannot read %s\n", argv[1]);
    return 2;
  }

  decaflash::mainframe::BpmTracker tracker;
  uint32_t timestampMs = 0;
  uint32_t level = 0;
  uint32_t percussiveLevel = 0;
  uint32_t firstTimestampMs = 0;
  uint32_t previousEvaluation = 0;
  uint32_t rows = 0;
  uint32_t evaluations = 0;
  uint32_t correct = 0;
  uint32_t halfTempo = 0;
  uint32_t other = 0;
  uint32_t unresolved = 0;
  char comma = 0;

  while (input >> timestampMs >> comma >> level >> comma >> percussiveLevel) {
    if (comma != ',') {
      std::fprintf(stderr, "invalid trace row in %s\n", argv[1]);
      return 2;
    }
    if (rows == 0) firstTimestampMs = timestampMs;
    ++rows;
    tracker.feed(timestampMs, level, percussiveLevel);
    const auto& estimate = tracker.estimate();
    if (estimate.analyzedFrames == previousEvaluation) continue;
    previousEvaluation = estimate.analyzedFrames;
    // The first four seconds only fill the real device's analysis history.
    if (timestampMs - firstTimestampMs < 4000U) continue;
    ++evaluations;
    if (estimate.bpm == 0U) {
      ++unresolved;
    } else if (within(estimate.bpm, expectedBpm, 3U)) {
      ++correct;
    } else if (within(estimate.bpm, static_cast<uint16_t>(expectedBpm / 2U), 3U)) {
      ++halfTempo;
    } else {
      ++other;
    }
  }

  if (rows == 0U || evaluations == 0U) {
    std::fprintf(stderr, "trace %s has insufficient data\n", argv[1]);
    return 2;
  }
  std::printf("%s expected=%u evaluations=%lu correct=%lu half=%lu other=%lu unresolved=%lu\n",
              argv[1], static_cast<unsigned>(expectedBpm),
              static_cast<unsigned long>(evaluations), static_cast<unsigned long>(correct),
              static_cast<unsigned long>(halfTempo), static_cast<unsigned long>(other),
              static_cast<unsigned long>(unresolved));
  return 0;
}
