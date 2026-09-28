#pragma once
#include <cstddef>
#include <cstdint>
#include "personality.h"

namespace decaflash::mainframe {
enum class SoundState : uint8_t { Annoyance, Depression, Attention, Loneliness, Energy };
enum class CrossingDirection : uint8_t { Up, Down };
enum class Waveform : uint8_t { Sine, Square, Saw, Noise };
struct ThresholdCrossingEvent {
  SoundState state;
  uint8_t threshold;
  CrossingDirection direction;
  ThresholdCrossingEvent(SoundState s = SoundState::Energy, uint8_t t = 0,
                         CrossingDirection d = CrossingDirection::Up)
      : state(s), threshold(t), direction(d) {}
};
struct SoundParams {
  Waveform waveform;
  uint16_t durationMs;
  float startFrequency, endFrequency;
  uint8_t bitDepth, sampleRateReduction;
  float distortion, noiseAmount, volume, modulationDepth, modulationRate;
};
namespace sound_config {
// This policy defines the approved mood crossings for a later pre-analysis
// preview. Runtime analysis never emits these sounds after the microphone starts.
constexpr bool enabled = true, downEnabled = true, energyLowEnabled = true;
constexpr uint8_t thresholds[] = {10, 30, 50, 70, 90};
constexpr uint8_t hysteresis = 3;
constexpr uint8_t energyLow = 10, energyLowRearm = 20, energyHigh = 90, energyHighRearm = 80;
constexpr uint32_t minIntervalMs = 10000;
constexpr uint32_t sampleRate = 16000;
constexpr size_t maxSamples = sampleRate * 800 / 1000;
// The Atomic Voice Base is substantially quieter than the display speaker.
// Character profiles still control the actual PCM amplitude.
constexpr uint8_t speakerVolume = 220;
// Earlier entries win. No pending sounds are kept during busy/cooldown periods.
constexpr SoundState priority[] = {SoundState::Annoyance, SoundState::Depression,
  SoundState::Attention, SoundState::Loneliness, SoundState::Energy};
constexpr SoundParams profiles[] = {
  {Waveform::Saw,    160, 230, 65,  6, 3, 3.0f, .18f, .72f, .06f, 31},
  {Waveform::Sine,   580, 110, 48,  6, 8, 1.5f, .08f, .45f, .03f, 3},
  {Waveform::Sine,   130, 360, 640, 8, 1, 1.0f, .02f, .55f, .04f, 18},
  {Waveform::Sine,   440, 165, 110, 8, 2, 1.0f, .03f, .35f, .01f, 2},
  {Waveform::Sine,   360, 100, 55,  8, 4, 1.2f, .04f, .42f, .02f, 3},
  {Waveform::Square, 110, 520, 920, 6, 3, 2.0f, .10f, .60f, .20f, 65}
};
}
class MoodThresholdWatcher {
 public:
  // Always observe, even when output is unavailable: dropped events never replay.
  bool update(const Mood& mood, uint32_t now, bool available, ThresholdCrossingEvent& event);
 private:
  Mood previous_;
  bool upArmed_[4][101] = {}, downArmed_[4][101] = {};
  bool started_ = false, lowArmed_ = false, highArmed_ = false;
  bool emitted_[5] = {};
  uint32_t lastSound_[5] = {};
};
SoundParams mapSound(const ThresholdCrossingEvent& event, uint32_t& random);
size_t synthesizeSound(const SoundParams& params, int16_t* output, size_t capacity, uint32_t& random);
}
