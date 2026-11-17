#include "frame_interval.h"

#include <cassert>
#include <cstdio>

using namespace decaflash::mainframe;

int main() {
  static_assert(kPanelFrameIntervalMs > kFrameIntervalMs,
                "the panel interval is the reduced rate");

  // Awake, no debug screen, message visible: panel frame, reduced rate.
  assert(drawsMessagePanel(false, false, true));
  assert(selectFrameIntervalMs(drawsMessagePanel(false, false, true)) == kPanelFrameIntervalMs);

  // No message: normal rate.
  assert(!drawsMessagePanel(false, false, false));
  assert(selectFrameIntervalMs(drawsMessagePanel(false, false, false)) == kFrameIntervalMs);

  // Message during a debug screen: the debug screen keeps the normal rate.
  assert(!drawsMessagePanel(true, false, true));
  assert(selectFrameIntervalMs(drawsMessagePanel(true, false, true)) == kFrameIntervalMs);

  // Message during sleep: the sleeping eye keeps the normal rate.
  assert(!drawsMessagePanel(false, true, true));
  assert(selectFrameIntervalMs(drawsMessagePanel(false, true, true)) == kFrameIntervalMs);
  assert(!drawsMessagePanel(true, true, true));

  // Explicit intervals, as used by the measurement sweep.
  assert(selectFrameIntervalMs(true, 40, 50) == 50);
  assert(selectFrameIntervalMs(false, 40, 50) == 40);

  std::puts("frame_interval: all tests passed");
  return 0;
}
