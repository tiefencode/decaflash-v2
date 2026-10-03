#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace decaflash::mainframe {

// A bounded spectral feature extractor for one 256-sample, 16 kHz audio
// block. It keeps only filter state and previous band energy; no PCM history.
class SpectralOnsetFeatures {
 public:
  static constexpr size_t kBandCount = 12;

  struct Frame {
    std::array<uint32_t, kBandCount> energy{};
    std::array<uint32_t, kBandCount> risingEnergy{};
  };

  void reset();
  void beginBlock();
  void feedSample(int32_t centeredSample);
  Frame finishBlock();

 private:
  std::array<int64_t, kBandCount> state1_{};
  std::array<int64_t, kBandCount> state2_{};
  std::array<uint32_t, kBandCount> previousEnergy_{};
};

}  // namespace decaflash::mainframe
