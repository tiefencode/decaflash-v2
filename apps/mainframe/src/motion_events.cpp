#include "motion_events.h"
#include <algorithm>
#include <cmath>
namespace decaflash::mainframe {
namespace {
float norm(float x, float y, float z) { return std::sqrt(x*x + y*y + z*z); }
}
const char* motionName(MotionKind kind) {
  switch (kind) {
    case MotionKind::Move: return "move";
    case MotionKind::Tap: return "tap";
    case MotionKind::MultiTap: return "multi-tap";
    case MotionKind::Impact: return "impact";
    case MotionKind::Rotate: return "rotate";
    case MotionKind::Tilt: return "tilt";
    case MotionKind::Shake: return "shake";
    case MotionKind::None: return "--";
  }
  return "--";
}
bool MotionEvents::canReport(uint32_t now) const {
  return !haveEvent_ || now - lastEventAt_ >= 1000;
}
void MotionEvents::clearWindow() {
  window_ = false; pulse_ = false; haveLobe_ = false;
  peak_ = 0; taps_ = 0; reversals_ = 0; turnDegrees_ = 0; moveMs_ = 0;
}
MotionEvent MotionEvents::feed(uint32_t now, const MotionSample& s) {
  MotionEvent none;
  if (!std::isfinite(s.ax) || !std::isfinite(s.ay) || !std::isfinite(s.az) ||
      !std::isfinite(s.gx) || !std::isfinite(s.gy) || !std::isfinite(s.gz)) {
    clearWindow(); tiltCandidate_ = false; return none;
  }
  const float a = norm(s.ax, s.ay, s.az), rotation = norm(s.gx, s.gy, s.gz);
  if (!seeded_) {
    if (a < 0.85f || a > 1.15f || rotation > 15) return none;
    rx_ = fx_ = s.ax; ry_ = fy_ = s.ay; rz_ = fz_ = s.az;
    lastAt_ = now; seeded_ = true; return none;
  }
  const uint32_t elapsed = now - lastAt_; lastAt_ = now;
  if (elapsed > 150) { clearWindow(); tiltCandidate_ = false; return none; }
  // Slow gravity estimate: reject individual impulses for pose estimation.
  const float dx = s.ax - fx_, dy = s.ay - fy_, dz = s.az - fz_;
  const float dynamic = norm(dx, dy, dz);
  const float alpha = static_cast<float>(elapsed) / (250.0f + elapsed);
  fx_ += alpha * dx; fy_ += alpha * dy; fz_ += alpha * dz;
  // Pose must come from the current, quiet gravity vector. Using the dynamic
  // low-pass estimate here lets a strong linear movement masquerade as Tilt.
  const float poseDenom = a * norm(rx_, ry_, rz_);
  const float poseCosine = poseDenom > 0.1f
    ? (s.ax * rx_ + s.ay * ry_ + s.az * rz_) / poseDenom : 1;
  const bool poseStable = std::fabs(a - 1) < 0.12f && rotation < 15;
  if (poseCosine > 0.966f) { tilted_ = false; tiltCandidate_ = false; }
  if (poseStable && poseCosine < 0.906f) {
    if (!tiltCandidate_) { tiltCandidate_ = true; tiltSince_ = now; }
  } else { tiltCandidate_ = false; }

  if (!window_ && (dynamic >= 0.30f || rotation >= 45)) {
    clearWindow(); window_ = true; startAt_ = now;
  }
  if (window_) {
    peak_ = std::max(peak_, dynamic);
    // Gyro motion counts as Rotate only while the accelerometer is quiet.
    // Linear handling movement can otherwise create a false angular event.
    if (rotation >= 45 && std::fabs(a - 1) < 0.12f && dynamic < 0.30f) {
      turnDegrees_ += rotation * elapsed / 1000.0f;
    }
    if (dynamic >= 0.30f) moveMs_ += elapsed;
    // A deliberate knock must exceed 0.45 g above the gravity estimate.
    // The lower release threshold and 100 ms refractory interval keep ringing
    // from counting as several taps.
    if (!pulse_ && dynamic >= 0.45f) {
      pulse_ = true; pulseStart_ = now;
    } else if (pulse_ && dynamic < 0.20f) {
      pulse_ = false;
      if (now - pulseStart_ <= 120 && (!taps_ || pulseStart_ - pulseAt_ >= 100)) {
        if (taps_ < 255) ++taps_;
        pulseAt_ = pulseStart_;
      }
    }
    // Shake requires alternating substantial acceleration vectors, not just peaks.
    if (dynamic >= 0.55f) {
      if (haveLobe_ && dx*lx_ + dy*ly_ + dz*lz_ < -0.15f) {
        if (reversals_ < 255) ++reversals_;
        lx_ = dx; ly_ = dy; lz_ = dz;
      } else if (!haveLobe_) { haveLobe_ = true; lx_ = dx; ly_ = dy; lz_ = dz; }
    }
    if (now - startAt_ >= 700) {
      MotionKind kind = MotionKind::None;
      if (reversals_ >= 3) kind = MotionKind::Shake;
      else if (peak_ >= 1.8f) kind = MotionKind::Impact;
      else if (turnDegrees_ >= 25.0f) kind = MotionKind::Rotate;
      else if (taps_ >= 2) kind = MotionKind::MultiTap;
      else if (taps_ == 1 && moveMs_ <= 120) kind = MotionKind::Tap;
      else if (moveMs_ >= 150) kind = MotionKind::Move;
      const uint8_t count = taps_; const float peak = peak_;
      clearWindow();
      if (kind != MotionKind::None && canReport(now)) {
        latest_.kind = kind; latest_.atMs = now; ++latest_.sequence;
        latest_.count = count; latest_.peakG = peak;
        lastEventAt_ = now; haveEvent_ = true;
        return latest_;
      } else if (kind != MotionKind::None) {
        return none;
      }
    }
  }
  if (!window_ && tiltCandidate_ && now - tiltSince_ >= 500 &&
      (!tilted_ || now - tiltReportAt_ >= 2000) && canReport(now)) {
    tilted_ = true; tiltReportAt_ = now;
    latest_.kind = MotionKind::Tilt; latest_.atMs = now; ++latest_.sequence;
    latest_.count = 1; latest_.peakG = 0;
    lastEventAt_ = now; haveEvent_ = true;
    return latest_;
  }
  return none;
}
}  // namespace decaflash::mainframe
