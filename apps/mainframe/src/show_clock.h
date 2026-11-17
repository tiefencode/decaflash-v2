#pragma once

#include <stdint.h>

namespace decaflash::mainframe {

// Moves the show clock on by one beat. beatInBar and bar name the beat that is
// being shown; on return they name the beat that starts now. Returns true when
// that new beat is beat 1 of a bar, so the caller sends ClockSync for exactly
// the beat the Mainframe itself shows as beat 1.
inline bool advanceShowBeat(uint8_t& beatInBar, uint32_t& bar, uint8_t beatsPerBar) {
  if (beatInBar >= beatsPerBar) {
    beatInBar = 1;
    ++bar;
    return true;
  }
  ++beatInBar;
  return false;
}

// Reports whether a show beat is due at now. If one is, the following beat is
// scheduled one interval after the time this beat was due, not after now, so a
// late loop pass does not push later beats back (BUG-028). The caller repeats
// the call until it returns false: a pass that comes one or more whole beats
// late then counts every missed beat and stays on the beat grid. Times are
// millis() values and may wrap. intervalMs must not be 0.
inline bool takeDueShowBeat(uint32_t now, uint32_t& nextBeatAtMs, uint32_t intervalMs) {
  if (static_cast<int32_t>(now - nextBeatAtMs) < 0) return false;
  nextBeatAtMs += intervalMs;
  return true;
}

}  // namespace decaflash::mainframe
