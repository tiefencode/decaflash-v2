#pragma once
#include <cstdint>
#include <cstddef>
namespace decaflash::mainframe {
// Fixed 16 kHz PCM input. Broad bass estimate, not an FFT or genre classifier.
class AudioMoodFeatures {
 public:
  void feed(uint32_t now, const int16_t* samples, size_t count);
  bool silent() const { return silent_; }
  bool bassValid() const { return validSamples_ >= 240000; }
  uint16_t bassPermille() const;
 private:
  float dc_ = 0, low1_ = 0, low2_ = 0;
  float totalPower_ = 0, bassPower_ = 0;
  uint32_t validSamples_ = 0, lastAt_ = 0;
  bool seen_ = false, silent_ = false;
};
}
