// Replays device captures (tests/fixtures/motion/) through the classifier.
// A capture holds only the magnitude of the dynamic acceleration per sample,
// so the replay cannot cover Shake, Rotate or Reposition: it checks knocks,
// hits and gentle moving. The magnitude is fed with alternating sign, which
// keeps the classifier's gravity estimate where it was on the device.
#include "motion_events.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>
using namespace decaflash::mainframe;

struct Window { uint32_t startMs = 0; unsigned samples = 0; std::vector<unsigned> raw; };
struct Counts { unsigned tap = 0, multiTap = 0, impact = 0, move = 0, other = 0; };

std::vector<std::pair<uint32_t, unsigned>> loadTimeline(const std::string& path) {
  FILE* file = std::fopen(path.c_str(), "r");
  if (!file) { std::fprintf(stderr, "cannot open %s\n", path.c_str()); std::exit(1); }
  std::map<unsigned, Window> windows;
  char line[512];
  while (std::fgets(line, sizeof(line), file)) {
    unsigned n = 0, first = 0, samples = 0;
    unsigned long startMs = 0;
    int used = 0;
    if (std::sscanf(line, "MOTION_WIN n=%u t=%lu", &n, &startMs) == 2) {
      const char* field = std::strstr(line, "samples=");
      if (!field || std::sscanf(field, "samples=%u", &samples) != 1 || !samples) continue;
      windows[n].startMs = static_cast<uint32_t>(startMs);
      windows[n].samples = samples;
    } else if (std::sscanf(line, "MOTION_RAW n=%u i=%u%n", &n, &first, &used) == 2) {
      const char* cursor = line + used;
      unsigned value = 0;
      int step = 0;
      while (std::sscanf(cursor, " %u%n", &value, &step) == 1) {
        windows[n].raw.push_back(value);
        cursor += step;
      }
    }
  }
  std::fclose(file);
  std::vector<std::pair<uint32_t, unsigned>> timeline;
  for (const auto& entry : windows) {
    const Window& w = entry.second;
    if (!w.samples) continue;
    for (size_t i = 0; i < w.raw.size(); ++i) {
      timeline.push_back({w.startMs + static_cast<uint32_t>(i * 720 / w.samples), w.raw[i]});
    }
  }
  std::sort(timeline.begin(), timeline.end());
  return timeline;
}

// Classifies the part of a capture between two device times in seconds.
Counts replay(const std::vector<std::pair<uint32_t, unsigned>>& timeline,
              uint32_t fromS, uint32_t toS) {
  MotionEvents detector;
  Counts counts;
  const MotionSample rest = {0, 0, 1, 0, 0, 0};
  uint32_t now = fromS * 1000;
  bool positive = true;
  detector.feed(now, rest);
  const auto count = [&](const MotionSample& sample) {
    switch (detector.feed(now, sample).kind) {
      case MotionKind::None: break;
      case MotionKind::Tap: ++counts.tap; break;
      case MotionKind::MultiTap: ++counts.multiTap; break;
      case MotionKind::Impact: ++counts.impact; break;
      case MotionKind::Move: ++counts.move; break;
      default: ++counts.other; break;
    }
  };
  const auto feed = [&](uint32_t at, unsigned mg) {
    if (at <= now) at = now + 1;
    // The capture has no samples while the sphere was at rest.
    while (at - now > 20) { now += 10; count(rest); }
    now = at;
    auto sample = rest;
    sample.ax = (positive ? 1.0f : -1.0f) * static_cast<float>(mg) / 1000.0f;
    positive = !positive;
    count(sample);
  };
  for (const auto& sample : timeline) {
    if (sample.first < fromS * 1000 || sample.first > toS * 1000) continue;
    feed(sample.first, sample.second);
  }
  feed(now + 2000, 0);
  return counts;
}

int main(int argc, char** argv) {
  assert(argc == 2);
  const std::string dir = argv[1];
  const auto sphere = loadTimeline(dir + "/2026-11-16-wood-sphere.txt");
  const auto moving = loadTimeline(dir + "/2026-11-16-move-and-knock.txt");
  assert(sphere.size() > 1000 && moving.size() > 1000);

  // 15 normal knocks, about 2 s apart.
  const Counts knocks = replay(sphere, 195, 227);
  std::printf("knocks: tap=%u multi=%u impact=%u move=%u\n",
              knocks.tap, knocks.multiTap, knocks.impact, knocks.move);
  assert(knocks.tap >= 14 && knocks.multiTap <= 1 && knocks.impact == 0 && knocks.move == 0);

  // 14 hits.
  const Counts hits = replay(sphere, 256, 290);
  std::printf("hits: tap=%u multi=%u impact=%u move=%u\n",
              hits.tap, hits.multiTap, hits.impact, hits.move);
  assert(hits.impact >= 13 && hits.tap + hits.multiTap <= 1);

  // 35 s of moving the sphere gently by hand: the device reported three Taps.
  const Counts move = replay(moving, 316, 353);
  std::printf("moving: tap=%u multi=%u impact=%u move=%u\n",
              move.tap, move.multiTap, move.impact, move.move);
  assert(move.tap == 0 && move.multiTap == 0 && move.impact == 0);

  // Seven softer knocks in the same capture; the device reported five.
  const Counts later = replay(moving, 370, 390);
  std::printf("later knocks: tap=%u multi=%u impact=%u move=%u\n",
              later.tap, later.multiTap, later.impact, later.move);
  assert(later.tap >= 4 && later.multiTap == 0 && later.impact == 0);
  std::puts("PASS: captured knocks, hits and gentle moving");
  return 0;
}
