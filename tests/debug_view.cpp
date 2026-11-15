#include "debug_view.h"

#include <cassert>
#include <cstdio>

using namespace decaflash::mainframe;

int main() {
  DebugViewState state;
  assert(state.view() == DebugView::Off);
  assert(!state.active());

  // Short press outside debug mode belongs to the show and changes nothing.
  assert(state.onClick());
  assert(state.view() == DebugView::Off);

  // Long press opens the mood screen.
  state.onHold();
  assert(state.view() == DebugView::Mood);
  assert(state.active());

  // Short presses switch between mood and presence and never reach the show.
  assert(!state.onClick());
  assert(state.view() == DebugView::Presence);
  assert(!state.onClick());
  assert(state.view() == DebugView::Mood);
  assert(!state.onClick());
  assert(state.view() == DebugView::Presence);

  // Long press from the presence screen returns to the eye.
  state.onHold();
  assert(state.view() == DebugView::Off);
  assert(state.onClick());

  // Long press from the mood screen returns to the eye as well, and the next
  // opening starts on the mood screen again.
  state.onHold();
  state.onHold();
  assert(state.view() == DebugView::Off);
  state.onHold();
  assert(!state.onClick());
  state.onHold();
  state.onHold();
  assert(state.view() == DebugView::Mood);

  std::puts("debug_view: all tests passed");
  return 0;
}
