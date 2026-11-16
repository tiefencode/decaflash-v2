#pragma once
#include <cstdint>

namespace decaflash::mainframe {
// Rotate: tipped over by 60 degrees, or spun a full turn (dizzy).
// Reposition: left looking in another direction than the learned one.
enum class MotionKind : uint8_t { None, Move, Tap, MultiTap, Impact, Rotate, Reposition, Shake };
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
  bool seeded_ = false, window_ = false, burst_ = false, tipped_ = false;
  bool impact_ = false;
  uint32_t lastAt_ = 0, startAt_ = 0, burstStart_ = 0, burstLastAt_ = 0, tipSince_ = 0;
  uint32_t lastEventAt_ = 0;
  uint32_t repositionReportAt_ = 0, yawAt_ = 0;
  float rx_ = 0, ry_ = 0, rz_ = 1, fx_ = 0, fy_ = 0, fz_ = 1;
  float peak_ = 0, burstPeak_ = 0, lx_ = 0, ly_ = 0, lz_ = 0;
  uint16_t moveMs_ = 0, strongMs_ = 0;
  float turnDegrees_ = 0;
  float heading_ = 0, spin_ = 0;  // degrees around the vertical
  uint8_t taps_ = 0, reversals_ = 0;
  bool tipCandidate_ = false, repositioned_ = false, haveLobe_ = false, haveEvent_ = false;
  void clearWindow();
  void closeBurst();
  bool canReport(uint32_t now) const;
  MotionEvent report(MotionKind kind, uint32_t now, uint8_t count, float peakG);
};
}  // namespace decaflash::mainframe
