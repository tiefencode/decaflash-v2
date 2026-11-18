#pragma once

#include <cstdint>

#include "motion_events.h"
#include "panel_glyphs.h"
#include "personality.h"
#include "presence_decision.h"
#include "text_bank.h"
#include "text_event.h"

namespace decaflash::mainframe {

// The single product-text surface. Motion and presence texts are fully equal:
// same cooldown per kind, same priority; poems never replace a visible message.
class MessagePanel {
 public:
  // Reaction texts come from the text bank (add-text-bank); without a bank
  // motion and presence events show nothing.
  void setTextBank(TextBank* bank) { bank_ = bank; }
  bool show(uint32_t now, const char* text);
  // Accept the event (cooldown per kind), then show the text bank's line for
  // the event and the lead value of `mood`, the snapshot after the event.
  bool showMotion(uint32_t now, const MotionEvent& event, const Mood& mood);
  bool showPresence(uint32_t now, const PresenceEvent& event, const Mood& mood);
  bool showDepressionPoem(uint32_t now, uint8_t sequence);

  // Model texts. requestMotion/requestPresence accept an event like
  // showMotion/showPresence (same cooldown, started now) but show nothing yet:
  // the current message stays until the model's emoji arrives. They cancel an
  // earlier open request or growing text. Return false if the event is ignored.
  bool requestMotion(uint32_t now, const MotionEvent& event, const Mood& mood);
  bool requestPresence(uint32_t now, const PresenceEvent& event, const Mood& mood);
  bool requestOpen() const { return requestOpen_; }
  // True once the open request has waited longer than the emoji wait time.
  bool requestTimedOut(uint32_t now) const;
  // Shows the text bank's line for the open request (event and mood of its
  // acceptance) and closes the request.
  bool fallBack(uint32_t now);
  // The model's emoji: closes the request and makes it the current message.
  bool startModelText(uint32_t now, PanelGlyph emoji);
  // Adds one finished word to the growing text; ignored if nothing grows.
  bool appendModelWord(const char* word);
  // Ends the growing text; its display time starts now.
  bool completeModelText(uint32_t now);
  // Cancel for low confidence (add-text-model-confidence-gate): the growing
  // text and its emoji go, and the text bank line for the request's event and
  // acceptance mood appears whole, with a new display time. Without a bank
  // line the panel is left empty. False if nothing grows.
  bool replaceWithBank(uint32_t now);
  // The current message's text (without a model text's leading emoji).
  const char* text() const { return text_; }
  bool growing() const { return growing_; }
  // The current message is a text bank line (gesture or presence text), not a
  // model text or poem. Model builds draw its frame wine red (design.md 7a).
  bool bankText() const { return bankText_; }
  // Event of the current reaction text (text bank line or model text); false
  // for poems and other texts. Model builds show it small as a debug aid.
  bool textEvent(TextModelEvent& out) const {
    out = textEvent_;
    return hasTextEvent_;
  }
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
  bool showFromBank(uint32_t now, TextModelEvent event, const Mood& mood);
  void openRequest(uint32_t now, TextModelEvent event, const Mood& mood);
  TextBank* bank_ = nullptr;
  void startDisplayTime(uint32_t now);
  void cancelModelText(uint32_t now);
  bool requestOpen_ = false;
  TextModelEvent requestEvent_ = TextModelEvent::Tap;
  Mood requestMood_;
  uint32_t requestedAtMs_ = 0;
  bool growing_ = false;
  bool bankText_ = false;
  bool hasTextEvent_ = false;
  TextModelEvent textEvent_ = TextModelEvent::Tap;
  bool motionShown_[8] = {};
  uint32_t lastMotionAtMs_[8] = {};
  bool presenceShown_[kPresenceEventKinds] = {};
  uint32_t lastPresenceAtMs_[kPresenceEventKinds] = {};
};

}  // namespace decaflash::mainframe
