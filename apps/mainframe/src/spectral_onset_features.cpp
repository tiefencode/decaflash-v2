#include "spectral_onset_features.h"

#include <limits>

namespace decaflash::mainframe {
namespace {

// Q14 values of 2*cos(2*pi*k/256), for 125 Hz through 6 kHz at 16 kHz.
constexpr std::array<int32_t, SpectralOnsetFeatures::kBandCount> kCoefficientsQ14{{
  32729, 32679, 32610, 32413, 31786, 30274,
  27246, 23170, 12540, 0, -12540, -23170,
}};

uint32_t saturateEnergy(uint64_t value) {
  // The Goertzel power is Q0 after this shift and comfortably fits uint32_t
  // for a signed 16-bit, 256-sample input block.
  value >>= 16U;
  return value > std::numeric_limits<uint32_t>::max()
    ? std::numeric_limits<uint32_t>::max() : static_cast<uint32_t>(value);
}

}  // namespace

void SpectralOnsetFeatures::reset() {
  *this = SpectralOnsetFeatures{};
}

void SpectralOnsetFeatures::beginBlock() {
  state1_.fill(0);
  state2_.fill(0);
}

void SpectralOnsetFeatures::feedSample(int32_t centeredSample) {
  for (size_t index = 0; index < kBandCount; ++index) {
    const int64_t next = static_cast<int64_t>(centeredSample) +
      ((static_cast<int64_t>(kCoefficientsQ14[index]) * state1_[index]) >> 14U) -
      state2_[index];
    state2_[index] = state1_[index];
    state1_[index] = next;
  }
}

SpectralOnsetFeatures::Frame SpectralOnsetFeatures::finishBlock() {
  Frame frame;
  for (size_t index = 0; index < kBandCount; ++index) {
    const int64_t first = state1_[index];
    const int64_t second = state2_[index];
    const int64_t cross = (static_cast<int64_t>(kCoefficientsQ14[index]) * first * second) >> 14U;
    const int64_t power = first * first + second * second - cross;
    const uint32_t energy = saturateEnergy(power > 0 ? static_cast<uint64_t>(power) : 0U);
    frame.energy[index] = energy;
    frame.risingEnergy[index] = energy > previousEnergy_[index]
      ? energy - previousEnergy_[index] : 0U;
    previousEnergy_[index] = energy;
  }
  return frame;
}

}  // namespace decaflash::mainframe
