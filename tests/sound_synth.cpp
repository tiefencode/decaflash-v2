#include "sound_synth.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace decaflash::mainframe;

namespace {

std::vector<uint8_t> renderWhole(uint8_t sound) {
  std::vector<uint8_t> pcm(creatureSoundSamples(sound));
  CreatureSoundRenderer renderer;
  renderer.start(sound);
  assert(renderer.render(pcm.data(), pcm.size()) == pcm.size());
  assert(renderer.done());
  return pcm;
}

}  // namespace

int main() {
  // Lengths follow the durations in the sound spec at 44.1 kHz.
  const size_t expectedSamples[kCreatureSoundCount] = {
    27342, 58212, 46305, 17640, 22050, 22050, 56448, 50715, 61740, 55125};
  size_t total = 0;
  for (uint8_t sound = 0; sound < kCreatureSoundCount; ++sound) {
    assert(creatureSoundSamples(sound) == expectedSamples[sound]);
    total += expectedSamples[sound];
  }
  assert(creatureSoundSamples(kCreatureSoundCount) == 0);
  assert(CreatureSoundBank::requiredBytes() == total);

  // Rendering in pieces gives the same bytes as rendering in one go, for
  // piece sizes that do and do not divide the sample-hold lengths.
  for (uint8_t sound = 0; sound < kCreatureSoundCount; ++sound) {
    const auto whole = renderWhole(sound);
    bool hasSignal = false;
    for (uint8_t value : whole) hasSignal = hasSignal || value < 100 || value > 155;
    assert(hasSignal);
    for (size_t piece : {size_t{1}, size_t{32}, size_t{1000}}) {
      if (piece == 1 && sound != 2 && sound != 4) continue;  // keep the test fast
      std::vector<uint8_t> pieces(whole.size());
      CreatureSoundRenderer renderer;
      renderer.start(sound);
      size_t at = 0;
      while (!renderer.done()) at += renderer.render(pieces.data() + at, piece);
      assert(at == whole.size());
      assert(renderer.render(pieces.data(), piece) == 0);
      assert(pieces == whole);
    }
  }

  // Without memory nothing is ready and there is no work.
  CreatureSoundBank none;
  none.attach(nullptr);
  assert(none.allReady() && !none.prepare(32) && !none.ready(4) && none.data(4) == nullptr);

  // The bank renders a bounded piece per call, makes each sound ready as soon
  // as it is complete, and starts with the attention phrase.
  std::vector<uint8_t> memory(CreatureSoundBank::requiredBytes() + 1, 0xEE);
  CreatureSoundBank bank;
  bank.attach(memory.data());
  assert(!bank.allReady());
  for (uint8_t sound = 0; sound < kCreatureSoundCount; ++sound) assert(!bank.ready(sound));
  size_t calls = 0;
  while (!bank.ready(4)) {
    assert(bank.prepare(32));
    ++calls;
  }
  assert(calls == (22050 + 31) / 32);
  for (uint8_t sound = 0; sound < kCreatureSoundCount; ++sound) {
    assert(bank.ready(sound) == (sound == 4));
  }
  while (bank.prepare(32)) ++calls;
  assert(bank.allReady() && !bank.prepare(32));
  for (uint8_t sound = 0; sound < kCreatureSoundCount; ++sound) {
    assert(bank.ready(sound));
    const auto whole = renderWhole(sound);
    assert(bank.samples(sound) == whole.size());
    assert(std::memcmp(bank.data(sound), whole.data(), whole.size()) == 0);
  }
  assert(memory.back() == 0xEE);  // nothing written past the bank
  assert(!bank.ready(kCreatureSoundCount) && bank.data(kCreatureSoundCount) == nullptr);

  std::cout << "sound_synth: ok\n";
  return 0;
}
