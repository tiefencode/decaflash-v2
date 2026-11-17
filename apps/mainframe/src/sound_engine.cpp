#include "sound_engine.h"

#include <algorithm>

#include <M5Unified.h>
#include <esp_heap_caps.h>

namespace decaflash::mainframe {
namespace {

// Preparing the creature sounds may take this long per loop pass. Rendering
// goes in pieces of kPrepareChunkSamples; the device needs 16-37 us per
// sample, so one piece stays near one millisecond.
constexpr uint32_t kPrepareBudgetUs = 2000;
constexpr size_t kPrepareChunkSamples = 32;

bool due(uint32_t now, uint32_t target) {
  return static_cast<int32_t>(now - target) >= 0;
}

int phraseForEvent(const ThresholdCrossingEvent& event) {
  switch (event.state) {
    case SoundState::Energy:
      return event.direction == CrossingDirection::Up ? 0 : 1;
    case SoundState::Annoyance:
      return event.direction == CrossingDirection::Up ? 2 : 3;
    case SoundState::Attention:
      return event.direction == CrossingDirection::Up ? 4 : 5;
    case SoundState::Loneliness:
      return event.direction == CrossingDirection::Up ? 6 : -1;
    case SoundState::Depression:
      return event.direction == CrossingDirection::Up ? 7 : 8;
  }
  return -1;
}

uint8_t previewVolume(uint8_t fullVolume) {
  return fullVolume / 2;
}

}  // namespace

bool StartupSoundPreview::begin() {
  const bool speakerReady = M5.Speaker.begin();
  finished_ = !speakerReady;
  phase_ = Phase::PcmJingle;
  moodSoundPlaying_ = false;
  if (pcmJingle_ == nullptr) {
    pcmJingle_ = static_cast<uint8_t*>(
      heap_caps_malloc(kJingleSamples, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pcmJingle_ != nullptr) {
      pcmJingleSize_ = kJingleSamples;
      buildPcmJingle(pcmJingle_);
    }
  }
  if (creatureMemory_ == nullptr) {
    creatureMemory_ = static_cast<uint8_t*>(heap_caps_malloc(
      CreatureSoundBank::requiredBytes(), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    creatureSounds_.attach(creatureMemory_);
  }
  nextCueAtMs_ = millis() + 180;
  jingleStartsAtMs_ = nextCueAtMs_;
  return speakerReady;
}

uint8_t StartupSoundPreview::bootProgress(uint32_t nowMs) const {
  if (finished_) return 255;
  if (static_cast<int32_t>(nowMs - jingleStartsAtMs_) <= 0) return 0;
  const uint32_t elapsed = nowMs - jingleStartsAtMs_;
  return static_cast<uint8_t>(std::min<uint32_t>(255, elapsed * 255UL / kJingleDurationMs));
}

bool StartupSoundPreview::service(uint32_t nowMs) {
  if (moodSoundPlaying_) {
    if (!due(nowMs, moodSoundEndsAtMs_)) return false;
    M5.Speaker.end();
    moodSoundPlaying_ = false;
    return true;
  }
  if (finished_) return true;
  if (!due(nowMs, nextCueAtMs_)) return false;

  if (phase_ == Phase::PcmJingle) {
    M5.Speaker.setVolume(previewVolume(190));
    if (pcmJingle_ != nullptr &&
        M5.Speaker.playRaw(pcmJingle_, pcmJingleSize_, kPcmSampleRate, false, 1, 0, true)) {
      phase_ = Phase::PcmJingleWait;
      nextCueAtMs_ = nowMs + kJingleDurationMs;
      return false;
    }
    M5.Speaker.end();
    finished_ = true;
    return true;
  }

  if (phase_ == Phase::PcmJingleWait) {
    M5.Speaker.end();
    finished_ = true;
    return true;
  }

  finished_ = true;
  M5.Speaker.end();
  return true;
}

void StartupSoundPreview::prepareSounds() {
  const uint32_t startedUs = micros();
  while (creatureSounds_.prepare(kPrepareChunkSamples) &&
         micros() - startedUs < kPrepareBudgetUs) {
  }
}

bool StartupSoundPreview::playCreatureSound(uint8_t sound, uint8_t fullVolume) {
  // A sound that is not rendered yet is dropped, like one that meets a busy
  // speaker.
  if (!availableForMoodSound() || !creatureSounds_.ready(sound)) return false;
  if (!M5.Speaker.begin()) return false;
  const size_t sampleCount = creatureSounds_.samples(sound);
  M5.Speaker.setVolume(previewVolume(fullVolume));
  if (!M5.Speaker.playRaw(creatureSounds_.data(sound), sampleCount, kPcmSampleRate, false, 1, 0,
                          true)) {
    M5.Speaker.end();
    return false;
  }
  moodSoundEndsAtMs_ = millis() + sampleCount * 1000UL / kPcmSampleRate;
  moodSoundPlaying_ = true;
  return true;
}

bool StartupSoundPreview::playMoodSound(const ThresholdCrossingEvent& event) {
  const int phrase = phraseForEvent(event);
  return phrase >= 0 && playCreatureSound(static_cast<uint8_t>(phrase), 190);
}

bool StartupSoundPreview::playSleepSnore() {
  return playCreatureSound(kSleepSnoreSound, 120);
}

}  // namespace decaflash::mainframe
