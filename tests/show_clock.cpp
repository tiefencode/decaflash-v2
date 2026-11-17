#include <cassert>

#include "show_clock.h"

using decaflash::mainframe::advanceShowBeat;
using decaflash::mainframe::takeDueShowBeat;

namespace {

// One loop pass of the show clock at now, as serviceShowClock() runs it.
// Returns the number of beats counted; sync says whether the pass ended on
// beat 1 of a bar, which is when the Mainframe sends ClockSync.
uint32_t servicePass(uint32_t now, uint32_t& nextBeatAtMs, uint32_t intervalMs,
                     uint8_t& beat, uint32_t& bar, bool& sync) {
  uint32_t beats = 0;
  sync = false;
  while (takeDueShowBeat(now, nextBeatAtMs, intervalMs)) {
    sync = advanceShowBeat(beat, bar, 4);
    ++beats;
  }
  return beats;
}

}  // namespace

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

  constexpr uint32_t kInterval = 500;  // 120 BPM
  bool sync = false;

  // A beat is not due before its time and leaves the schedule alone.
  uint32_t next = 1500;
  assert(!takeDueShowBeat(1499, next, kInterval));
  assert(next == 1500);

  // Late loop passes do not shift later beats (BUG-028): every beat is noticed
  // 1 to 44 ms late, yet beat n stays scheduled at start + n * interval.
  beat = 1;
  bar = 1;
  next = 1000 + kInterval;
  uint32_t counted = 0;
  for (uint32_t n = 1; n <= 240; ++n) {
    const uint32_t due = 1000 + n * kInterval;
    assert(next == due);
    assert(servicePass(due - 1, next, kInterval, beat, bar, sync) == 0);
    const uint32_t late = 1 + (n * 7) % 44;
    assert(servicePass(due + late, next, kInterval, beat, bar, sync) == 1);
    assert(sync == (n % 4 == 0));
    ++counted;
  }
  assert(counted == 240);
  assert(next == 1000 + 241 * kInterval);
  assert(beat == 1);
  assert(bar == 61);

  // A pass that comes whole beats late counts every missed beat at once and
  // stays on the grid. Beats 2, 3 and 4 were missed and the pass ends on beat 1
  // of the next bar, so that beat sends ClockSync.
  beat = 1;
  bar = 1;
  next = 1500;
  assert(servicePass(3010, next, kInterval, beat, bar, sync) == 4);
  assert(beat == 1);
  assert(bar == 2);
  assert(sync);
  assert(next == 3500);
  assert(servicePass(3499, next, kInterval, beat, bar, sync) == 0);

  // If the missed beats run past a bar start, the position still counts them
  // all, but no ClockSync goes out: the beat that starts now is not beat 1.
  beat = 3;
  bar = 7;
  next = 1500;
  assert(servicePass(2600, next, kInterval, beat, bar, sync) == 3);
  assert(beat == 2);
  assert(bar == 8);
  assert(!sync);
  assert(next == 3000);

  // The schedule survives the wrap of the millisecond counter.
  beat = 1;
  bar = 1;
  next = 0xFFFFFF00u;
  assert(servicePass(0xFFFFFEFFu, next, kInterval, beat, bar, sync) == 0);
  assert(servicePass(0xFFFFFF10u, next, kInterval, beat, bar, sync) == 1);
  assert(next == 0xFFFFFF00u + kInterval);  // wrapped past zero
  assert(next < kInterval);
  assert(servicePass(next - 1, next, kInterval, beat, bar, sync) == 0);
  assert(servicePass(next + 3, next, kInterval, beat, bar, sync) == 1);
  assert(beat == 3);
}
