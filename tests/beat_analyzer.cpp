#include <cassert>
#include <cstdio>

#include "beat_analyzer.h"

int main() {
  decaflash::mainframe::BeatAnalyzer analyzer;
  for (uint32_t now = 0; now < 8256; now += 64) {
    const bool pulse = ((now + 128) % 512) < 128;
    analyzer.feed(now, pulse ? 1000 : 100, pulse ? 1200 : 140);
  }

  assert(analyzer.musicPresent());
  assert(analyzer.detectedBpm() >= 114 && analyzer.detectedBpm() <= 122);
  assert(analyzer.clockBpm() >= 114 && analyzer.clockBpm() <= 122);
  assert(analyzer.confidence() >= 50);
  std::puts("PASS: V1-derived onset history resolves a steady 117 BPM pulse train");
}
