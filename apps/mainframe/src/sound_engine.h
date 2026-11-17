#pragma once

#include <cstddef>
#include <cstdint>
#include "sound_personality.h"
#include "sound_synth.h"
namespace decaflash::mainframe {

// Owns the speaker only while the microphone has not been enabled yet.
// It plays the original start jingle and accepts pre-analysis mood phrases.
class StartupSoundPreview {
 public:
  bool begin();
  bool service(uint32_t nowMs);
  // Renders a small piece of the creature sounds; call once per loop pass
  // until the audio analysis is requested.
  void prepareSounds();
  uint8_t bootProgress(uint32_t nowMs) const;
  bool availableForMoodSound() const { return finished_ && !moodSoundPlaying_; }
  bool playMoodSound(const ThresholdCrossingEvent& event);
  bool playSleepSnore();

 private:
  bool playCreatureSound(uint8_t sound, uint8_t fullVolume);

  enum class Phase : uint8_t { PcmJingle, PcmJingleWait };

  bool finished_ = false;
  Phase phase_ = Phase::PcmJingle;
  uint32_t nextCueAtMs_ = 0;
  uint32_t jingleStartsAtMs_ = 0;
  uint8_t* pcmJingle_ = nullptr;
  size_t pcmJingleSize_ = 0;
  uint8_t* creatureMemory_ = nullptr;
  CreatureSoundBank creatureSounds_;
  bool moodSoundPlaying_ = false;
  uint32_t moodSoundEndsAtMs_ = 0;
};

}  // namespace decaflash::mainframe
