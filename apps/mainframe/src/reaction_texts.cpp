#include "reaction_texts.h"

namespace decaflash::mainframe {

MoodLevel moodLevel(uint8_t value) {
  if (value <= 33) return MoodLevel::Low;
  if (value <= 66) return MoodLevel::Middle;
  return MoodLevel::High;
}

bool textModelEventFor(const MotionEvent& event, TextModelEvent& out) {
  switch (event.kind) {
    case MotionKind::Move: out = TextModelEvent::Move; return true;
    case MotionKind::Tap: out = TextModelEvent::Tap; return true;
    case MotionKind::MultiTap:
      out = event.count >= 3 ? TextModelEvent::MultiTap : TextModelEvent::DoubleTap;
      return true;
    case MotionKind::Impact: out = TextModelEvent::Impact; return true;
    case MotionKind::Rotate: out = TextModelEvent::Rotate; return true;
    case MotionKind::Reposition: out = TextModelEvent::Reposition; return true;
    case MotionKind::Shake: out = TextModelEvent::Shake; return true;
    case MotionKind::None: return false;
  }
  return false;
}

bool textModelEventFor(const PresenceEvent& event, TextModelEvent& out) {
  switch (event.kind) {
    case PresenceEventKind::ArrivedInFront: out = TextModelEvent::ArrivedInFront; return true;
    case PresenceEventKind::WalkedPast: out = TextModelEvent::WalkedPast; return true;
    case PresenceEventKind::Left: out = TextModelEvent::Left; return true;
    case PresenceEventKind::StandsFar: out = TextModelEvent::StandsFar; return true;
    case PresenceEventKind::Moves: return false;  // no text, no request
    case PresenceEventKind::None: return false;
  }
  return false;
}

bool ReactionTexts::modelUsable(bool panelHidden) const {
  return model_ && model_->ready() && !panelHidden;
}

void ReactionTexts::cancelModel() {
  if (model_ && activeRequestId_) model_->cancel();
  activeRequestId_ = 0;
}

void ReactionTexts::startRequest(uint32_t now, TextModelEvent event, const Mood& mood,
                                 uint32_t sequence) {
  cancelModel();
  TextModelRequest request;
  request.id = nextRequestId_++;
  if (!nextRequestId_) nextRequestId_ = 1;
  request.event = event;
  request.mood[0] = moodLevel(mood.energy);
  request.mood[1] = moodLevel(mood.annoyance);
  request.mood[2] = moodLevel(mood.attention);
  request.mood[3] = moodLevel(mood.loneliness);
  request.mood[4] = moodLevel(mood.depression);
  request.seed = now * 2654435761U ^ sequence;
  model_->request(request);
  activeRequestId_ = request.id;
}

void ReactionTexts::onMotion(uint32_t now, const MotionEvent& event, const Mood& mood,
                             bool panelHidden) {
  TextModelEvent modelEvent;
  if (modelUsable(panelHidden) && textModelEventFor(event, modelEvent)) {
    if (panel_.requestMotion(now, event)) startRequest(now, modelEvent, mood, event.sequence);
    return;
  }
  if (panel_.showMotion(now, event)) cancelModel();
}

void ReactionTexts::onPresence(uint32_t now, const PresenceEvent& event, const Mood& mood,
                               bool panelHidden) {
  TextModelEvent modelEvent;
  if (modelUsable(panelHidden) && textModelEventFor(event, modelEvent)) {
    if (panel_.requestPresence(now, event)) {
      startRequest(now, modelEvent, mood, event.sequence);
    }
    return;
  }
  if (panel_.showPresence(now, event)) cancelModel();
}

void ReactionTexts::service(uint32_t now) {
  if (!model_) return;
  TextModelPiece piece;
  while (model_->poll(piece)) {
    if (!activeRequestId_ || piece.requestId != activeRequestId_) continue;  // stale
    bool taken = false;
    switch (piece.kind) {
      case TextModelPiece::Kind::Emoji:
        taken = panel_.startModelText(now, piece.emoji);
        break;
      case TextModelPiece::Kind::Word:
        taken = panel_.appendModelWord(piece.word) || panel_.growing();
        break;
      case TextModelPiece::Kind::End:
        taken = panel_.completeModelText(now);
        if (taken) activeRequestId_ = 0;
        break;
    }
    if (!taken) cancelModel();  // the panel moved on (fixed text, late emoji)
  }
  if (panel_.requestTimedOut(now)) {
    cancelModel();
    if (panel_.fallBack(now)) ++fallbacks_;
  }
  if (activeRequestId_ && !panel_.requestOpen() && !panel_.growing()) cancelModel();
}

}  // namespace decaflash::mainframe
