#pragma once

#include <cstdint>

namespace decaflash::mainframe {

// Display-only envelope, fed once per 256-sample / 16 ms capture block.
// Absolute thresholds need a listening check with the actual codec gain.
class IrisVu {
 public:
  static constexpr uint8_t kFacets = 36;
  static constexpr uint32_t kNoiseGate = 80;
  static constexpr uint32_t kFullLevel = 2400;
  static constexpr uint32_t kStaleMs = 250;

  void feed(uint32_t now, uint32_t level) {
    const uint32_t target = level <= kNoiseGate ? 0 :
      (level >= kFullLevel ? 255 :
       (level - kNoiseGate) * 255 / (kFullLevel - kNoiseGate));
    // Immediate attack, ~160 ms release to black, independent of display FPS.
    envelope_ = target >= envelope_ ? target :
      (envelope_ > 26 ? envelope_ - 26 : 0);
    if (envelope_ < target) envelope_ = target;
    lastBlockAtMs_ = now;
  }

  uint8_t level(uint32_t now) const {
    return now - lastBlockAtMs_ >= kStaleMs ? 0 : envelope_;
  }

  static uint8_t litFacets(uint8_t level) {
    return level == 0 ? 0 : (static_cast<uint16_t>(level) * kFacets + 254) / 255;
  }

 private:
  uint32_t lastBlockAtMs_ = 0;
  uint8_t envelope_ = 0;
};

// Brain matrix behaviour: shuffled activation, gentle drift, per-facet fades.
// Updated only on display frames; no shared Arduino RNG or audio state.
class IrisFacets {
 public:
  struct Rgb { uint8_t r, g, b; };
  IrisFacets() {
    for (uint8_t i = 0; i < IrisVu::kFacets; ++i) order_[i] = i;
    for (uint8_t i = IrisVu::kFacets - 1; i > 0; --i) swap(i, random(i + 1));
  }
  void update(uint32_t now, uint8_t level) {
    const uint8_t filled = IrisVu::litFacets(level);
    if (now - lastDrift_ >= 220) {
      lastDrift_ = now;
      if (filled > 0 && filled < IrisVu::kFacets) {
        // Four swaps retain the old meter's scattered activation.
        for (uint8_t i = 0; i < 4; ++i)
          swap(random(filled), filled + random(IrisVu::kFacets - filled));
      }
    }
    for (uint8_t slot = 0; slot < IrisVu::kFacets; ++slot) {
      auto& value = levels_[order_[slot]];
      if (level == 0) value = 0;
      else if (slot < filled) value = value > 237 ? 255 : value + 18;
      else value = value > 14 ? value - 14 : 0;
    }
  }
  uint8_t level(uint8_t facet) const { return levels_[facet]; }
  uint8_t displayLevel(uint8_t facet, uint8_t baseLevel) const {
    (void)facet;
    return levels_[facet] > baseLevel ? levels_[facet] : baseLevel;
  }
  static Rgb color(uint8_t value) {
    if (value <= 85) return {
      static_cast<uint8_t>(8U * value / 85),
      static_cast<uint8_t>(34U * value / 85),
      static_cast<uint8_t>(110U * value / 85)};
    if (value <= 170) {
      const uint16_t mix = value - 85;
      return {static_cast<uint8_t>(8 + 32U * mix / 85),
              static_cast<uint8_t>(34 + 178U * mix / 85),
              static_cast<uint8_t>(110 + 145U * mix / 85)};
    }
    const uint16_t mix = value - 170;
    return {static_cast<uint8_t>(40 + 215U * mix / 85),
            static_cast<uint8_t>(212 - 182U * mix / 85),
            static_cast<uint8_t>(255 - 25U * mix / 85)};
  }
  static Rgb shadedColor(uint8_t facet, uint8_t value) {
    const Rgb base = color(value);
    const int16_t shade = static_cast<int16_t>(facetHash(facet) % 51U) - 25;
    return {shadeChannel(base.r, shade), shadeChannel(base.g, shade),
            shadeChannel(base.b, shade)};
  }
 private:
  static uint8_t facetHash(uint8_t facet) {
    uint8_t value = static_cast<uint8_t>(facet * 73U + 29U);
    value ^= value >> 3U;
    value = static_cast<uint8_t>(value * 47U + 11U);
    return value;
  }
  static uint8_t shadeChannel(uint8_t channel, int16_t shade) {
    if (shade < 0) {
      return static_cast<uint8_t>(static_cast<int16_t>(channel) * (100 + shade) / 100);
    }
    return static_cast<uint8_t>(channel +
      (static_cast<uint16_t>(255 - channel) * shade / 100));
  }
  uint8_t random(uint8_t bound) {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
    return rng_ % bound;
  }
  void swap(uint8_t a, uint8_t b) {
    const uint8_t saved = order_[a]; order_[a] = order_[b]; order_[b] = saved;
  }
  uint32_t rng_ = 0x49524953;
  uint32_t lastDrift_ = 0;
  uint8_t order_[IrisVu::kFacets] = {};
  uint8_t levels_[IrisVu::kFacets] = {};
};

}  // namespace decaflash::mainframe
