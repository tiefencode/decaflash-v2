#pragma once

#include <Arduino.h>
#include "personality.h"

namespace decaflash::mainframe {

// Owns only the display animation cadence. Mainframe control remains in main.cpp.
class EyeRenderer {
 public:
  bool begin() { return canvasReady_ || initialiseCanvas(); }
  void service(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
               uint8_t vuLevel, uint8_t beatPulse, uint8_t attention, uint8_t annoyance,
               uint8_t loneliness,
               const Mood* debug = nullptr, const MotionEvent* event = nullptr);

 private:
  bool initialiseCanvas();
  void draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
            uint8_t vuLevel, uint8_t beatPulse, uint8_t attention, uint8_t annoyance,
            uint8_t loneliness);
  void updateGaze(uint32_t now, uint8_t attention, float& gazeX, float& gazeY);
  void drawEmotionLids(uint8_t annoyance, uint8_t loneliness);
  uint32_t nextGazeRandom();

  uint32_t lastFrameAtMs_ = 0;
  uint32_t lastGazeAtMs_ = 0;
  float gazeAlertness_ = 0.0f;
  float gazeX_ = 0.0f;
  float gazeY_ = 0.0f;
  float gazeStartX_ = 0.0f;
  float gazeStartY_ = 0.0f;
  float gazeTargetX_ = 0.0f;
  float gazeTargetY_ = 0.0f;
  uint32_t gazeNextAtMs_ = 0;
  uint32_t gazeSaccadeAtMs_ = 0;
  uint16_t gazeSaccadeDurationMs_ = 0;
  uint32_t gazeRandom_ = 0xC0FFEE21;
  bool gazeSaccading_ = false;
  bool canvasReady_ = false;
};

}  // namespace decaflash::mainframe
