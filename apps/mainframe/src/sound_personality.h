#pragma once
#include <cstdint>
#include "personality.h"

namespace decaflash::mainframe {
enum class SoundState : uint8_t { Annoyance, Depression, Attention, Loneliness, Energy };
enum class CrossingDirection : uint8_t { Up, Down };
struct ThresholdCrossingEvent {
  SoundState state;
  uint8_t threshold;
  CrossingDirection direction;
  ThresholdCrossingEvent(SoundState s = SoundState::Energy, uint8_t t = 0,
                         CrossingDirection d = CrossingDirection::Up)
      : state(s), threshold(t), direction(d) {}
};
namespace sound_config {
// This policy defines the approved mood crossings for a later pre-analysis
// preview. Runtime analysis never emits these sounds after the microphone starts.
constexpr uint8_t thresholds[] = {10, 30, 50, 70, 90};
constexpr uint8_t energyLow = 10, energyLowRearm = 20, energyHigh = 90, energyHighRearm = 80;
constexpr uint32_t minIntervalMs = 10000;
// Earlier entries win. No pending sounds are kept during busy/cooldown periods.
constexpr SoundState priority[] = {SoundState::Annoyance, SoundState::Depression,
  SoundState::Attention, SoundState::Loneliness, SoundState::Energy};
}
class MoodThresholdWatcher {
 public:
  // Always observe, even when output is unavailable: dropped events never replay.
  bool update(const Mood& mood, uint32_t now, bool available, ThresholdCrossingEvent& event);
 private:
  static constexpr uint8_t kThresholdCount = sizeof(sound_config::thresholds) /
                                             sizeof(sound_config::thresholds[0]);
  Mood previous_;
  bool upArmed_[4][kThresholdCount] = {}, downArmed_[4][kThresholdCount] = {};
  bool started_ = false, lowArmed_ = false, highArmed_ = false;
  bool emitted_[5] = {};
  uint32_t lastSound_[5] = {};
};
}
