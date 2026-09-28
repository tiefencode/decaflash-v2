#include "sound_personality.h"
#include <algorithm>
#include <cmath>

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
float randomUnit(uint32_t& seed) {
  seed = seed * 1664525U + 1013904223U;
  return static_cast<float>(seed >> 8) / 16777215.0f;
}
}
bool MoodThresholdWatcher::update(const Mood& mood, uint32_t now, bool available,
                                  ThresholdCrossingEvent& event) {
  if (!started_) {
    started_ = true;
    previous_ = mood;
    for (unsigned s = 0; s < 4; ++s) {
      const auto v = value(mood, static_cast<SoundState>(s));
      for (const auto t : sound_config::thresholds) {
        upArmed_[s][t] = v < t;
        downArmed_[s][t] = v > t;
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
      if (!found && (high || (low && sound_config::energyLowEnabled))) {
        event = {state, high ? sound_config::energyHigh : sound_config::energyLow,
                 high ? CrossingDirection::Up : CrossingDirection::Down};
        found = true;
      }
      continue;
    }
    bool stateFound = false;
    ThresholdCrossingEvent candidate;
    for (const auto t : sound_config::thresholds) {
      if (v <= t - sound_config::hysteresis) upArmed_[s][t] = true;
      if (v >= t + sound_config::hysteresis) downArmed_[s][t] = true;
      const bool up = upArmed_[s][t] && old < t && v >= t;
      const bool down = downArmed_[s][t] && old > t && v <= t;
      if (up) upArmed_[s][t] = false;
      if (down) downArmed_[s][t] = false;
      // Falls only speak after the mood has dropped into the lower half.
      // Loneliness deliberately has no falling voice.
      const bool audibleDown = sound_config::downEnabled &&
          state != SoundState::Loneliness && t <= 50;
      if (up || (down && audibleDown)) {
        candidate = {state, static_cast<uint8_t>(t), up ? CrossingDirection::Up : CrossingDirection::Down};
        stateFound = true; // Largest crossed threshold represents a jump.
      }
    }
    if (!found && stateFound) { event = candidate; found = true; }
  }
  previous_ = mood;
  const unsigned stateIndex = static_cast<unsigned>(event.state);
  if (!found || !available || !sound_config::enabled ||
      (emitted_[stateIndex] && now - lastSound_[stateIndex] < sound_config::minIntervalMs)) {
    return false;
  }
  emitted_[stateIndex] = true;
  lastSound_[stateIndex] = now;
  return true;
}
SoundParams mapSound(const ThresholdCrossingEvent& event, uint32_t& random) {
  unsigned index = static_cast<unsigned>(event.state);
  if (event.state == SoundState::Energy && event.direction == CrossingDirection::Up) index = 5;
  auto p = sound_config::profiles[index];
  const float strength = event.threshold / 100.0f;
  if (event.state == SoundState::Annoyance) {
    p.distortion += 5 * strength;
    p.bitDepth = event.threshold >= 80 ? 4 : 6;
    if (event.threshold == 100) p.durationMs = 75;
  } else if (event.state == SoundState::Depression) {
    p.endFrequency *= 1 - .35f * strength;
    p.sampleRateReduction += static_cast<uint8_t>(8 * strength);
  } else if (event.state == SoundState::Attention) {
    p.startFrequency *= 1 + .4f * strength;
    p.endFrequency *= 1 + .4f * strength;
  }
  if (event.direction == CrossingDirection::Down && event.state != SoundState::Energy)
    std::swap(p.startFrequency, p.endFrequency);
  const float pitch = .95f + .10f * randomUnit(random);
  p.startFrequency *= pitch; p.endFrequency *= pitch;
  p.durationMs = static_cast<uint16_t>(p.durationMs * (.9f + .2f * randomUnit(random)));
  p.noiseAmount *= .9f + .2f * randomUnit(random);
  return p;
}
size_t synthesizeSound(const SoundParams& p, int16_t* output, size_t capacity, uint32_t& random) {
  const size_t count = std::min(capacity, static_cast<size_t>(p.durationMs) * sound_config::sampleRate / 1000);
  float phase = 0, held = 0;
  const unsigned reduction = std::max(1U, static_cast<unsigned>(p.sampleRateReduction));
  const unsigned bits = std::max(2U, std::min(16U, static_cast<unsigned>(p.bitDepth)));
  const float levels = static_cast<float>((1U << (bits - 1)) - 1);
  for (size_t i = 0; i < count; ++i) {
    const float t = static_cast<float>(i) / std::max(size_t(1), count - 1);
    const float modulation = std::sin(6.2831853f * p.modulationRate * i / sound_config::sampleRate);
    phase += (p.startFrequency + (p.endFrequency - p.startFrequency) * t) *
             (1 + p.modulationDepth * modulation) / sound_config::sampleRate;
    phase -= std::floor(phase);
    if (i % reduction == 0) {
      float wave = std::sin(6.2831853f * phase);
      if (p.waveform == Waveform::Saw) wave = 2 * phase - 1;
      if (p.waveform == Waveform::Square) wave = phase < .5f ? 1 : -1;
      if (p.waveform == Waveform::Noise) wave = 2 * randomUnit(random) - 1;
      wave += p.noiseAmount * (2 * randomUnit(random) - 1);
      wave = std::max(-1.0f, std::min(1.0f, wave * p.distortion));
      held = std::round(wave * levels) / levels;
    }
    const float envelope = std::min(1.0f, std::min(t * 20, (1 - t) * 5));
    output[i] = static_cast<int16_t>(held * envelope * std::max(0.0f, std::min(1.0f, p.volume)) * 32767);
  }
  return count;
}
}
