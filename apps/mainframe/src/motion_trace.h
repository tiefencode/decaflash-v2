#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "motion_events.h"

namespace decaflash::mainframe {
// Measurement only (mainframe-motion-capture). Summarises raw IMU activity far
// below the classifier thresholds so thresholds can be chosen from device
// data. No sensor I/O, no serial output, no effect on the classifier.
struct MotionTraceWindow {
  static constexpr uint8_t kRawCapacity = 96;
  uint32_t sequence = 0;
  uint32_t startMs = 0;
  uint16_t peakMg = 0;      // peak acceleration above the gravity estimate
  uint16_t peakDps = 0;     // peak rotation rate
  uint16_t peakAtMs = 0;    // time from window start to the acceleration peak
  uint16_t overMs[5] = {};  // time spent at or above 30/100/200/300/450 mg
  uint16_t samples = 0;
  uint8_t rawCount = 0;
  uint16_t rawMg[kRawCapacity] = {};
};
struct MotionTraceIdle {
  uint32_t atMs = 0;
  uint16_t maxMg = 0;
  uint16_t maxDps = 0;
  uint16_t samples = 0;   // samples seen in the report period
  uint16_t maxGapMs = 0;  // longest pause between two samples
};
class MotionTrace {
 public:
  enum class Output : uint8_t { None, Window, Idle };
  static constexpr uint16_t kTriggerMg = 30;
  static constexpr uint16_t kTriggerDps = 8;
  static constexpr uint32_t kWindowMs = 700;  // same length as the classifier window
  static constexpr uint32_t kIdlePeriodMs = 2000;

  // Feed every new sensor sample. Units: g and degrees/second.
  Output feed(uint32_t now, const MotionSample& s) {
    if (!std::isfinite(s.ax) || !std::isfinite(s.ay) || !std::isfinite(s.az) ||
        !std::isfinite(s.gx) || !std::isfinite(s.gy) || !std::isfinite(s.gz)) {
      return Output::None;
    }
    if (!seeded_) {
      fx_ = s.ax; fy_ = s.ay; fz_ = s.az;
      lastAt_ = idleStartAt_ = now; seeded_ = true;
      return Output::None;
    }
    const uint32_t elapsed = now - lastAt_; lastAt_ = now;
    // Same gravity estimate as MotionEvents, so "mg" here means what the
    // classifier calls `dynamic`.
    const float dx = s.ax - fx_, dy = s.ay - fy_, dz = s.az - fz_;
    const float alpha = static_cast<float>(elapsed) / (250.0f + elapsed);
    fx_ += alpha * dx; fy_ += alpha * dy; fz_ += alpha * dz;
    const uint16_t mg = clamp16(std::sqrt(dx*dx + dy*dy + dz*dz) * 1000.0f);
    const uint16_t dps = clamp16(std::sqrt(s.gx*s.gx + s.gy*s.gy + s.gz*s.gz));
    const uint16_t step = static_cast<uint16_t>(std::min<uint32_t>(elapsed, 50));

    if (!open_ && (mg >= kTriggerMg || dps >= kTriggerDps)) {
      const uint32_t sequence = window_.sequence + 1;
      window_ = MotionTraceWindow();
      window_.sequence = sequence; window_.startMs = now;
      open_ = true;
    }
    if (open_) {
      if (mg > window_.peakMg) {
        window_.peakMg = mg;
        window_.peakAtMs = static_cast<uint16_t>(now - window_.startMs);
      }
      window_.peakDps = std::max(window_.peakDps, dps);
      // Local on purpose: the device toolchain builds as C++11, where a
      // static constexpr array member would need an out-of-class definition.
      const uint16_t kLevelsMg[5] = {30, 100, 200, 300, 450};
      for (uint8_t i = 0; i < 5; ++i) {
        if (mg >= kLevelsMg[i]) window_.overMs[i] += step;
      }
      ++window_.samples;
      if (window_.rawCount < MotionTraceWindow::kRawCapacity) {
        window_.rawMg[window_.rawCount++] = mg;
      }
      if (now - window_.startMs >= kWindowMs) {
        open_ = false;
        return Output::Window;
      }
      return Output::None;
    }
    // Idle statistics cover only samples outside a window: the resting floor.
    pending_.maxMg = std::max(pending_.maxMg, mg);
    pending_.maxDps = std::max(pending_.maxDps, dps);
    pending_.maxGapMs = std::max(pending_.maxGapMs, clamp16(static_cast<float>(elapsed)));
    ++pending_.samples;
    if (now - idleStartAt_ >= kIdlePeriodMs) {
      idle_ = pending_; idle_.atMs = now;
      pending_ = MotionTraceIdle(); idleStartAt_ = now;
      return Output::Idle;
    }
    return Output::None;
  }
  const MotionTraceWindow& window() const { return window_; }
  const MotionTraceIdle& idle() const { return idle_; }

 private:
  static uint16_t clamp16(float value) {
    return value >= 65535.0f ? 65535 : static_cast<uint16_t>(value + 0.5f);
  }
  MotionTraceWindow window_;
  MotionTraceIdle idle_, pending_;
  bool seeded_ = false, open_ = false;
  uint32_t lastAt_ = 0, idleStartAt_ = 0;
  float fx_ = 0, fy_ = 0, fz_ = 1;
};
}  // namespace decaflash::mainframe
