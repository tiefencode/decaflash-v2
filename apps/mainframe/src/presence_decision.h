#pragma once

#include <cstddef>
#include <cstdint>

namespace decaflash::mainframe {

// What the Mainframe concludes about a person in front of the eye. The sensor
// flag is not trusted on its own: a stale sensor baseline sets it with nobody
// there, so presence uses the Mainframe's own thresholds on the raw value.
enum class PersonState : uint8_t { Away, Still, Moving };

enum class PresenceEventKind : uint8_t { None, ArrivedInFront, WalkedPast, Left, MovesALot };

struct PresenceEvent {
  PresenceEventKind kind = PresenceEventKind::None;
  uint32_t atMs = 0;
  uint16_t sequence = 0;  // per kind, for deterministic text variants
};

// Personality input: whether someone is present and how much they move.
struct MoodPresence {
  bool present = false;
  uint16_t activityPermille = 0;  // share of recent samples with motion, 0-1000
};

struct PresenceSampleInput {
  bool valid = false;
  int16_t presence = 0;
  bool motionFlag = false;
  uint32_t atMs = 0;
};

// Calibration values from the add-presence-mood design; fixed on the device.
struct PresenceDecisionConfig {
  int16_t enterThreshold = 900;
  int16_t leaveThreshold = 600;
  uint32_t settleMs = 15000;
  uint32_t activityWindowMs = 3000;
  uint32_t dwellMs = 3000;
  uint16_t movesALotPermille = 500;
  uint16_t movesALotRearmPermille = 200;
};

class PresenceDecision {
 public:
  explicit PresenceDecision(const PresenceDecisionConfig& config = PresenceDecisionConfig())
    : config_(config) {}

  // The sensor was (re)configured at `now`; samples before settling count as away.
  void sensorConfigured(uint32_t now);
  // The sensor is gone; nobody is present until it is configured again.
  void sensorMissing(uint32_t now);
  // One sensor sample, valid or not. Returns at most one event.
  PresenceEvent feed(const PresenceSampleInput& sample);

  bool present() const { return present_; }
  uint16_t activityPermille() const { return present_ ? activity_ : 0; }
  MoodPresence mood() const {
    MoodPresence result;
    result.present = present_;
    result.activityPermille = activityPermille();
    return result;
  }
  PersonState state() const { return state_; }
  uint32_t stateChangedAtMs() const { return stateChangedAt_; }
  bool haveState() const { return haveState_; }
  const PresenceDecisionConfig& config() const { return config_; }

 private:
  static constexpr size_t kWindowSamples = 48;  // 6 s at 8 Hz, more than the window
  void reset();
  void setState(PersonState state, uint32_t now);
  uint16_t computeActivity(uint32_t now) const;
  PresenceEvent makeEvent(PresenceEventKind kind, uint32_t now);

  PresenceDecisionConfig config_;
  uint32_t windowAt_[kWindowSamples] = {};
  bool windowMotion_[kWindowSamples] = {};
  size_t windowHead_ = 0;
  size_t windowCount_ = 0;
  uint32_t settleUntil_ = 0;
  uint32_t presentSince_ = 0;
  uint32_t stateChangedAt_ = 0;
  uint16_t activity_ = 0;
  uint16_t sequences_[5] = {};
  PersonState state_ = PersonState::Away;
  bool sensorReady_ = false;
  bool present_ = false;
  bool arrived_ = false;
  bool movesArmed_ = true;
  bool haveState_ = false;
};

}  // namespace decaflash::mainframe
