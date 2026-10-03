#include <cassert>
#include <cmath>
#include <cstdint>

#include "spectral_onset_features.h"

namespace {

constexpr double kPi = 3.14159265358979323846;

void feedTone(decaflash::mainframe::SpectralOnsetFeatures& features, uint16_t bin,
              int16_t amplitude) {
  features.beginBlock();
  for (uint16_t index = 0; index < 256; ++index) {
    const double phase = 2.0 * kPi * static_cast<double>(bin * index) / 256.0;
    features.feedSample(static_cast<int32_t>(std::lround(amplitude * std::sin(phase))));
  }
}

}  // namespace

int main() {
  decaflash::mainframe::SpectralOnsetFeatures features;
  feedTone(features, 16, 1000);  // 1 kHz: sixth configured band.
  const auto first = features.finishBlock();
  assert(first.energy[5] > first.energy[0] * 20U);
  assert(first.energy[5] > first.energy[11] * 20U);

  feedTone(features, 16, 2000);
  const auto louder = features.finishBlock();
  assert(louder.energy[5] > first.energy[5]);
  assert(louder.risingEnergy[5] > 0U);

  feedTone(features, 16, 2000);
  const auto steady = features.finishBlock();
  assert(steady.risingEnergy[5] == 0U);
  return 0;
}
