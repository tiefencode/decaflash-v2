#pragma once

#include <Arduino.h>

namespace decaflash::mainframe {

// Owns only the display animation cadence. Mainframe control remains in main.cpp.
class EyeRenderer {
 public:
  void service(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync);

 private:
  bool initialiseCanvas();
  void draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync);

  uint32_t lastFrameAtMs_ = 0;
  bool canvasReady_ = false;
};

}  // namespace decaflash::mainframe
