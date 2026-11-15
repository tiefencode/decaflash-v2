#include "motion_trace.h"
#include <cassert>
#include <cstdio>
using namespace decaflash::mainframe;
using Output = MotionTrace::Output;
const MotionSample rest = {0, 0, 1, 0, 0, 0};

int main() {
  // Rest produces only idle reports, with the sample cadence in them.
  {
    MotionTrace trace;
    unsigned idle = 0, windows = 0;
    for (uint32_t t = 0; t <= 4100; t += 10) {
      const auto out = trace.feed(t, rest);
      if (out == Output::Idle) ++idle;
      if (out == Output::Window) ++windows;
    }
    assert(idle == 2 && windows == 0);
    assert(trace.idle().maxMg == 0 && trace.idle().maxDps == 0);
    assert(trace.idle().samples == 200 && trace.idle().maxGapMs == 10);
  }
  // A 60 mg pulse is far below the classifier's 300 mg but must be captured.
  {
    MotionTrace trace;
    unsigned windows = 0;
    for (uint32_t t = 0; t <= 1500; t += 10) {
      auto sample = rest;
      if (t == 500 || t == 510) sample.ax = 0.06f;
      if (t == 520) sample.ax = 0.25f;
      if (trace.feed(t, sample) == Output::Window) ++windows;
    }
    assert(windows == 1);
    const auto& w = trace.window();
    assert(w.sequence == 1 && w.startMs == 500);
    assert(w.peakMg >= 240 && w.peakMg <= 250 && w.peakAtMs == 20);
    assert(w.overMs[0] == 30 && w.overMs[1] == 10 && w.overMs[2] == 10);
    assert(w.overMs[3] == 0 && w.overMs[4] == 0);
    assert(w.samples == 71 && w.rawCount == 71);
    assert(w.rawMg[0] == 60 && w.rawMg[2] == w.peakMg);
  }
  // Rotation alone opens a window as well.
  {
    MotionTrace trace;
    unsigned windows = 0;
    for (uint32_t t = 0; t <= 1500; t += 10) {
      auto sample = rest;
      if (t >= 500 && t <= 600) sample.gz = 20;
      if (trace.feed(t, sample) == Output::Window) ++windows;
    }
    assert(windows == 1 && trace.window().peakDps == 20 && trace.window().peakMg == 0);
  }
  // Raw capture is bounded when samples arrive faster than expected.
  {
    MotionTrace trace;
    bool done = false;
    for (uint32_t t = 0; t <= 2000 && !done; t += 2) {
      auto sample = rest;
      if (t >= 500) sample.ax = 0.5f;
      done = trace.feed(t, sample) == Output::Window;
    }
    assert(done && trace.window().rawCount == MotionTraceWindow::kRawCapacity);
    assert(trace.window().samples > MotionTraceWindow::kRawCapacity);
  }
  std::puts("motion_trace: ok");
  return 0;
}
