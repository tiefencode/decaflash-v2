#include "motion_events.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
using namespace decaflash::mainframe;
const MotionSample rest = {0, 0, 1, 0, 0, 0};
MotionEvent trace(unsigned type, uint32_t base = 0) {
  MotionEvents detector;
  MotionEvent result;
  detector.feed(base, rest);
  for (unsigned t = 20; t <= 1000; t += 20) {
    auto sample = rest;
    if (type == 1 && t == 100) sample.ax = 0.5f;
    if (type == 2 && (t == 100 || t == 300)) sample.ax = 0.5f;
    if (type == 3 && t == 100) sample.ax = 2.0f;
    if (type == 4 && t >= 100 && t <= 600) sample.gz = 90;
    if (type == 5 && t >= 100 && t <= 600) sample.ax = (t / 100) % 2 ? 1.0f : -1.0f;
    if (type == 6 && t >= 100 && t <= 300) sample.ax = (t / 40) % 2 ? 0.4f : -0.4f;
    if (type == 7 && t >= 100 && t <= 600) sample.gy = 90;
    if (type == 8 && t >= 100 && t <= 400) sample.gz = 90;
    if (type == 9 && t >= 100 && t <= 300) { sample.ax = 0.7f; sample.gz = 90; }
    const auto event = detector.feed(base + t, sample);
    if (event.kind != MotionKind::None) result = event;
  }
  return result;
}
// Traces shaped like the capture from the wooden sphere
// (tests/fixtures/motion/2026-11-16-wood-sphere.log): a stroke rings with
// alternating direction and decays; `step` is the sample interval in ms.
struct Stroke { uint32_t atMs; float peakG; float decayMs; };
MotionEvent sphere(const Stroke* strokes, unsigned count, unsigned step,
                   unsigned* events = nullptr, uint32_t untilMs = 3000) {
  MotionEvents detector;
  MotionEvent first;
  unsigned seen = 0;
  detector.feed(0, rest);
  for (uint32_t t = step; t <= untilMs; t += step) {
    auto sample = rest;
    for (unsigned i = 0; i < count; ++i) {
      if (t < strokes[i].atMs) continue;
      const uint32_t age = t - strokes[i].atMs;
      const float level = strokes[i].peakG * std::exp(-static_cast<float>(age) / strokes[i].decayMs);
      sample.ax += (age / step) % 2 ? -level : level;
    }
    const auto event = detector.feed(t, sample);
    if (event.kind != MotionKind::None && !seen++) first = event;
  }
  if (events) *events = seen;
  return first;
}
void sphereCases() {
  for (unsigned step : {10u, 14u, 28u}) {
    unsigned events = 0;
    // Soft knock, about 120 mg: decided to stay silent.
    const Stroke soft[] = {{500, 0.12f, 60}};
    assert(sphere(soft, 1, step, &events).kind == MotionKind::None && events == 0);
    // Normal knock, about 400 mg, ringing for well under 100 ms.
    const Stroke knock[] = {{500, 0.40f, 30}};
    assert(sphere(knock, 1, step, &events).kind == MotionKind::Tap && events == 1);
    const Stroke firmKnock[] = {{500, 0.60f, 40}};
    assert(sphere(firmKnock, 1, step, &events).kind == MotionKind::Tap && events == 1);
    // Two knocks half a second apart.
    // Both starts fall on a sample for every step, so the 28 ms cadence
    // cannot step over the short second knock.
    const Stroke twice[] = {{504, 0.40f, 30}, {924, 0.40f, 30}};
    const auto doubleKnock = sphere(twice, 2, step, &events);
    assert(doubleKnock.kind == MotionKind::MultiTap && doubleKnock.count == 2 && events == 1);
    // Hits from 0.7 g to 2.5 g ring for 150-430 ms. The ringing must not turn
    // them into Move or Shake, and must not report a second event.
    const Stroke lightHit[] = {{500, 0.72f, 60}};
    assert(sphere(lightHit, 1, step, &events).kind == MotionKind::Impact && events == 1);
    const Stroke hit[] = {{500, 1.3f, 120}};
    assert(sphere(hit, 1, step, &events).kind == MotionKind::Impact && events == 1);
    const Stroke hardHit[] = {{500, 2.5f, 150}};
    assert(sphere(hardHit, 1, step, &events).kind == MotionKind::Impact && events == 1);
    // A knock that follows light handling late in the window still counts.
    const Stroke late[] = {{504, 0.25f, 40}, {1148, 0.40f, 30}};
    const auto lateKnock = sphere(late, 2, step, &events);
    assert(lateKnock.kind == MotionKind::MultiTap || lateKnock.kind == MotionKind::Tap);
    assert(events == 1);
  }
  // Seen on the device: the sphere is picked up and turned (rotation opens
  // the window), then shaken or hit 450 ms later. Shaking and hitting turn
  // it at 200-400 degrees/s, which must not be reported as Rotate.
  for (unsigned kind = 0; kind < 2; ++kind) {
    MotionEvents detector;
    MotionEvent first;
    unsigned events = 0;
    detector.feed(0, rest);
    for (unsigned t = 14; t <= 4000; t += 14) {
      auto sample = rest;
      if (t >= 500 && t < 950) sample.gz = 60;
      if (kind == 0 && t >= 950 && t < 1800) {
        sample.ax = ((t - 950) / 98) % 2 ? -1.5f : 1.5f;
        sample.gz = 350;
      }
      if (kind == 1 && t >= 950) {
        const unsigned age = t - 950;
        const float level = 1.5f * std::exp(-static_cast<float>(age) / 120.0f);
        sample.ax = (age / 14) % 2 ? -level : level;
        if (age < 300) sample.gz = 250;
      }
      const auto event = detector.feed(t, sample);
      if (event.kind != MotionKind::None && !events++) first = event;
    }
    assert(first.kind == (kind == 0 ? MotionKind::Shake : MotionKind::Impact));
  }
  // Sustained strong motion with a peak like a hit is not an Impact.
  MotionEvents shove;
  shove.feed(0, rest);
  for (unsigned t = 10; t <= 3000; t += 10) {
    auto sample = rest;
    if (t >= 500 && t <= 1700) sample.ax = (t / 150) % 2 ? 1.2f : -1.2f;
    assert(shove.feed(t, sample).kind != MotionKind::Impact);
  }
}
int main() {
  sphereCases();
  assert(trace(0).kind == MotionKind::None);
  assert(trace(1).kind == MotionKind::Tap);
  const auto multi = trace(2);
  assert(multi.kind == MotionKind::MultiTap && multi.count == 2);
  assert(trace(3).kind == MotionKind::Impact);
  assert(trace(4).kind == MotionKind::Rotate);
  assert(trace(5).kind == MotionKind::Shake);
  assert(trace(6).kind == MotionKind::Move); // lifting must not look like a tap
  assert(trace(7).kind == MotionKind::Rotate); // forward pitch is also Rotate
  assert(trace(8).kind == MotionKind::None); // under 35 degrees is not Rotate
  MotionEvents linearMotion;
  linearMotion.feed(0, rest);
  for (unsigned t = 20; t <= 2000; t += 20) {
    auto sample = rest;
    if (t >= 100 && t <= 300) { sample.ax = 0.7f; sample.gz = 90; }
    const auto event = linearMotion.feed(t, sample);
    assert(event.kind != MotionKind::Rotate && event.kind != MotionKind::Tilt);
  }
  MotionEvents rapidShake;
  rapidShake.feed(0, rest);
  uint32_t previousEventAt = 0;
  unsigned shakeEvents = 0;
  for (unsigned t = 20; t <= 5000; t += 20) {
    auto sample = rest;
    if (t >= 100) sample.ax = (t / 100) % 2 ? 1.0f : -1.0f;
    const auto event = rapidShake.feed(t, sample);
    if (event.kind != MotionKind::None) {
      if (shakeEvents++) assert(event.atMs - previousEventAt >= 1000);
      previousEventAt = event.atMs;
    }
  }
  assert(shakeEvents >= 3);
  assert(trace(2, 0xffffff00U).kind == MotionKind::MultiTap);
  MotionEvents tilt;
  tilt.feed(0, rest);
  bool seenTilt = false;
  unsigned tiltReports = 0;
  for (unsigned t = 20; t <= 6000; t += 20) {
    // Smoothly tilt 45 degrees then keep holding, no sustained gyro motion.
    const float angle = std::fmin(1.0f, t / 1000.0f) * 0.785398f;
    const MotionSample sample = {std::sin(angle), 0, std::cos(angle), 0, 0, 0};
    const auto event = tilt.feed(t, sample);
    if (event.kind == MotionKind::Tilt) { seenTilt = true; ++tiltReports; }
    assert(event.kind != MotionKind::Impact && event.kind != MotionKind::Shake);
  }
  assert(seenTilt && tiltReports >= 2 && tiltReports <= 3);
  assert(tilt.latest().sequence >= tiltReports);
  MotionEvents gap;
  gap.feed(0, rest);
  auto hit = rest; hit.ax = 0.5f;
  gap.feed(20, hit);
  assert(gap.feed(1000, rest).kind == MotionKind::None);
  for (unsigned t = 1020; t <= 2000; t += 20) assert(gap.feed(t, rest).kind == MotionKind::None);
  auto bad = rest; bad.ax = NAN;
  assert(gap.feed(2020, bad).kind == MotionKind::None);
  puts("PASS: tap/multitap, impact, rotation, held tilt, shake, gaps and wrap");
}
