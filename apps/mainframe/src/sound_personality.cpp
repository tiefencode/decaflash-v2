#include "sound_personality.h"

namespace decaflash::mainframe {
namespace {
uint8_t value(const Mood& m, SoundState s) {
  switch (s) {
    case SoundState::Annoyance: return m.annoyance;
    case SoundState::Depression: return m.depression;
    case SoundState::Attention: return m.attention;
    case SoundState::Loneliness: return m.loneliness;
    default: return m.energy;
  }
}
}
bool MoodThresholdWatcher::update(const Mood& mood, uint32_t now, bool available,
                                  ThresholdCrossingEvent& event) {
  if (!started_) {
    started_ = true;
    previous_ = mood;
    for (unsigned s = 0; s < 4; ++s) {
      const auto v = value(mood, static_cast<SoundState>(s));
      for (uint8_t i = 0; i < kThresholdCount; ++i) {
        const auto t = sound_config::thresholds[i];
        upArmed_[s][i] = v < t;
        downArmed_[s][i] = v > t;
      }
    }
    lowArmed_ = mood.energy > sound_config::energyLow;
    highArmed_ = mood.energy < sound_config::energyHigh;
    return false; // A boot snapshot is not a crossing.
  }
  bool found = false;
  for (auto state : sound_config::priority) {
    const unsigned s = static_cast<unsigned>(state);
    const int v = value(mood, state), old = value(previous_, state);
    if (state == SoundState::Energy) {
      if (v >= sound_config::energyLowRearm) lowArmed_ = true;
      if (v <= sound_config::energyHighRearm) highArmed_ = true;
      const bool low = lowArmed_ && old > sound_config::energyLow && v <= sound_config::energyLow;
      const bool high = highArmed_ && old < sound_config::energyHigh && v >= sound_config::energyHigh;
      if (low) lowArmed_ = false;
      if (high) highArmed_ = false;
      if (!found && (high || low)) {
        event = {state, high ? sound_config::energyHigh : sound_config::energyLow,
                 high ? CrossingDirection::Up : CrossingDirection::Down};
        found = true;
      }
      continue;
    }
    bool stateFound = false;
    ThresholdCrossingEvent candidate;
    for (uint8_t i = 0; i < kThresholdCount; ++i) {
      const auto t = sound_config::thresholds[i];
      // Mood values are already integer and protected by the ten-second
      // per-state cooldown. Re-arm exactly on the other side of a threshold:
      // after 70 -> 69, a later 69 -> 70 must be a real new crossing.
      if (v < t) upArmed_[s][i] = true;
      if (v >= t) downArmed_[s][i] = true;
      const bool up = upArmed_[s][i] && old < t && v >= t;
      // A DOWN sound marks leaving a band. For example, rage is active at
      // 90, so the Annoyance-down phrase must play on 90 -> 89, not on
      // merely arriving at 90.
      const bool down = downArmed_[s][i] && old >= t && v < t;
      if (up) upArmed_[s][i] = false;
      if (down) downArmed_[s][i] = false;
      // Annoyance reacts across its full range. Other moods only speak after
      // dropping into the lower half; Loneliness has no falling voice.
      const bool audibleDown = state != SoundState::Loneliness &&
          (state == SoundState::Annoyance || t <= 50);
      if (up || (down && audibleDown)) {
        candidate = {state, static_cast<uint8_t>(t), up ? CrossingDirection::Up : CrossingDirection::Down};
        stateFound = true; // Largest crossed threshold represents a jump.
      }
    }
    if (!found && stateFound) { event = candidate; found = true; }
  }
  previous_ = mood;
  const unsigned stateIndex = static_cast<unsigned>(event.state);
  if (!found || !available ||
      (emitted_[stateIndex] && now - lastSound_[stateIndex] < sound_config::minIntervalMs)) {
    return false;
  }
  emitted_[stateIndex] = true;
  lastSound_[stateIndex] = now;
  return true;
}
}
