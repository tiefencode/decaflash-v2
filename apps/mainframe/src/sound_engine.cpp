#include "sound_engine.h"

#include <algorithm>
#include <cmath>

#include <M5Unified.h>
#include <esp_heap_caps.h>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kPcmSampleRate = 44100;
constexpr uint32_t kJingleDurationMs = 2400;
constexpr uint32_t kCreatureMaxDurationMs = 1500;
constexpr size_t kJingleSamples = kPcmSampleRate * kJingleDurationMs / 1000;
constexpr size_t kCreatureMaxSamples = kPcmSampleRate * kCreatureMaxDurationMs / 1000;
constexpr float kTwoPi = 6.283185307f;

constexpr uint16_t kCreatureDurationsMs[] = {
    620, 1320, 1050, 400, 500, 500, 1280, 1150, 1400,
};
constexpr uint16_t kSleepSnoreDurationMs = 1250;

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

float clipped(float value) {
  return std::max(-1.0f, std::min(1.0f, value));
}

float square(float phase) {
  return std::sin(kTwoPi * phase) >= 0.0f ? 1.0f : -1.0f;
}

float saw(float phase) {
  return 2.0f * (phase - std::floor(phase + 0.5f));
}

float envelope(float progress, float attack = 0.08f, float release = 0.22f) {
  if (progress <= 0.0f || progress >= 1.0f) return 0.0f;
  return std::min(1.0f, std::min(progress / attack, (1.0f - progress) / release));
}

float chipNote(float time, float start, float duration, float startHz, float endHz,
               float gain, uint8_t waveform = 0) {
  if (time < start || time >= start + duration) return 0.0f;
  const float progress = (time - start) / duration;
  const float phase = startHz * (time - start) +
      0.5f * (endHz - startHz) * duration * progress * progress;
  float wave = std::sin(kTwoPi * phase);
  if (waveform == 1) wave = square(phase);
  if (waveform == 2) wave = saw(phase);
  return wave * envelope(progress) * gain;
}

void encodeSample(uint8_t* output, size_t index, uint32_t& noise, float mixed,
                  float& held, uint8_t levels, uint8_t holdSamples) {
  if (index % holdSamples == 0) {
    held = std::round(clipped(mixed) * levels) / levels;
  }
  noise = noise * 1664525U + 1013904223U;
  const float dither = (static_cast<float>(noise >> 8) / 16777215.0f - 0.5f) / 30.0f;
  output[index] = static_cast<uint8_t>(
      std::round((clipped(held + dither) + 1.0f) * 127.5f));
}

void buildPcmJingle(uint8_t* output) {
  constexpr float beatSeconds = 0.30f;
  constexpr float bassNotes[] = {
      130.81f, 130.81f, 110.00f, 110.00f, 174.61f, 174.61f, 196.00f, 196.00f,
  };
  constexpr float leadNotes[] = {
      659.25f, 783.99f, 880.00f, 783.99f, 659.25f, 587.33f, 659.25f, 783.99f,
      880.00f, 1046.50f, 880.00f, 783.99f, 739.99f, 880.00f, 783.99f, 1046.50f,
  };
  uint32_t noise = 0xC0DEF00DU;
  float held = 0.0f;
  for (size_t index = 0; index < kJingleSamples; ++index) {
    const float time = static_cast<float>(index) / kPcmSampleRate;
    const float beatPosition = std::fmod(time, beatSeconds);
    const uint8_t beat = static_cast<uint8_t>(time / beatSeconds) & 7U;
    const float kickProgress = std::min(1.0f, beatPosition / 0.14f);
    const float kick = std::sin(kTwoPi * (145.0f - 78.0f * kickProgress) * beatPosition) *
      std::exp(-beatPosition * 26.0f) * 0.62f;
    const float bass = square(bassNotes[beat] * time) * std::exp(-beatPosition * 5.5f) * 0.17f;
    const uint8_t leadIndex = std::min<uint8_t>(
      sizeof(leadNotes) / sizeof(leadNotes[0]) - 1, static_cast<uint8_t>(time / 0.15f));
    const float leadProgress = std::fmod(time, 0.15f);
    const float leadEnvelope = std::min(1.0f, leadProgress * 55.0f) *
      std::max(0.0f, 1.0f - leadProgress * 5.5f);
    const float lead = square(leadNotes[leadIndex] * time) * leadEnvelope * 0.26f;
    constexpr float arpeggio[] = {261.63f, 329.63f, 392.00f, 220.00f, 261.63f, 329.63f,
                                   174.61f, 220.00f, 261.63f, 196.00f, 246.94f, 293.66f};
    const uint8_t arpeggioIndex = static_cast<uint8_t>(time / 0.10f) %
      (sizeof(arpeggio) / sizeof(arpeggio[0]));
    const float arpPosition = std::fmod(time, 0.10f);
    const float arp = square(arpeggio[arpeggioIndex] * time) *
      std::exp(-arpPosition * 15.0f) * 0.075f;
    noise = noise * 1664525U + 1013904223U;
    const float random = static_cast<float>(noise >> 8) / 16777215.0f * 2.0f - 1.0f;
    const float hatPosition = std::fmod(time + beatSeconds * 0.5f, beatSeconds);
    const float hat = hatPosition < 0.025f ? random * (1.0f - hatPosition / 0.025f) * 0.12f : 0.0f;
    encodeSample(output, index, noise, kick + bass + lead + arp + hat, held, 31, 2);
  }
}

size_t buildCreaturePhrase(uint8_t phrase, uint8_t* output) {
  if (phrase >= sizeof(kCreatureDurationsMs) / sizeof(kCreatureDurationsMs[0])) return 0;
  const size_t samples = kPcmSampleRate * kCreatureDurationsMs[phrase] / 1000;
  uint32_t noise = 0xA11CE000U + phrase;
  float held = 0.0f;
  for (size_t index = 0; index < samples; ++index) {
    const float time = static_cast<float>(index) / kPcmSampleRate;
    float mixed = 0.0f;
    switch (phrase) {
      case 0:  // Energy up: "juHUU!" — low, then a bright emphatic jump.
        mixed = chipNote(time, 0.00f, .15f, 380, 455, .20f) +
                chipNote(time, .18f, .37f, 740, 1120, .66f, 1) +
                chipNote(time, .24f, .25f, 1480, 1740, .12f, 1);
        break;
      case 1: { // Energy down: a sleepy yawn resolving into a disappointed "ooou".
        const float yawn = chipNote(time, .02f, .20f, 116, 142, .18f) +
          chipNote(time, .16f, .66f, 142, 274, .47f) +
          chipNote(time, .21f, .59f, 284, 514, .13f) +
          chipNote(time, .65f, .62f, 424, 248, .29f) +
          chipNote(time, .67f, .55f, 848, 496, .09f) +
          chipNote(time, .96f, .26f, 210, 86, .26f, 2);
        noise = noise * 1664525U + 1013904223U;
        const float breath = (static_cast<float>(noise >> 8) / 16777215.0f * 2.0f - 1.0f) *
          ((time > .08f && time < .82f) ? .035f : 0.0f);
        mixed = yawn + breath;
        break;
      }
      case 2: { // Annoyance up: aggressive "mep-mep".
        const float mep = chipNote(time, .00f, .16f, 250, 175, .55f, 2) +
          chipNote(time, .23f, .18f, 265, 150, .58f, 2) +
          chipNote(time, .47f, .11f, 122, 94, .48f, 2) +
          chipNote(time, .61f, .10f, 116, 88, .48f, 2) +
          chipNote(time, .74f, .10f, 110, 82, .48f, 2) +
          chipNote(time, .87f, .12f, 104, 72, .52f, 2);
        noise = noise * 1664525U + 1013904223U;
        const bool growl = (time >= .47f && time < .58f) ||
          (time >= .61f && time < .71f) || (time >= .74f && time < .84f) ||
          (time >= .87f && time < .99f);
        const float grit = (static_cast<float>(noise >> 8) / 16777215.0f * 2.0f - 1.0f) *
          (time < .16f || (time >= .23f && time < .41f) || growl ? .17f : 0.0f);
        mixed = mep + grit;
        break;
      }
      case 3:  // Annoyance down: an unambiguous, confident "mm-HM!".
        mixed = chipNote(time, .02f, .14f, 128, 140, .40f) +
                chipNote(time, .17f, .19f, 178, 244, .55f) +
                chipNote(time, .21f, .13f, 356, 430, .13f, 1);
        break;
      case 4:  // Attention up: friendly, curious "hm?"
        mixed = chipNote(time, .02f, .18f, 470, 520, .31f) +
                chipNote(time, .18f, .27f, 520, 770, .52f) +
                chipNote(time, .27f, .16f, 770, 850, .12f, 1);
        break;
      case 5:  // Attention down: questioning "oh?"
        mixed = chipNote(time, .03f, .29f, 365, 455, .47f) +
                chipNote(time, .20f, .20f, 455, 600, .20f);
        break;
      case 6:  // Loneliness up: recorded "ooou" contour, high and gently falling.
        // The voice reference holds near 420 Hz, then glides to about 250 Hz.
        // Its upper components start open like "o" and settle lower into "u".
        mixed = chipNote(time, .02f, 1.03f, 424, 248, .42f) +
                chipNote(time, .02f, 1.03f, 848, 496, .15f) +
                chipNote(time, .03f, .54f, 1272, 820, .075f) +
                chipNote(time, .48f, .55f, 820, 615, .10f);
        break;
      case 7:  // Depression up: a small whistleable thinking melody.
        mixed = chipNote(time, .02f, .18f, 330, 330, .30f) +
                chipNote(time, .23f, .16f, 392, 392, .32f) +
                chipNote(time, .42f, .16f, 330, 330, .31f) +
                chipNote(time, .61f, .18f, 440, 440, .34f) +
                chipNote(time, .83f, .25f, 392, 330, .38f);
        break;
      default: { // Depression down: an original harmonic pop resolution.
        mixed = chipNote(time, .00f, .16f, 523, 523, .30f, 1) +
                chipNote(time, .18f, .16f, 659, 659, .32f, 1) +
                chipNote(time, .36f, .18f, 784, 784, .35f, 1) +
                chipNote(time, .57f, .16f, 880, 880, .34f, 1) +
                chipNote(time, .76f, .22f, 1047, 784, .38f, 1) +
                chipNote(time, 1.03f, .25f, 880, 1047, .40f, 1);
        const float chordTime = std::fmod(time, .35f);
        const uint8_t chord = static_cast<uint8_t>(time / .35f) & 3U;
        constexpr float roots[] = {130.81f, 110.00f, 174.61f, 196.00f};
        mixed += square(roots[chord] * time) * std::exp(-chordTime * 4.5f) * .13f;
        mixed += std::sin(kTwoPi * (140.0f - chordTime * 180.0f) * chordTime) *
          std::exp(-chordTime * 25.0f) * .18f;
        break;
      }
    }
    const uint8_t levels = phrase == 2 ? 7 : (phrase == 0 || phrase >= 6 ? 31 : 15);
    const uint8_t holdSamples = phrase == 2 ? 7 : (phrase == 0 || phrase >= 6 ? 2 : 4);
    encodeSample(output, index, noise, mixed, held, levels, holdSamples);
  }
  return samples;
}

size_t buildSleepSnore(uint8_t* output) {
  const size_t samples = kPcmSampleRate * kSleepSnoreDurationMs / 1000;
  uint32_t noise = 0x5A0E0001U;
  float held = 0.0f;
  for (size_t index = 0; index < samples; ++index) {
    const float time = static_cast<float>(index) / kPcmSampleRate;
    const float breath = chipNote(time, .03f, 1.12f, 73, 59, .22f) +
      chipNote(time, .06f, .94f, 146, 118, .075f) +
      chipNote(time, .72f, .28f, 54, 47, .11f, 2);
    encodeSample(output, index, noise, breath, held, 31, 3);
  }
  return samples;
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
  if (pcmCreature_ == nullptr) {
    pcmCreature_ = static_cast<uint8_t*>(
      heap_caps_malloc(kCreatureMaxSamples, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pcmCreature_ != nullptr) pcmCreatureCapacity_ = kCreatureMaxSamples;
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

bool StartupSoundPreview::playMoodSound(const ThresholdCrossingEvent& event) {
  const int phrase = phraseForEvent(event);
  if (!availableForMoodSound() || phrase < 0 || pcmCreature_ == nullptr) return false;
  const size_t sampleCount = buildCreaturePhrase(static_cast<uint8_t>(phrase), pcmCreature_);
  if (sampleCount == 0 || !M5.Speaker.begin()) return false;
  M5.Speaker.setVolume(previewVolume(190));
  if (!M5.Speaker.playRaw(pcmCreature_, sampleCount, kPcmSampleRate, false, 1, 0, true)) {
    M5.Speaker.end();
    return false;
  }
  moodSoundEndsAtMs_ = millis() + sampleCount * 1000UL / kPcmSampleRate;
  moodSoundPlaying_ = true;
  return true;
}

bool StartupSoundPreview::playSleepSnore() {
  if (!availableForMoodSound() || pcmCreature_ == nullptr) return false;
  const size_t sampleCount = buildSleepSnore(pcmCreature_);
  if (!M5.Speaker.begin()) return false;
  M5.Speaker.setVolume(previewVolume(120));
  if (!M5.Speaker.playRaw(pcmCreature_, sampleCount, kPcmSampleRate, false, 1, 0, true)) {
    M5.Speaker.end();
    return false;
  }
  moodSoundEndsAtMs_ = millis() + sampleCount * 1000UL / kPcmSampleRate;
  moodSoundPlaying_ = true;
  return true;
}

}  // namespace decaflash::mainframe
