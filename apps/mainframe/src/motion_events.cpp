#include "motion_events.h"
#include <algorithm>
#include <cmath>
namespace decaflash::mainframe {
namespace {
float norm(float x, float y, float z) { return std::sqrt(x*x + y*y + z*z); }

// Knock and hit levels for the Mainframe mounted in the 3 kg wooden sphere,
// taken from tests/fixtures/motion/2026-11-16-wood-sphere.txt. Rest stays
// below 0.03 g there, normal knocks peak at 0.23-0.60 g and ring for up to
// 100 ms, hits peak at 0.69-2.6 g and ring for 160-430 ms. The rest level at
// show volume is not measured; raise kKnockG if loud bass triggers knocks.
constexpr float kKnockG = 0.20f;          // a burst starts and continues here
constexpr float kImpactG = 0.65f;         // burst peak that makes it a hit
constexpr uint32_t kBurstQuietMs = 100;   // this long below kKnockG ends a burst
constexpr uint32_t kKnockMaxMs = 130;     // longest burst that counts as a knock
constexpr uint32_t kImpactMaxMs = 500;    // longer strong bursts are shake or roll
// Moving the sphere gently produces short bursts as well, but stays above
// kActiveG far longer than a knock: 290-700 ms per window in the capture of
// 2026-11-16 (tests/fixtures/motion/), against at most 170 ms for a knock.
constexpr float kActiveG = 0.10f;
constexpr uint16_t kKnockActiveMs = 220;      // active time allowed for one knock
constexpr uint16_t kKnockActiveMoreMs = 170;  // and for each further knock
constexpr uint32_t kKnockCalmMs = 1000;       // no knocks this long after such a window
constexpr float kStrongG = 0.45f;
constexpr uint16_t kShakeStrongMs = 350;  // hit ringing stays below this, a shake does not
constexpr uint32_t kWindowMs = 700;
constexpr uint32_t kWindowMaxMs = 1100;   // a burst still running may extend the window

// Tipping: the gravity direction against the pose at start. Not measured on
// the device yet.
constexpr float kTipCosine = 0.5f;        // 60 degrees: tipped over, reports Rotate
constexpr float kUprightCosine = 0.707f;  // back within 45 degrees re-arms it
constexpr uint32_t kTipHoldMs = 300;
// Looking direction: the rotation rate around the vertical, integrated since
// the direction was last learned. Rates below the dead band are ignored so
// that gyro offset does not add up at rest (rest stays at 0-1 degrees/s in
// the wooden sphere capture).
constexpr float kYawDeadbandDps = 5.0f;
constexpr float kRepositionDegrees = 45.0f;
constexpr float kRepositionReleaseDegrees = 30.0f;
constexpr uint32_t kYawSettleMs = 500;      // no turning for this long: settled
constexpr uint32_t kRepositionRepeatMs = 9000;  // the panel's cooldown per kind
constexpr uint32_t kRelearnMs = 60000;      // settled this long: new looking direction
constexpr float kSpinDegrees = 360.0f;      // one full turn in one go reports Rotate
}
const char* motionName(MotionKind kind) {
  switch (kind) {
    case MotionKind::Move: return "move";
    case MotionKind::Tap: return "tap";
    case MotionKind::MultiTap: return "multi-tap";
    case MotionKind::Impact: return "impact";
    case MotionKind::Rotate: return "rotate";
    case MotionKind::Reposition: return "reposition";
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
  activeMs_ = 0;
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
MotionEvent MotionEvents::report(MotionKind kind, uint32_t now, uint8_t count, float peakG) {
  latest_.kind = kind; latest_.atMs = now; ++latest_.sequence;
  latest_.count = count; latest_.peakG = peakG;
  lastEventAt_ = now; haveEvent_ = true;
  return latest_;
}
MotionEvent MotionEvents::feed(uint32_t now, const MotionSample& s) {
  MotionEvent none;
  if (!std::isfinite(s.ax) || !std::isfinite(s.ay) || !std::isfinite(s.az) ||
      !std::isfinite(s.gx) || !std::isfinite(s.gy) || !std::isfinite(s.gz)) {
    clearWindow(); tipCandidate_ = false; return none;
  }
  const float a = norm(s.ax, s.ay, s.az), rotation = norm(s.gx, s.gy, s.gz);
  if (!seeded_) {
    if (a < 0.85f || a > 1.15f || rotation > 15) return none;
    rx_ = fx_ = s.ax; ry_ = fy_ = s.ay; rz_ = fz_ = s.az;
    lastAt_ = now; yawAt_ = now; seeded_ = true; return none;
  }
  const uint32_t elapsed = now - lastAt_; lastAt_ = now;
  if (elapsed > 150) { clearWindow(); tipCandidate_ = false; return none; }
  // Slow gravity estimate: reject individual impulses for pose estimation.
  const float dx = s.ax - fx_, dy = s.ay - fy_, dz = s.az - fz_;
  const float dynamic = norm(dx, dy, dz);
  const float alpha = static_cast<float>(elapsed) / (250.0f + elapsed);
  fx_ += alpha * dx; fy_ += alpha * dy; fz_ += alpha * dz;
  // Pose must come from the current, quiet gravity vector. Using the dynamic
  // low-pass estimate here lets a strong linear movement masquerade as a tip.
  const float poseDenom = a * norm(rx_, ry_, rz_);
  const float poseCosine = poseDenom > 0.1f
    ? (s.ax * rx_ + s.ay * ry_ + s.az * rz_) / poseDenom : 1;
  if (poseCosine > kUprightCosine) { tipped_ = false; tipCandidate_ = false; }
  if (std::fabs(a - 1) < 0.12f && poseCosine <= kTipCosine) {
    if (!tipCandidate_) { tipCandidate_ = true; tipSince_ = now; }
  } else { tipCandidate_ = false; }

  // Turning around the vertical changes where the device looks. The vertical
  // is the gravity estimate, so the mounting orientation does not matter.
  const float up = norm(fx_, fy_, fz_);
  const float yawRate = up > 0.5f ? (s.gx * fx_ + s.gy * fy_ + s.gz * fz_) / up : 0;
  if (std::fabs(yawRate) >= kYawDeadbandDps) {
    const float step = yawRate * elapsed / 1000.0f;
    heading_ += step; spin_ += step; yawAt_ = now;
    if (heading_ > 180) heading_ -= 360; else if (heading_ <= -180) heading_ += 360;
  }
  const uint32_t yawQuietMs = now - yawAt_;
  if (yawQuietMs >= kYawSettleMs) spin_ = 0;
  if (yawQuietMs >= kRelearnMs) heading_ = 0;
  const float offset = std::fabs(heading_);
  if (offset < kRepositionReleaseDegrees) repositioned_ = false;
  const bool displaced = offset >= kRepositionDegrees || repositioned_;

  if (!window_ && (dynamic >= kKnockG || rotation >= 45)) {
    clearWindow(); window_ = true; startAt_ = now;
    if (haveBusy_ && now - busyAt_ >= kKnockCalmMs) haveBusy_ = false;
    afterBusy_ = haveBusy_;
  }
  if (window_) {
    peak_ = std::max(peak_, dynamic);
    // Keep integrating deliberate gyro motion while the device is carried.
    // A window with 35 degrees of turning is handling, so its bursts are not
    // knocks; the threshold rejects brief gyro spikes from linear handling.
    if (rotation >= 45) {
      turnDegrees_ += rotation * elapsed / 1000.0f;
    }
    if (dynamic >= 0.30f) moveMs_ += elapsed;
    if (dynamic >= kStrongG) strongMs_ += elapsed;
    if (dynamic >= kActiveG && activeMs_ < 60000) activeMs_ += elapsed;
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
    const bool turning = turnDegrees_ >= 35.0f;
    // A knock or hit late in the window is judged once its burst has ended.
    // Shaking and hitting turn the sphere as well, so turning must not end the
    // window while a strong burst is still running: it would drop a shake
    // that started late.
    const bool strongBurst = burst_ && burstPeak_ >= kImpactG;
    const bool waitForBurst = burst_ && !shake && (!turning || strongBurst) &&
                              now - startAt_ < kWindowMaxMs;
    if (now - startAt_ >= kWindowMs && !waitForBurst) {
      MotionKind kind = MotionKind::None;
      // Knocks leave the sphere quiet again quickly; a window that stays
      // active is the sphere being moved, whatever short bursts it contains.
      // The window after such a one is still the same movement.
      const bool calm = activeMs_ <= kKnockActiveMs + (taps_ ? taps_ - 1 : 0) * kKnockActiveMoreMs;
      const bool knocking = !turning && taps_ > 0 && calm && !afterBusy_;
      if (!calm || turning) { haveBusy_ = true; busyAt_ = now; }
      if (shake) kind = MotionKind::Shake;
      else if (impact_) kind = MotionKind::Impact;
      else if (knocking && taps_ >= 2) kind = MotionKind::MultiTap;
      else if (knocking && moveMs_ <= 120) kind = MotionKind::Tap;
      else if (moveMs_ >= 150) kind = MotionKind::Move;
      const uint8_t count = taps_; const float peak = peak_;
      clearWindow();
      if (kind != MotionKind::None && canReport(now)) {
        return report(kind, now, count, peak);
      } else if (kind != MotionKind::None) {
        return none;
      }
    }
  }
  // A full turn around the vertical in one go. Shaking and hitting turn the
  // device too, so a window that looks like either keeps its own verdict.
  const bool rough = window_ && (reversals_ > 0 || (burst_ && burstPeak_ >= kImpactG));
  if (std::fabs(spin_) >= kSpinDegrees && !rough && canReport(now)) {
    spin_ = 0; clearWindow();
    return report(MotionKind::Rotate, now, 1, 0);
  }
  // Tipped over backwards, forwards or sideways: reported once, armed again
  // after it has been set upright.
  if (!window_ && tipCandidate_ && !tipped_ && now - tipSince_ >= kTipHoldMs &&
      canReport(now)) {
    tipped_ = true;
    return report(MotionKind::Rotate, now, 1, 0);
  }
  // Left looking somewhere else: repeated until it is turned back or the new
  // direction has been learned.
  if (!window_ && displaced && yawQuietMs >= kYawSettleMs &&
      (!repositioned_ || now - repositionReportAt_ >= kRepositionRepeatMs) &&
      canReport(now)) {
    repositioned_ = true; repositionReportAt_ = now;
    return report(MotionKind::Reposition, now, 1, 0);
  }
  return none;
}
}  // namespace decaflash::mainframe
