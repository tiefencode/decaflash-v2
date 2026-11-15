#include <cassert>

#include "show_clock.h"

using decaflash::mainframe::advanceShowBeat;

int main() {
  constexpr uint8_t kBeatsPerBar = 4;

  // Show start: bar 1, beat 1 is shown and its ClockSync is already sent.
  uint8_t beat = 1;
  uint32_t bar = 1;

  // The next three beats are 2, 3 and 4 of bar 1 and send no ClockSync.
  // In particular the beat after the start is not a second beat 1 (BUG-007).
  for (uint8_t expected = 2; expected <= kBeatsPerBar; ++expected) {
    assert(!advanceShowBeat(beat, bar, kBeatsPerBar));
    assert(beat == expected);
    assert(bar == 1);
  }

  // The fifth beat is beat 1 of bar 2: ClockSync goes out on that beat and
  // carries the position the Mainframe shows.
  assert(advanceShowBeat(beat, bar, kBeatsPerBar));
  assert(beat == 1);
  assert(bar == 2);

  // Over many bars ClockSync fires once per bar, always on beat 1.
  uint32_t syncs = 0;
  for (int i = 0; i < 4 * 10; ++i) {
    if (advanceShowBeat(beat, bar, kBeatsPerBar)) {
      ++syncs;
      assert(beat == 1);
    } else {
      assert(beat != 1);
    }
  }
  assert(syncs == 10);
  assert(beat == 1);
  assert(bar == 12);

  // Tempo adoption in the middle of a bar moves to beat 1 of the next bar and
  // sends ClockSync itself; the following beat is beat 2, not beat 1 again.
  beat = 1;  // adopted on beat 3 of bar 20
  bar = 21;
  assert(!advanceShowBeat(beat, bar, kBeatsPerBar));
  assert(beat == 2);
  assert(bar == 21);

  // An out-of-range position wraps to beat 1 of the next bar.
  beat = 9;
  bar = 5;
  assert(advanceShowBeat(beat, bar, kBeatsPerBar));
  assert(beat == 1);
  assert(bar == 6);
}
