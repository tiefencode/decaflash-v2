#pragma once

namespace decaflash::mainframe {

// On-device debug screens. A long press opens or closes debug mode; inside
// debug mode a short press switches between the two screens. Outside debug
// mode the short press belongs to the show (start, next scene).
enum class DebugView { Off, Mood, Presence };

class DebugViewState {
 public:
  DebugView view() const { return view_; }
  bool active() const { return view_ != DebugView::Off; }

  void onHold() { view_ = view_ == DebugView::Off ? DebugView::Mood : DebugView::Off; }

  // Returns true when the click is not consumed by debug mode and the caller
  // should run the normal show action.
  bool onClick() {
    switch (view_) {
      case DebugView::Off: return true;
      case DebugView::Mood: view_ = DebugView::Presence; return false;
      case DebugView::Presence: view_ = DebugView::Mood; return false;
    }
    return false;
  }

 private:
  DebugView view_ = DebugView::Off;
};

}  // namespace decaflash::mainframe
