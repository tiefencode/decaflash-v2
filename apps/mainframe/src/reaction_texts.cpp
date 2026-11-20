#include "reaction_texts.h"

#include <cstring>

namespace decaflash::mainframe {

MoodLevel moodLevel(uint8_t value) {
  if (value <= 33) return MoodLevel::Low;
  if (value <= 66) return MoodLevel::Middle;
  return MoodLevel::High;
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
  gate_.reset();
  partial_[0] = '\0';
}

void ReactionTexts::onMotion(uint32_t now, const MotionEvent& event, const Mood& mood,
                             bool panelHidden) {
  TextModelEvent modelEvent;
  if (modelUsable(panelHidden) && textModelEventFor(event, modelEvent)) {
    if (panel_.requestMotion(now, event, mood)) startRequest(now, modelEvent, mood, event.sequence);
    return;
  }
  if (panel_.showMotion(now, event, mood)) cancelModel();
}

void ReactionTexts::onPresence(uint32_t now, const PresenceEvent& event, const Mood& mood,
                               bool panelHidden) {
  TextModelEvent modelEvent;
  if (modelUsable(panelHidden) && textModelEventFor(event, modelEvent)) {
    if (panel_.requestPresence(now, event, mood)) {
      startRequest(now, modelEvent, mood, event.sequence);
    }
    return;
  }
  if (panel_.showPresence(now, event, mood)) cancelModel();
}

void ReactionTexts::appendPartial(const char* utf8) {
  const size_t used = std::strlen(partial_);
  const size_t bytes = std::strlen(utf8);
  const size_t separator = used ? 1 : 0;
  if (used + separator + bytes >= sizeof(partial_)) return;
  if (separator) partial_[used] = ' ';
  std::memcpy(partial_ + used + separator, utf8, bytes + 1);
}

// Feeds the piece's margins to the gate token by token. On a cancel the
// request ends, the growing text and its emoji go, and the text bank line for
// the request's event and acceptance mood appears whole (design.md Decision 4).
bool ReactionTexts::gate(uint32_t now, const TextModelPiece& piece) {
  if (!kGateActsOnPanel) return false;
  const int count = piece.marginCount < TextModelPiece::kMaxMargins ? piece.marginCount
                                                                    : TextModelPiece::kMaxMargins;
  for (int i = 0; i < count; ++i) {
    if (!gate_.add(piece.margins[i])) continue;
    if (piece.kind != TextModelPiece::Kind::End) appendPartial(piece.word);
    lastGateCancel_ = GateCancel{};
    lastGateCancel_.atMs = now;
    lastGateCancel_.requestId = activeRequestId_;
    lastGateCancel_.token = gate_.cancelledAt();
    lastGateCancel_.mean = gate_.mean();
    std::memcpy(lastGateCancel_.partial, partial_, sizeof(partial_));
    cancelModel();
    if (panel_.replaceWithBank(now)) {
      std::strncpy(lastGateCancel_.bank, panel_.text(), sizeof(lastGateCancel_.bank) - 1);
    }
    ++gateCancels_;
    return true;
  }
  return false;
}

void ReactionTexts::service(uint32_t now) {
  if (!model_) return;
  TextModelPiece piece;
  while (model_->poll(piece)) {
    if (!activeRequestId_ || piece.requestId != activeRequestId_) continue;  // stale
    if (gate(now, piece)) continue;
    bool taken = false;
    switch (piece.kind) {
      case TextModelPiece::Kind::Emoji:
        taken = panel_.startModelText(now, piece.emoji);
        appendPartial(piece.word);
        break;
      case TextModelPiece::Kind::Word:
        taken = panel_.appendModelWord(piece.word) || panel_.growing();
        appendPartial(piece.word);
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
