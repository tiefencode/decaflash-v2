#pragma once

#include <cstddef>
#include <cstdint>

namespace decaflash::mainframe {

// Pure PCM synthesis of the Mainframe sounds: no speaker, no device calls.
constexpr uint32_t kPcmSampleRate = 44100;
constexpr uint32_t kJingleDurationMs = 2400;
constexpr size_t kJingleSamples = kPcmSampleRate * kJingleDurationMs / 1000;

// Creature sounds: the nine mood phrases 0-8 and the sleep snore.
constexpr uint8_t kCreaturePhraseCount = 9;
constexpr uint8_t kSleepSnoreSound = kCreaturePhraseCount;
constexpr uint8_t kCreatureSoundCount = kCreaturePhraseCount + 1;

void buildPcmJingle(uint8_t* output);
size_t creatureSoundSamples(uint8_t sound);

// Renders one creature sound in pieces of any size. The result does not
// depend on how the samples are split over the render() calls.
class CreatureSoundRenderer {
 public:
  void start(uint8_t sound);
  // Writes up to maxSamples further samples to output and returns how many.
  size_t render(uint8_t* output, size_t maxSamples);
  bool done() const { return index_ >= total_; }

 private:
  uint8_t sound_ = 0;
  size_t index_ = 0;
  size_t total_ = 0;
  uint32_t noise_ = 0;
  float held_ = 0.0f;
};

// Holds every creature sound ready for playback. The sounds are rendered a
// few samples at a time by prepare(), so no caller has to wait for a whole
// sound; a sound can be played once ready() reports it.
class CreatureSoundBank {
 public:
  static size_t requiredBytes();
  // memory must hold requiredBytes() and outlive the bank; nullptr leaves
  // every sound not ready.
  void attach(uint8_t* memory);
  // Renders up to maxSamples samples of the next unfinished sound. Returns
  // true while work remains.
  bool prepare(size_t maxSamples);
  bool allReady() const { return memory_ == nullptr || next_ >= kCreatureSoundCount; }
  bool ready(uint8_t sound) const { return sound < kCreatureSoundCount && ready_[sound]; }
  const uint8_t* data(uint8_t sound) const;
  size_t samples(uint8_t sound) const { return creatureSoundSamples(sound); }

 private:
  uint8_t* memory_ = nullptr;
  uint8_t next_ = 0;  // position in the preparation order
  bool rendering_ = false;
  size_t rendered_ = 0;
  bool ready_[kCreatureSoundCount] = {};
  CreatureSoundRenderer renderer_;
};

}  // namespace decaflash::mainframe
