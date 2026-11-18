#include "text_event.h"

namespace decaflash::mainframe {

bool textModelEventFor(const MotionEvent& event, TextModelEvent& out) {
  switch (event.kind) {
    case MotionKind::Move: out = TextModelEvent::Move; return true;
    case MotionKind::Tap: out = TextModelEvent::Tap; return true;
    case MotionKind::MultiTap:
      out = event.count >= 3 ? TextModelEvent::MultiTap : TextModelEvent::DoubleTap;
      return true;
    case MotionKind::Impact: out = TextModelEvent::Impact; return true;
    case MotionKind::Rotate: out = TextModelEvent::Rotate; return true;
    case MotionKind::Reposition: out = TextModelEvent::Reposition; return true;
    case MotionKind::Shake: out = TextModelEvent::Shake; return true;
    case MotionKind::None: return false;
  }
  return false;
}

bool textModelEventFor(const PresenceEvent& event, TextModelEvent& out) {
  switch (event.kind) {
    case PresenceEventKind::ArrivedInFront: out = TextModelEvent::ArrivedInFront; return true;
    case PresenceEventKind::WalkedPast: out = TextModelEvent::WalkedPast; return true;
    case PresenceEventKind::Left: out = TextModelEvent::Left; return true;
    case PresenceEventKind::StandsFar: out = TextModelEvent::StandsFar; return true;
    case PresenceEventKind::Moves: return false;  // no text, no request
    case PresenceEventKind::None: return false;
  }
  return false;
}

}  // namespace decaflash::mainframe
