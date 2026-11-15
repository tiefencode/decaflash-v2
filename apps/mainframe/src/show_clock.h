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

}  // namespace decaflash::mainframe
