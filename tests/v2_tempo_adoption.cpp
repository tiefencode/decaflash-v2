#include <cassert>

#include "v2_tempo_adoption.h"

int main() {
  decaflash::mainframe::V2TempoAdoption adoption;
  constexpr uint32_t kNow = 10000;
  constexpr uint32_t kOnset = 9800;

  // A lone 134-BPM transition outlier cannot change a 101-BPM show.
  assert(adoption.observe(1, 101, 134, 46, kOnset, kNow) == 0);
  assert(adoption.observe(2, 101, 99, 30, kOnset, kNow) == 0);
  assert(adoption.observe(3, 101, 99, 31, kOnset, kNow) == 99);

  // Re-reading the same 250-ms evaluation cannot accelerate confirmation.
  assert(adoption.observe(4, 99, 120, 40, kOnset, kNow) == 0);
  assert(adoption.observe(4, 99, 120, 40, kOnset, kNow) == 0);
  assert(adoption.observe(5, 99, 121, 42, kOnset, kNow) == 121);

  // A low-confidence sample clears a pending candidate instead of extending it.
  assert(adoption.observe(6, 121, 100, 40, kOnset, kNow) == 0);
  assert(adoption.observe(7, 121, 100, 29, kOnset, kNow) == 0);
  assert(adoption.observe(8, 121, 100, 40, kOnset, kNow) == 0);
  assert(adoption.observe(9, 121, 100, 40, kOnset, kNow) == 100);
}
