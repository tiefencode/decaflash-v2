#pragma once
#include <cstdint>

namespace decaflash::mainframe {
enum class MotionKind : uint8_t { None, Move, Tap, MultiTap, Impact, Rotate, Tilt, Shake };
const char* motionName(MotionKind kind);
struct MotionEvent {
  MotionKind kind = MotionKind::None;
  uint32_t atMs = 0;
  uint32_t sequence = 0;
  uint8_t count = 0;
  float peakG = 0;
};
struct MotionSample { float ax, ay, az, gx, gy, gz; };
// Bounded heuristic classifier. Units: g and degrees/second. No sensor I/O.
class MotionEvents {
 public:
  MotionEvent feed(uint32_t now, const MotionSample& sample);
  const MotionEvent& latest() const { return latest_; }
 private:
  MotionEvent latest_;
  bool seeded_ = false, window_ = false, pulse_ = false, tilted_ = false;
  uint32_t lastAt_ = 0, startAt_ = 0, pulseAt_ = 0, pulseStart_ = 0, tiltSince_ = 0;
  uint32_t tiltReportAt_ = 0;
  float rx_ = 0, ry_ = 0, rz_ = 1, fx_ = 0, fy_ = 0, fz_ = 1;
  float peak_ = 0, lx_ = 0, ly_ = 0, lz_ = 0;
  uint16_t turnMs_ = 0, moveMs_ = 0;
  uint8_t taps_ = 0, reversals_ = 0;
  bool tiltCandidate_ = false, haveLobe_ = false;
  void clearWindow();
};
}  // namespace decaflash::mainframe
