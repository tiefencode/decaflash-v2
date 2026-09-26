#include "iris_vu.h"
#include <cassert>
#include <cstdint>
#include <iostream>
using decaflash::mainframe::IrisVu;
int main() {
  IrisVu vu;
  assert(vu.level(0) == 0);
  vu.feed(16, IrisVu::kNoiseGate);
  assert(vu.level(16) == 0);
  uint8_t previous = 0;
  for (uint32_t input = IrisVu::kNoiseGate + 1; input <= IrisVu::kFullLevel; ++input) {
    vu.feed(32, input);
    assert(vu.level(32) >= previous);
    previous = vu.level(32);
  }
  assert(previous == 255);
  assert(IrisVu::litFacets(0) == 0);
  assert(IrisVu::litFacets(1) == 1);
  assert(IrisVu::litFacets(255) == IrisVu::kFacets);
  vu.feed(48, UINT32_MAX);
  assert(vu.level(48) == 255);
  for (uint32_t i = 1; i <= 10; ++i) vu.feed(48 + i * 16, 0);
  assert(vu.level(208) == 0);
  vu.feed(UINT32_MAX - 100, IrisVu::kFullLevel);
  assert(vu.level(20) == 255); // millis wraparound
  assert(vu.level(149) == 0); // lost capture must not leave a lit iris
  using decaflash::mainframe::IrisFacets;
  IrisFacets facets;
  for (uint32_t t = 0; t < 1000; t += 33) facets.update(t, 255);
  uint8_t lowest = 255;
  uint8_t highest = 0;
  for (uint8_t i = 0; i < IrisVu::kFacets; ++i) {
    lowest = facets.level(i) < lowest ? facets.level(i) : lowest;
    highest = facets.level(i) > highest ? facets.level(i) : highest;
  }
  assert(lowest == 255 && highest == 255);
  assert(facets.displayLevel(0, 52) == 255);
  auto rgb = IrisFacets::color(0);
  assert(rgb.r == 0 && rgb.g == 0 && rgb.b == 0);
  rgb = IrisFacets::color(85);
  assert(rgb.r == 8 && rgb.g == 34 && rgb.b == 110);
  rgb = IrisFacets::color(170);
  assert(rgb.r == 40 && rgb.g == 212 && rgb.b == 255);
  rgb = IrisFacets::color(255);
  assert(rgb.r == 255 && rgb.g == 30 && rgb.b == 230);
  rgb = IrisFacets::color(85, IrisFacets::Profile::Annoyed);
  assert(rgb.r == 145 && rgb.g == 6 && rgb.b == 12);
  rgb = IrisFacets::color(170, IrisFacets::Profile::Annoyed);
  assert(rgb.r == 255 && rgb.g == 14 && rgb.b == 17);
  rgb = IrisFacets::color(255, IrisFacets::Profile::Annoyed);
  assert(rgb.r == 255 && rgb.g == 65 && rgb.b == 5);
  const auto darkPink = IrisFacets::shadedColor(0, 255);
  const auto lightPink = IrisFacets::shadedColor(1, 255);
  assert(darkPink.r != lightPink.r || darkPink.g != lightPink.g || darkPink.b != lightPink.b);
  facets.update(1100, 0);
  for (uint8_t i = 0; i < IrisVu::kFacets; ++i) assert(facets.level(i) == 0);
  facets.update(1133, 64);
  unsigned active = 0, transitions = 0;
  for (uint8_t i = 0; i < IrisVu::kFacets; ++i) {
    active += facets.level(i) > 0;
    if (i && (facets.level(i) > 0) != (facets.level(i - 1) > 0)) ++transitions;
  }
  assert(active == IrisVu::litFacets(64));
  assert(transitions > 10); // Scattered activation, not a contiguous wedge.
  std::cout << "Iris VU tests passed\n";
}
