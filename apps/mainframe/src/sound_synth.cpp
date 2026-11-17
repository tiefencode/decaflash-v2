#include "sound_synth.h"

#include <algorithm>
#include <cmath>

namespace decaflash::mainframe {
namespace {

constexpr float kTwoPi = 6.283185307f;

constexpr uint16_t kCreatureDurationsMs[kCreatureSoundCount] = {
    620, 1320, 1050, 400, 500, 500, 1280, 1150, 1400,
    1250,  // sleep snore
};

// Sounds heard soonest after boot come first: the attention phrases and
// loneliness, then the other moods; the snore needs minutes of creature mode.
constexpr uint8_t kPreparationOrder[kCreatureSoundCount] = {4, 5, 6, 0, 1, 2, 3, 7, 8,
                                                            kSleepSnoreSound};

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

// index counts from the start of the sound, whatever piece is being rendered.
uint8_t encodeSample(size_t index, uint32_t& noise, float mixed, float& held, uint8_t levels,
                     uint8_t holdSamples) {
  if (index % holdSamples == 0) {
    held = std::round(clipped(mixed) * levels) / levels;
  }
  noise = noise * 1664525U + 1013904223U;
  const float dither = (static_cast<float>(noise >> 8) / 16777215.0f - 0.5f) / 30.0f;
  return static_cast<uint8_t>(std::round((clipped(held + dither) + 1.0f) * 127.5f));
}

}  // namespace

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
    output[index] = encodeSample(index, noise, kick + bass + lead + arp + hat, held, 31, 2);
  }
}


namespace {

// One unquantised sample of a creature sound. noise is the sound's running
// generator; some phrases draw from it before the encoder does.
float creatureSample(uint8_t phrase, float time, uint32_t& noise) {
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
  return mixed;
}

float snoreSample(float time) {
  return chipNote(time, .03f, 1.12f, 73, 59, .22f) +
    chipNote(time, .06f, .94f, 146, 118, .075f) +
    chipNote(time, .72f, .28f, 54, 47, .11f, 2);
}

}  // namespace

size_t creatureSoundSamples(uint8_t sound) {
  if (sound >= kCreatureSoundCount) return 0;
  return kPcmSampleRate * kCreatureDurationsMs[sound] / 1000;
}

void CreatureSoundRenderer::start(uint8_t sound) {
  sound_ = sound;
  index_ = 0;
  total_ = creatureSoundSamples(sound);
  noise_ = sound == kSleepSnoreSound ? 0x5A0E0001U : 0xA11CE000U + sound;
  held_ = 0.0f;
}

size_t CreatureSoundRenderer::render(uint8_t* output, size_t maxSamples) {
  const size_t count = std::min(maxSamples, total_ - index_);
  const bool snore = sound_ == kSleepSnoreSound;
  const uint8_t levels = snore ? 31 : (sound_ == 2 ? 7 : (sound_ == 0 || sound_ >= 6 ? 31 : 15));
  const uint8_t holdSamples = snore ? 3 : (sound_ == 2 ? 7 : (sound_ == 0 || sound_ >= 6 ? 2 : 4));
  for (size_t written = 0; written < count; ++written, ++index_) {
    const float time = static_cast<float>(index_) / kPcmSampleRate;
    const float mixed = snore ? snoreSample(time) : creatureSample(sound_, time, noise_);
    output[written] = encodeSample(index_, noise_, mixed, held_, levels, holdSamples);
  }
  return count;
}

namespace {

size_t soundOffset(uint8_t sound) {
  size_t offset = 0;
  for (uint8_t before = 0; before < sound; ++before) offset += creatureSoundSamples(before);
  return offset;
}

}  // namespace

size_t CreatureSoundBank::requiredBytes() {
  size_t bytes = 0;
  for (uint8_t sound = 0; sound < kCreatureSoundCount; ++sound) {
    bytes += creatureSoundSamples(sound);
  }
  return bytes;
}

void CreatureSoundBank::attach(uint8_t* memory) {
  memory_ = memory;
  next_ = 0;
  rendering_ = false;
  for (bool& ready : ready_) ready = false;
}

const uint8_t* CreatureSoundBank::data(uint8_t sound) const {
  if (memory_ == nullptr || sound >= kCreatureSoundCount) return nullptr;
  return memory_ + soundOffset(sound);
}

bool CreatureSoundBank::prepare(size_t maxSamples) {
  if (allReady()) return false;
  const uint8_t sound = kPreparationOrder[next_];
  if (!rendering_) {
    renderer_.start(sound);
    rendered_ = 0;
    rendering_ = true;
  }
  rendered_ += renderer_.render(memory_ + soundOffset(sound) + rendered_, maxSamples);
  if (renderer_.done()) {
    ready_[sound] = true;
    rendering_ = false;
    ++next_;
  }
  return !allReady();
}

}  // namespace decaflash::mainframe
