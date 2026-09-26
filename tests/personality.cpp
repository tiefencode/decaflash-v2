#include "personality.h"
#include <cassert>
#include <cstdio>
using namespace decaflash::mainframe;
MoodAudio music(uint32_t now, uint16_t bpm, uint16_t bass = 300) {
  MoodAudio a;
  a.fresh = true; a.bpm = bpm; a.confidence = 80;
  a.onsetAtMs = now - now % 500; a.bassValid = true; a.bassPermille = bass;
  return a;
}
int main() {
  for (unsigned bpm = 80; bpm <= 160; bpm += 20) {
    Personality p;
    for (unsigned t = 0; t <= 40000; t += 100) p.update(t, music(t, bpm));
    const int base = bpm <= 120 ? (bpm - 60) * 2000 / 3 :
      bpm <= 140 ? 40000 + (bpm - 120) * 1000 : 60000 + (bpm - 140) * 1500;
    const int expected = (base + (80 - 68) * 10000 / 32) / 1000;
    assert(p.snapshot().energy == expected);
  }
  Personality unknown;
  MoodAudio missing;
  for (unsigned t = 0; t <= 600000; t += 100) unknown.update(t, missing);
  assert(unknown.snapshot().energy == 30 && unknown.snapshot().loneliness == 100);
  Personality uncertain;
  for (unsigned t = 0; t <= 30000; t += 100) {
    auto a = music(t, 160); a.confidence = 20; uncertain.update(t, a);
  }
  assert(uncertain.snapshot().energy == 90); // BPM drives the base without a beat bonus
  Personality sameOnset;
  auto held = music(0, 160);
  for (unsigned t = 0; t <= 1000; t += 100) sameOnset.update(t, held);
  assert(sameOnset.snapshot().energy == 34); // BPM base ramps without onset history
  Personality tempoDrop;
  for (unsigned t = 0; t <= 40000; t += 100) tempoDrop.update(t, music(t, 160));
  for (unsigned t = 40100; t <= 45000; t += 100) tempoDrop.update(t, music(t, 115));
  assert(tempoDrop.snapshot().energy <= 45); // accepted slower tempo releases energy quickly
  Personality p;
  for (unsigned t = 0; t <= 40000; t += 100) p.update(t, music(t, 160));
  MoodAudio silence; silence.fresh = true; silence.silent = true;
  for (unsigned t = 40100; t <= 41100; t += 100) p.update(t, silence);
  assert(p.snapshot().energy == 93); // one second before decline
  for (unsigned t = 41200; t <= 45800; t += 100) p.update(t, silence);
  assert(p.snapshot().energy == 0); // confirmed silence reaches zero
  for (unsigned t = 45900; t <= 48900; t += 100) p.update(t, music(t, 160));
  assert(p.snapshot().energy >= 0);
  const auto before = p.snapshot().energy;
  for (unsigned t = 49000; t <= 51000; t += 100) p.update(t, missing);
  assert(p.snapshot().energy == before); // device failure never means silence
  Personality slow, fast, noBass;
  for (unsigned t = 0; t <= 400000; t += 100) {
    slow.update(t, music(t, 80)); fast.update(t, music(t, 160));
    noBass.update(t, music(t, 160, 0));
  }
  assert(slow.snapshot().depression > fast.snapshot().depression);
  assert(noBass.snapshot().depression > fast.snapshot().depression);
  assert(slow.snapshot().loneliness == fast.snapshot().loneliness);
  MotionEvent event; event.kind = MotionKind::Tap;
  const auto energyBeforeInteraction = unknown.snapshot().energy;
  unknown.onMotion(event);
  assert(unknown.snapshot().energy == energyBeforeInteraction);
  assert(unknown.snapshot().loneliness == 92);
  event.kind = MotionKind::Impact;
  for (unsigned i = 0; i < 50; ++i) unknown.onMotion(event);
  assert(unknown.snapshot().loneliness == 0 && unknown.snapshot().annoyance == 100);
  Personality annoyed;
  annoyed.update(0, missing);
  annoyed.onMotion(event); // impact: +15
  for (unsigned t = 100; t <= 10000; t += 100) annoyed.update(t, missing);
  assert(annoyed.snapshot().annoyance == 14);
  for (unsigned t = 10100; t <= 150000; t += 100) annoyed.update(t, missing);
  assert(annoyed.snapshot().annoyance == 0);
  Personality wrap;
  const uint32_t base = 0xffffff00U;
  wrap.update(base, silence);
  for (unsigned t = 100; t <= 2000; t += 100) wrap.update(base + t, silence);
  assert(wrap.snapshot().energy == 10);
  Personality fine, coarse;
  for (unsigned t = 0; t <= 30000; ++t) fine.update(t, silence);
  for (unsigned t = 0; t <= 30000; t += 100) coarse.update(t, silence);
  assert(fine.snapshot().energy == coarse.snapshot().energy);
  assert(fine.snapshot().depression == coarse.snapshot().depression);
  assert(fine.snapshot().loneliness == coarse.snapshot().loneliness);
  puts("PASS: BPM mapping/confidence, silence/recovery, unknown audio, bass/depression, independent loneliness");
}
