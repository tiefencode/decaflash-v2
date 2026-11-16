#include "presence_debug.h"

#include <cstdio>
#include <initializer_list>

namespace decaflash::mainframe {
namespace {

size_t finish(int written, size_t size) {
  if (written < 0 || static_cast<size_t>(written) >= size) return 0;
  return static_cast<size_t>(written);
}

size_t flagIndex(PresenceFlag which) { return static_cast<size_t>(which); }

}  // namespace

void PresenceDebug::reset() {
  latest_ = PresenceSample{};
  head_ = 0;
  count_ = 0;
  for (size_t i = 0; i < 3; ++i) {
    flagChangedAt_[i] = 0;
    flagState_[i] = false;
  }
}

void PresenceDebug::setState(PresenceSensorState state, uint32_t errors) {
  // A sensor that disappears leaves a gap; the next session starts a fresh
  // history instead of joining two unrelated curves.
  if (state == PresenceSensorState::Missing && state_ != PresenceSensorState::Missing) {
    reset();
    haveConfig_ = false;
  }
  state_ = state;
  errors_ = errors;
}

void PresenceDebug::add(const PresenceSample& sample) {
  if (!sample.valid) return;
  const bool flags[3] = {sample.presenceFlag, sample.motionFlag, sample.shockFlag};
  for (size_t i = 0; i < 3; ++i) {
    if (count_ == 0 || flags[i] != flagState_[i]) {
      flagState_[i] = flags[i];
      flagChangedAt_[i] = sample.atMs;
    }
  }
  presence_[head_] = sample.presence;
  motion_[head_] = sample.motion;
  head_ = (head_ + 1) % kHistorySize;
  if (count_ < kHistorySize) ++count_;
  latest_ = sample;
}

int16_t PresenceDebug::valueAt(PresenceSeries series, size_t index) const {
  if (index >= count_) return 0;
  const size_t start = (head_ + kHistorySize - count_) % kHistorySize;
  const size_t at = (start + index) % kHistorySize;
  return series == PresenceSeries::Presence ? presence_[at] : motion_[at];
}

bool PresenceDebug::flag(PresenceFlag which) const {
  return count_ > 0 && flagState_[flagIndex(which)];
}

uint32_t PresenceDebug::flagAgeMs(PresenceFlag which, uint32_t now) const {
  if (count_ == 0) return kUnknownAge;
  return now - flagChangedAt_[flagIndex(which)];
}

PresenceGraphRange PresenceDebug::range(PresenceSeries series, int32_t threshold,
                                        int32_t extraThreshold,
                                        int32_t secondExtraThreshold) const {
  PresenceGraphRange result;
  result.low = 0;
  result.high = 0;
  for (const int32_t limit : {threshold, extraThreshold, secondExtraThreshold}) {
    if (limit < result.low) result.low = limit;
    if (limit > result.high) result.high = limit;
  }
  for (size_t i = 0; i < count_; ++i) {
    const int32_t value = valueAt(series, i);
    if (value < result.low) result.low = value;
    if (value > result.high) result.high = value;
  }
  if (result.high == result.low) result.high = result.low + 1;
  return result;
}

size_t formatCentiCelsius(char* out, size_t size, int16_t centiC) {
  const int32_t value = centiC;
  const int32_t tenths = value >= 0 ? (value + 5) / 10 : (value - 5) / 10;
  const int32_t magnitude = tenths < 0 ? -tenths : tenths;
  return finish(std::snprintf(out, size, "%s%ld.%ld", tenths < 0 ? "-" : "",
                              static_cast<long>(magnitude / 10),
                              static_cast<long>(magnitude % 10)),
                size);
}

const char* personName(PersonState person) {
  switch (person) {
    case PersonState::Far: return "far";
    case PersonState::FarMoving: return "far_moving";
    case PersonState::Near: return "near";
    case PersonState::Away: break;
  }
  return "away";
}

const char* presenceEventName(PresenceEventKind kind) {
  switch (kind) {
    case PresenceEventKind::StandsInFront: return "stands_in_front";
    case PresenceEventKind::WalkedPast: return "walked_past";
    case PresenceEventKind::Left: return "left";
    case PresenceEventKind::None: break;
  }
  return "none";
}

size_t formatPresenceEventLine(char* out, size_t size, const PresenceEvent& event) {
  return finish(std::snprintf(out, size, "TMOS_EVENT t=%lu kind=%s\n",
                              static_cast<unsigned long>(event.atMs),
                              presenceEventName(event.kind)),
                size);
}

size_t formatPresenceSampleLine(char* out, size_t size, const PresenceSample& sample,
                                uint32_t errors, PersonState person) {
  char ambient[12];
  if (formatCentiCelsius(ambient, sizeof(ambient), sample.ambientCentiC) == 0) return 0;
  return finish(std::snprintf(
                  out, size,
                  "TMOS t=%lu presence_flag=%u motion_flag=%u shock_flag=%u presence=%d "
                  "motion=%d shock=%d object=%d object_comp=%d ambient=%s errors=%lu person=%s\n",
                  static_cast<unsigned long>(sample.atMs), sample.presenceFlag ? 1U : 0U,
                  sample.motionFlag ? 1U : 0U, sample.shockFlag ? 1U : 0U,
                  static_cast<int>(sample.presence), static_cast<int>(sample.motion),
                  static_cast<int>(sample.shock), static_cast<int>(sample.objectRaw),
                  static_cast<int>(sample.objectComp), ambient,
                  static_cast<unsigned long>(errors), personName(person)),
                size);
}

size_t formatPresenceConfigLine(char* out, size_t size, const PresenceConfig& config) {
  return finish(std::snprintf(
                  out, size,
                  "TMOS_CONFIG odr_hz=%u avg_tmos=%u avg_t=%u presence_ths=%u presence_hyst=%u "
                  "motion_ths=%u motion_hyst=%u shock_ths=%u shock_hyst=%u sensitivity=%u\n",
                  static_cast<unsigned>(config.odrHz), static_cast<unsigned>(config.avgTmos),
                  static_cast<unsigned>(config.avgT),
                  static_cast<unsigned>(config.presenceThreshold),
                  static_cast<unsigned>(config.presenceHysteresis),
                  static_cast<unsigned>(config.motionThreshold),
                  static_cast<unsigned>(config.motionHysteresis),
                  static_cast<unsigned>(config.shockThreshold),
                  static_cast<unsigned>(config.shockHysteresis),
                  static_cast<unsigned>(config.objectSensitivity)),
                size);
}

size_t formatPresenceStateLine(char* out, size_t size, bool sensorOk, uint32_t errors) {
  return finish(std::snprintf(out, size, "TMOS_STATE sensor=%s errors=%lu\n",
                              sensorOk ? "ok" : "missing",
                              static_cast<unsigned long>(errors)),
                size);
}

size_t formatHeatCelsius(char* out, size_t size, int16_t objectRaw, uint16_t sensitivity) {
  if (sensitivity == 0) return finish(std::snprintf(out, size, "--"), size);
  const int32_t scaled = static_cast<int32_t>(objectRaw) * 100;
  const int32_t half = sensitivity / 2;
  const int32_t hundredths = scaled >= 0 ? (scaled + half) / sensitivity
                                         : (scaled - half) / sensitivity;
  const int32_t magnitude = hundredths < 0 ? -hundredths : hundredths;
  return finish(std::snprintf(out, size, "%s%ld.%02ld", hundredths < 0 ? "-" : "+",
                              static_cast<long>(magnitude / 100),
                              static_cast<long>(magnitude % 100)),
                size);
}

size_t formatPresenceAge(char* out, size_t size, uint32_t ageMs) {
  if (ageMs == PresenceDebug::kUnknownAge) return finish(std::snprintf(out, size, "--"), size);
  const uint32_t seconds = ageMs / 1000U;
  if (seconds < 100U) {
    return finish(std::snprintf(out, size, "%lus", static_cast<unsigned long>(seconds)), size);
  }
  uint32_t minutes = seconds / 60U;
  if (minutes > 999U) minutes = 999U;
  return finish(std::snprintf(out, size, "%lum", static_cast<unsigned long>(minutes)), size);
}

}  // namespace decaflash::mainframe
