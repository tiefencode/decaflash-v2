#pragma once

#include <cstddef>
#include <cstdint>
#include "sound_personality.h"
namespace decaflash::mainframe {

// Owns the speaker only while the microphone has not been enabled yet.
// It plays the original start jingle and accepts pre-analysis mood phrases.
class StartupSoundPreview {
 public:
  bool begin();
  bool service(uint32_t nowMs);
  bool active() const { return started_ && (!finished_ || moodSoundPlaying_); }
  bool bootAnimationActive() const { return started_ && !finished_; }
  uint8_t bootProgress(uint32_t nowMs) const;
  bool availableForMoodSound() const { return finished_ && !moodSoundPlaying_; }
  bool playMoodSound(const ThresholdCrossingEvent& event);

 private:
  enum class Phase : uint8_t { PcmJingle, PcmJingleWait };

  bool started_ = false;
  bool finished_ = false;
  Phase phase_ = Phase::PcmJingle;
  uint32_t nextCueAtMs_ = 0;
  uint32_t jingleStartsAtMs_ = 0;
  uint8_t* pcmJingle_ = nullptr;
  size_t pcmJingleSize_ = 0;
  uint8_t* pcmCreature_ = nullptr;
  size_t pcmCreatureCapacity_ = 0;
  bool moodSoundPlaying_ = false;
  uint32_t moodSoundEndsAtMs_ = 0;
};

}  // namespace decaflash::mainframe
