#include "audio_mood_features.h"
#include <algorithm>
#include <cmath>
namespace decaflash::mainframe {
void AudioMoodFeatures::feed(uint32_t now, const int16_t* samples, size_t count) {
  if (!count) return;
  if (seen_ && now - lastAt_ > 250) {
    dc_ = low1_ = low2_ = totalPower_ = bassPower_ = 0;
    validSamples_ = 0; silent_ = false;
  }
  seen_ = true; lastAt_ = now;
  float total = 0, bass = 0;
  size_t clipped = 0;
  for (size_t i = 0; i < count; ++i) {
    const float raw = samples[i];
    if (raw >= 32700 || raw <= -32700) ++clipped;
    // One-pole DC/high-pass near 40 Hz, two low-pass poles near 180 Hz.
    dc_ += 0.015466f * (raw - dc_);
    const float signal = raw - dc_;
    low1_ += 0.066018f * (signal - low1_);
    low2_ += 0.066018f * (low1_ - low2_);
    total += signal * signal;
    bass += low2_ * low2_;
  }
  total /= count; bass /= count;
  // Absolute RMS hysteresis, intentionally independent of the adaptive music gate.
  silent_ = total < (silent_ ? 120.0f * 120.0f : 80.0f * 80.0f);
  if (silent_ || clipped * 100 > count) {
    validSamples_ = 0; totalPower_ = bassPower_ = 0;
    return;
  }
  if (!validSamples_) { totalPower_ = total; bassPower_ = bass; }
  const float alpha = static_cast<float>(count) / (240000.0f + count);
  totalPower_ += alpha * (total - totalPower_);
  bassPower_ += alpha * (bass - bassPower_);
  validSamples_ = std::min<uint32_t>(240000, validSamples_ + count);
}
uint16_t AudioMoodFeatures::bassPermille() const {
  return totalPower_ > 0 ? static_cast<uint16_t>(
    std::min(1000.0f, std::max(0.0f, bassPower_ * 1000.0f / totalPower_))) : 0;
}
}
