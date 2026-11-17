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

  // Model texts. requestMotion/requestPresence accept an event like
  // showMotion/showPresence (same cooldown, started now) but show nothing yet:
  // the current message stays until the model's emoji arrives. They cancel an
  // earlier open request or growing text. Return false if the event is ignored.
  bool requestMotion(uint32_t now, const MotionEvent& event);
  bool requestPresence(uint32_t now, const PresenceEvent& event);
  bool requestOpen() const { return requestOpen_; }
  // True once the open request has waited longer than the emoji wait time.
  bool requestTimedOut(uint32_t now) const;
  // Shows the fixed text of the open request's event and closes the request.
  bool fallBack(uint32_t now);
  // The model's emoji: closes the request and makes it the current message.
  bool startModelText(uint32_t now, PanelGlyph emoji);
  // Adds one finished word to the growing text; ignored if nothing grows.
  bool appendModelWord(const char* word);
  // Ends the growing text; its display time starts now.
  bool completeModelText(uint32_t now);
  bool growing() const { return growing_; }
  // A message is visible, grows, or a model text is requested.
  bool busy(uint32_t now) const { return requestOpen_ || visible(now); }

  bool visible(uint32_t now) const;
  uint16_t length() const { return length_; }
  // Curated emoji that ended the text, shown large as element 0 above the text lines.
  PanelGlyph leadingGlyph() const { return leadingGlyph_; }
  uint32_t shownAtMs() const { return shownAtMs_; }
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
  PanelGlyph leadingGlyph_ = PanelGlyph::None;
  uint32_t shownAtMs_ = 0;
  uint32_t untilMs_ = 0;
  static bool accept(uint32_t now, bool shown, uint32_t lastAt);
  bool showVariant(uint32_t now, const char* const* texts, uint8_t count, uint32_t sequence,
                   uint8_t textKey);
  bool showFixedMotion(uint32_t now, const MotionEvent& event);
  bool showFixedPresence(uint32_t now, const PresenceEvent& event);
  void startDisplayTime(uint32_t now);
  void cancelModelText(uint32_t now);
  bool requestOpen_ = false;
  bool requestIsMotion_ = false;
  MotionEvent requestMotion_;
  PresenceEvent requestPresence_;
  uint32_t requestedAtMs_ = 0;
  bool growing_ = false;
  // The current message came from the model: it keeps its newest rows in view.
  bool modelText_ = false;
  bool motionShown_[8] = {};
  uint32_t lastMotionAtMs_[8] = {};
  bool presenceShown_[kPresenceEventKinds] = {};
  uint32_t lastPresenceAtMs_[kPresenceEventKinds] = {};
  // Kind of the last text shown (0: none, 1+: motion kind, 32+: presence kind).
  uint8_t lastTextKey_ = 0;
  uint8_t lastTextVariant_ = 0;
};

}  // namespace decaflash::mainframe
