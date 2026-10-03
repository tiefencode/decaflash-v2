#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "bpm_tracker.h"

namespace {

enum Field : uint8_t {
  kLevel = 0,
  kPercussive = 1,
  kBass = 2,
  kLowMid = 3,
  kMid = 4,
  kHigh = 5,
};

struct Frame {
  uint32_t timestampMs = 0;
  std::array<uint32_t, 6> values{};
};

struct Mode {
  const char* name;
  Field level;
  Field percussive;
};

constexpr std::array<Mode, 7> kModes{{
  {"current", kLevel, kPercussive},
  {"bass", kBass, kBass},
  {"lowmid", kLowMid, kLowMid},
  {"mid", kMid, kMid},
  {"high", kHigh, kHigh},
  {"bass-high", kBass, kHigh},
  {"bass-percussive", kBass, kPercussive},
}};

bool within(uint16_t observed, uint16_t reference) {
  return observed >= reference - 3U && observed <= reference + 3U;
}

void report(const std::vector<Frame>& frames, const Mode& mode, uint16_t expected) {
  decaflash::mainframe::BpmTracker tracker;
  const uint32_t firstTimestampMs = frames.front().timestampMs;
  uint32_t previousEvaluation = 0;
  uint32_t correct = 0;
  uint32_t half = 0;
  uint32_t other = 0;
  uint32_t evaluations = 0;
  for (const Frame& frame : frames) {
    tracker.feed(frame.timestampMs, frame.values[mode.level], frame.values[mode.percussive]);
    const auto& estimate = tracker.estimate();
    if (estimate.analyzedFrames == previousEvaluation) continue;
    previousEvaluation = estimate.analyzedFrames;
    if (frame.timestampMs - firstTimestampMs < 4000U) continue;
    ++evaluations;
    if (within(estimate.bpm, expected)) {
      ++correct;
    } else if (within(estimate.bpm, static_cast<uint16_t>(expected / 2U))) {
      ++half;
    } else {
      ++other;
    }
  }
  std::printf("%-16s correct=%lu half=%lu other=%lu evaluations=%lu\n", mode.name,
              static_cast<unsigned long>(correct), static_cast<unsigned long>(half),
              static_cast<unsigned long>(other), static_cast<unsigned long>(evaluations));
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const uint16_t expected = static_cast<uint16_t>(std::strtoul(argv[2], nullptr, 10));
  std::ifstream input(argv[1]);
  if (!input) return 2;
  std::vector<Frame> frames;
  char comma = 0;
  Frame frame;
  while (input >> frame.timestampMs >> comma >> frame.values[kLevel] >> comma >>
         frame.values[kPercussive] >> comma >> frame.values[kBass] >> comma >>
         frame.values[kLowMid] >> comma >> frame.values[kMid] >> comma >>
         frame.values[kHigh]) {
    frames.push_back(frame);
  }
  if (frames.empty()) return 2;
  std::printf("%s expected=%u frames=%lu\n", argv[1], static_cast<unsigned>(expected),
              static_cast<unsigned long>(frames.size()));
  for (const Mode& mode : kModes) report(frames, mode, expected);
  return 0;
}
