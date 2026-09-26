#include "motion_events.h"
#include <cassert>
#include <cmath>
#include <cstdio>
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
    const auto event = detector.feed(base + t, sample);
    if (event.kind != MotionKind::None) result = event;
  }
  return result;
}
int main() {
  assert(trace(0).kind == MotionKind::None);
  assert(trace(1).kind == MotionKind::Tap);
  const auto multi = trace(2);
  assert(multi.kind == MotionKind::MultiTap && multi.count == 2);
  assert(trace(3).kind == MotionKind::Impact);
  assert(trace(4).kind == MotionKind::Rotate);
  assert(trace(5).kind == MotionKind::Shake);
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
