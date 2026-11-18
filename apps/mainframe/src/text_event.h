#pragma once

#include <cstdint>

#include "motion_events.h"
#include "presence_decision.h"

namespace decaflash::mainframe {

// The twelve text events: the event condition of a text model request and the
// event index of the text bank (same order as the corpus files).
enum class TextModelEvent : uint8_t {
  Move, Tap, DoubleTap, MultiTap, Impact, Rotate, Reposition, Shake,
  ArrivedInFront, WalkedPast, Left, StandsFar,
};
constexpr uint8_t kTextModelEvents = 12;

// False for events without text (None, Bewegt sich).
bool textModelEventFor(const MotionEvent& event, TextModelEvent& out);
bool textModelEventFor(const PresenceEvent& event, TextModelEvent& out);

}  // namespace decaflash::mainframe
