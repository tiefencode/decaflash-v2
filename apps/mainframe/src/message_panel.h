#pragma once

#include <cstdint>

#include "motion_events.h"
#include "panel_glyphs.h"
#include "presence_decision.h"

namespace decaflash::mainframe {

// The single product-text surface. Motion and presence texts are fully equal:
// same cooldown per kind, same priority; poems never replace a visible message.
class MessagePanel {
 public:
  bool show(uint32_t now, const char* text);
  bool showMotion(uint32_t now, const MotionEvent& event);
  bool showPresence(uint32_t now, const PresenceEvent& event);
  bool showDepressionPoem(uint32_t now, uint8_t sequence);
  bool visible(uint32_t now) const;
  uint16_t length() const { return length_; }
  PanelGlyph trailingGlyph() const { return trailingGlyph_; }
  PanelGlyph glyphAtRow(uint32_t now, uint8_t row) const;
  bool moreTextBelow(uint32_t now) const;
  void wrappedLine(uint32_t now, uint8_t row, char* output, uint8_t capacity) const;

 private:
  uint8_t lineCount(uint8_t width) const;
  uint8_t firstVisibleLine(uint32_t now, uint8_t totalLines) const;
  static constexpr uint16_t kTextCapacity = 193;
  char text_[kTextCapacity] = {};
  uint16_t length_ = 0;
  uint16_t wrappedLength_ = 0;
  PanelGlyph trailingGlyph_ = PanelGlyph::None;
  uint32_t shownAtMs_ = 0;
  uint32_t untilMs_ = 0;
  bool showVariant(uint32_t now, const char* const* texts, uint8_t count, uint32_t sequence,
                   uint8_t textKey, bool& shown, uint32_t& lastAt);
  bool motionShown_[8] = {};
  uint32_t lastMotionAtMs_[8] = {};
  bool presenceShown_[kPresenceEventKinds] = {};
  uint32_t lastPresenceAtMs_[kPresenceEventKinds] = {};
  // Kind of the last text shown (0: none, 1+: motion kind, 32+: presence kind).
  uint8_t lastTextKey_ = 0;
  uint8_t lastTextVariant_ = 0;
};

}  // namespace decaflash::mainframe
