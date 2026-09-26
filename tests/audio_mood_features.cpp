#include "audio_mood_features.h"
#include <cassert>
#include <cmath>
#include <cstdio>
using namespace decaflash::mainframe;
void tone(AudioMoodFeatures& f, float frequency, float amplitude, unsigned blocks,
          uint32_t start = 0, float dc = 0) {
  int16_t samples[256];
  for (unsigned b = 0; b < blocks; ++b) {
    for (unsigned i = 0; i < 256; ++i)
      samples[i] = static_cast<int16_t>(dc + amplitude * std::sin(
        6.28318530718 * frequency * (b * 256 + i) / 16000));
    f.feed(start + b * 16, samples, 256);
  }
}
int main() {
  AudioMoodFeatures bass, treble, soft;
  tone(bass, 100, 5000, 1000);
  tone(treble, 1000, 5000, 1000);
  tone(soft, 100, 1000, 1000);
  assert(bass.bassValid() && treble.bassValid());
  assert(bass.bassPermille() > 250 && treble.bassPermille() < 80);
  assert(std::abs(int(bass.bassPermille()) - int(soft.bassPermille())) <= 2);
  AudioMoodFeatures warmup;
  tone(warmup, 100, 5000, 500);
  assert(!warmup.bassValid()); // insufficient evidence
  tone(bass, 100, 0, 100, 16000);
  assert(bass.silent() && !bass.bassValid());
  tone(bass, 100, 5000, 1, 18000);
  assert(!bass.silent() && !bass.bassValid()); // resume does not reuse old estimate
  tone(treble, 1000, 5000, 1, 30000);
  assert(!treble.bassValid()); // missing capture invalidates accumulated bass evidence
  AudioMoodFeatures dc;
  tone(dc, 0, 0, 1000, 0, 10000);
  assert(dc.silent() && !dc.bassValid());
  AudioMoodFeatures clipping;
  int16_t clipped[256];
  for (auto& s : clipped) s = 32767;
  clipping.feed(0, clipped, 256);
  assert(!clipping.bassValid());
  AudioMoodFeatures wrapped;
  tone(wrapped, 100, 5000, 1000, 0xffffff00U);
  assert(wrapped.bassValid());
  puts("PASS: bass separation/amplitude independence, warmup, silence, gaps, DC and clipping");
}
