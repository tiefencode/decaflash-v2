#pragma once

#include <cstdint>

#include "message_panel.h"
#include "motion_events.h"
#include "panel_glyphs.h"
#include "personality.h"
#include "presence_decision.h"
#include "text_event.h"
#include "text_model_gate.h"

namespace decaflash::mainframe {

enum class MoodLevel : uint8_t { Low, Middle, High };
// Fixed thirds for all five mood values: 0-33 low, 34-66 middle, 67-100 high.
MoodLevel moodLevel(uint8_t value);

struct TextModelRequest {
  uint32_t id = 0;
  TextModelEvent event = TextModelEvent::Tap;
  // energy, annoyance, attention, loneliness, depression
  MoodLevel mood[5] = {};
  uint32_t seed = 0;
};

// One delivered piece of a model text: the emoji first, then finished words,
// then the end. Pieces carry the id of their request and the confidence
// values (margins) of the tokens that formed them, in order: one for the emoji
// (word holds its UTF-8), one per word token, one for the end token (none when
// the text ended at the length limit).
struct TextModelPiece {
  static constexpr int kMaxMargins = 16;
  enum class Kind : uint8_t { Emoji, Word, End };
  uint32_t requestId = 0;
  Kind kind = Kind::End;
  PanelGlyph emoji = PanelGlyph::None;
  char word[48] = {};
  uint8_t marginCount = 0;
  float margins[kMaxMargins] = {};
};

// The last cancel of an unsure model text, for the serial report.
struct GateCancel {
  uint32_t atMs = 0;
  uint32_t requestId = 0;
  int token = 0;
  float mean = 0.0f;
  char partial[200] = {};
  char bank[200] = {};
};

// The on-device text model as the panel path sees it. The firmware runtime
// implements it in the model builds; without a runtime there is no model.
class TextModel {
 public:
  virtual ~TextModel() = default;
  // The runtime is on: a valid image is mapped and the working memory taken.
  virtual bool ready() const = 0;
  // Starts a request; an earlier open request is replaced.
  virtual void request(const TextModelRequest& request) = 0;
  // Stops the open request; the model task ends it between forward passes.
  virtual void cancel() = 0;
  // Takes the next delivered piece, if any.
  virtual bool poll(TextModelPiece& piece) = 0;
};

// Decides per accepted event between a model text and a text bank line, and moves
// the model's pieces into the panel. model may be null (no runtime).
class ReactionTexts {
 public:
  ReactionTexts(MessagePanel& panel, TextModel* model) : panel_(panel), model_(model) {}
  void onMotion(uint32_t now, const MotionEvent& event, const Mood& mood, bool panelHidden);
  void onPresence(uint32_t now, const PresenceEvent& event, const Mood& mood,
                  bool panelHidden);
  // Call every loop pass: takes delivered pieces and falls back to the fixed
  // text when the emoji is late.
  void service(uint32_t now);
  uint32_t fallbacks() const { return fallbacks_; }
  // Cancels for low confidence since start, and the last one.
  uint32_t gateCancels() const { return gateCancels_; }
  const GateCancel& lastGateCancel() const { return lastGateCancel_; }

 private:
  bool modelUsable(bool panelHidden) const;
  void startRequest(uint32_t now, TextModelEvent event, const Mood& mood, uint32_t sequence);
  void cancelModel();
  bool gate(uint32_t now, const TextModelPiece& piece);
  void appendPartial(const char* utf8);
  MessagePanel& panel_;
  TextModel* model_;
  uint32_t nextRequestId_ = 1;
  uint32_t activeRequestId_ = 0;
  uint32_t fallbacks_ = 0;
  ConfidenceGate gate_;
  char partial_[200] = {};
  uint32_t gateCancels_ = 0;
  GateCancel lastGateCancel_;
};

}  // namespace decaflash::mainframe
