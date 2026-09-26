#pragma once

#include <Arduino.h>
#include "personality.h"

namespace decaflash::mainframe {

// Owns only the display animation cadence. Mainframe control remains in main.cpp.
class EyeRenderer {
 public:
  bool begin() { return canvasReady_ || initialiseCanvas(); }
  void service(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
               uint8_t vuLevel, uint8_t beatPulse, const Mood* debug = nullptr, const MotionEvent* event = nullptr);

 private:
  bool initialiseCanvas();
  void draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
            uint8_t vuLevel, uint8_t beatPulse);

  uint32_t lastFrameAtMs_ = 0;
  bool canvasReady_ = false;
};

}  // namespace decaflash::mainframe
