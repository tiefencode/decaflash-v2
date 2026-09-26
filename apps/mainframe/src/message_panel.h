#pragma once

#include <cstdint>

#include "motion_events.h"

namespace decaflash::mainframe {

// The single product-text surface. Any source may call show(); motion is only
// the first source wired into it.
class MessagePanel {
 public:
  bool show(uint32_t now, const char* text);
  bool showMotion(uint32_t now, const MotionEvent& event);
  bool visible(uint32_t now) const;
  uint16_t length() const { return length_; }
  void wrappedLine(uint32_t now, uint8_t row, char* output, uint8_t capacity) const;

 private:
  uint8_t lineCount(uint8_t width) const;
  static constexpr uint16_t kTextCapacity = 81;
  char text_[kTextCapacity] = {};
  uint16_t length_ = 0;
  uint32_t shownAtMs_ = 0;
  uint32_t untilMs_ = 0;
  bool motionShown_[8] = {};
  uint32_t lastMotionAtMs_[8] = {};
  MotionKind lastMotionKind_ = MotionKind::None;
  uint8_t lastMotionVariant_ = 0;
};

}  // namespace decaflash::mainframe
