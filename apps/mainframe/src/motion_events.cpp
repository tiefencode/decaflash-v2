#include "motion_events.h"
#include <algorithm>
#include <cmath>
namespace decaflash::mainframe {
namespace {
float norm(float x, float y, float z) { return std::sqrt(x*x + y*y + z*z); }

// Knock and hit levels for the Mainframe mounted in the 3 kg wooden sphere,
// taken from tests/fixtures/motion/2026-11-16-wood-sphere.log. Rest stays
// below 0.03 g there, normal knocks peak at 0.23-0.60 g and ring for up to
// 100 ms, hits peak at 0.69-2.6 g and ring for 160-430 ms. The rest level at
// show volume is not measured; raise kKnockG if loud bass triggers knocks.
constexpr float kKnockG = 0.20f;          // a burst starts and continues here
constexpr float kImpactG = 0.65f;         // burst peak that makes it a hit
constexpr uint32_t kBurstQuietMs = 100;   // this long below kKnockG ends a burst
constexpr uint32_t kKnockMaxMs = 130;     // longest burst that counts as a knock
constexpr uint32_t kImpactMaxMs = 500;    // longer strong bursts are shake or roll
constexpr float kStrongG = 0.45f;
constexpr uint16_t kShakeStrongMs = 350;  // hit ringing stays below this, a shake does not
constexpr uint32_t kWindowMs = 700;
constexpr uint32_t kWindowMaxMs = 1100;   // a burst still running may extend the window
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
  window_ = false; burst_ = false; impact_ = false; haveLobe_ = false;
  peak_ = 0; taps_ = 0; reversals_ = 0; turnDegrees_ = 0; moveMs_ = 0; strongMs_ = 0;
}
// A burst is one stretch of activity including its ringing. Its peak and
// length decide between knock and hit; anything longer is neither.
void MotionEvents::closeBurst() {
  burst_ = false;
  const uint32_t length = burstLastAt_ - burstStart_;
  if (burstPeak_ >= kImpactG) {
    if (length <= kImpactMaxMs) impact_ = true;
  } else if (length <= kKnockMaxMs && taps_ < 255) {
    ++taps_;
  }
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

  if (!window_ && (dynamic >= kKnockG || rotation >= 45)) {
    clearWindow(); window_ = true; startAt_ = now;
  }
  if (window_) {
    peak_ = std::max(peak_, dynamic);
    // Keep integrating deliberate gyro motion while the device is carried.
    // The 35-degree threshold rejects brief gyro spikes from linear handling.
    if (rotation >= 45) {
      turnDegrees_ += rotation * elapsed / 1000.0f;
    }
    if (dynamic >= 0.30f) moveMs_ += elapsed;
    if (dynamic >= kStrongG) strongMs_ += elapsed;
    if (dynamic >= kKnockG) {
      if (!burst_) { burst_ = true; burstStart_ = now; burstPeak_ = 0; }
      burstLastAt_ = now;
      burstPeak_ = std::max(burstPeak_, dynamic);
    } else if (burst_ && now - burstLastAt_ >= kBurstQuietMs) {
      closeBurst();
    }
    // Shake requires alternating substantial acceleration vectors, not just peaks.
    if (dynamic >= 0.55f) {
      if (haveLobe_ && dx*lx_ + dy*ly_ + dz*lz_ < -0.15f) {
        if (reversals_ < 255) ++reversals_;
        lx_ = dx; ly_ = dy; lz_ = dz;
      } else if (!haveLobe_) { haveLobe_ = true; lx_ = dx; ly_ = dy; lz_ = dz; }
    }
    // The ringing of a hit reverses direction too, so a shake also needs
    // sustained strong motion.
    const bool shake = reversals_ >= 3 && strongMs_ >= kShakeStrongMs;
    const bool rotate = turnDegrees_ >= 35.0f;
    // A knock or hit late in the window is judged once its burst has ended.
    const bool waitForBurst = burst_ && !shake && !rotate && now - startAt_ < kWindowMaxMs;
    if (now - startAt_ >= kWindowMs && !waitForBurst) {
      MotionKind kind = MotionKind::None;
      if (shake) kind = MotionKind::Shake;
      else if (impact_) kind = MotionKind::Impact;
      else if (rotate) kind = MotionKind::Rotate;
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
