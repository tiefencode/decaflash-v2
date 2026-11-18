#include "reaction_texts.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace decaflash::mainframe;

namespace {

class FakeModel : public TextModel {
 public:
  bool on = true;
  int requests = 0;
  int cancels = 0;
  TextModelRequest last;
  TextModelPiece queue[16];
  int head = 0;
  int tail = 0;

  bool ready() const override { return on; }
  void request(const TextModelRequest& request) override {
    ++requests;
    last = request;
  }
  void cancel() override { ++cancels; }
  bool poll(TextModelPiece& piece) override {
    if (head == tail) return false;
    piece = queue[head++];
    return true;
  }
  void emoji(uint32_t id, PanelGlyph glyph) {
    TextModelPiece& piece = queue[tail++];
    piece = TextModelPiece{};
    piece.requestId = id;
    piece.kind = TextModelPiece::Kind::Emoji;
    piece.emoji = glyph;
  }
  void word(uint32_t id, const char* text) {
    TextModelPiece& piece = queue[tail++];
    piece = TextModelPiece{};
    piece.requestId = id;
    piece.kind = TextModelPiece::Kind::Word;
    std::strncpy(piece.word, text, sizeof(piece.word) - 1);
  }
  void end(uint32_t id) {
    TextModelPiece& piece = queue[tail++];
    piece = TextModelPiece{};
    piece.requestId = id;
    piece.kind = TextModelPiece::Kind::End;
  }
};

}  // namespace

int main() {
  char line[64] = {};
  Mood mood;
  mood.energy = 33;
  mood.annoyance = 34;
  mood.attention = 66;
  mood.loneliness = 67;
  mood.depression = 100;

  assert(moodLevel(0) == MoodLevel::Low && moodLevel(33) == MoodLevel::Low);
  assert(moodLevel(34) == MoodLevel::Middle && moodLevel(66) == MoodLevel::Middle);
  assert(moodLevel(67) == MoodLevel::High && moodLevel(100) == MoodLevel::High);

  MotionEvent tap;
  tap.kind = MotionKind::Tap;

  // No runtime: every event takes the fixed path at once.
  {
    MessagePanel panel;
    ReactionTexts texts(panel, nullptr);
    texts.onMotion(0, tap, mood, false);
    panel.wrappedLine(0, 1, line, sizeof(line));
    assert(std::strcmp(line, "KLOPF") == 0);
    texts.service(5000);
    assert(!panel.requestOpen());
  }

  // Runtime off (no valid image): fixed text, the model is not asked.
  {
    MessagePanel panel;
    FakeModel model;
    model.on = false;
    ReactionTexts texts(panel, &model);
    MotionEvent shake;
    shake.kind = MotionKind::Shake;
    texts.onMotion(0, shake, mood, false);
    assert(model.requests == 0);
    assert(panel.leadingGlyph() == PanelGlyph::Blush);
  }

  // Model running: request with event and mood levels, emoji first, then words.
  {
    MessagePanel panel;
    FakeModel model;
    ReactionTexts texts(panel, &model);
    MotionEvent triple;
    triple.kind = MotionKind::MultiTap;
    triple.count = 3;
    texts.onMotion(100, triple, mood, false);
    assert(model.requests == 1);
    assert(model.last.event == TextModelEvent::MultiTap);
    assert(model.last.mood[0] == MoodLevel::Low && model.last.mood[1] == MoodLevel::Middle);
    assert(model.last.mood[2] == MoodLevel::Middle && model.last.mood[3] == MoodLevel::High);
    assert(model.last.mood[4] == MoodLevel::High);
    assert(panel.requestOpen() && !panel.visible(100));
    const uint32_t id = model.last.id;
    model.emoji(id, PanelGlyph::Angry);
    texts.service(300);
    assert(panel.glyphAtRow(300, 0) == PanelGlyph::Angry && panel.growing());
    model.word(id, "NICHT");
    model.word(id, "SO!");
    texts.service(500);
    panel.wrappedLine(500, 1, line, sizeof(line));
    assert(std::strcmp(line, "NICHT") == 0);
    panel.wrappedLine(500, 2, line, sizeof(line));
    assert(std::strcmp(line, "SO!") == 0);
    model.end(id);
    texts.service(700);
    assert(!panel.growing() && panel.visible(4699) && !panel.visible(4700));
    assert(texts.fallbacks() == 0);

    // A double tap asks with its own event token.
    MotionEvent twice;
    twice.kind = MotionKind::MultiTap;
    twice.count = 2;
    texts.onMotion(20000, twice, mood, false);
    assert(model.requests == 2 && model.last.event == TextModelEvent::DoubleTap);
  }

  // Emoji too late: cancel and fixed text; a stale emoji is ignored.
  {
    MessagePanel panel;
    FakeModel model;
    ReactionTexts texts(panel, &model);
    texts.onMotion(0, tap, mood, false);
    const uint32_t id = model.last.id;
    texts.service(1999);
    assert(panel.requestOpen());
    texts.service(2000);
    assert(model.cancels == 1 && texts.fallbacks() == 1);
    panel.wrappedLine(2000, 1, line, sizeof(line));
    assert(std::strcmp(line, "KLOPF") == 0);
    model.emoji(id, PanelGlyph::Smile);
    texts.service(2100);
    assert(panel.leadingGlyph() == PanelGlyph::Speech);
  }

  // Panel hidden (sleep or debug screen): fixed text without asking the model,
  // and an open request or growing text is cancelled.
  {
    MessagePanel panel;
    FakeModel model;
    ReactionTexts texts(panel, &model);
    texts.onMotion(0, tap, mood, false);
    model.emoji(model.last.id, PanelGlyph::Smile);
    texts.service(100);
    assert(panel.growing());
    PresenceEvent left;
    left.kind = PresenceEventKind::Left;
    texts.onPresence(200, left, mood, true);
    assert(model.requests == 1 && model.cancels == 1);
    assert(!panel.growing() && panel.leadingGlyph() == PanelGlyph::Cry);
  }

  // New event while a text grows: the old request is cancelled, its late
  // pieces are dropped, and the new request takes over.
  {
    MessagePanel panel;
    FakeModel model;
    ReactionTexts texts(panel, &model);
    texts.onMotion(0, tap, mood, false);
    const uint32_t first = model.last.id;
    model.emoji(first, PanelGlyph::Smile);
    model.word(first, "HUHU");
    texts.service(100);
    PresenceEvent arrived;
    arrived.kind = PresenceEventKind::ArrivedInFront;
    texts.onPresence(200, arrived, mood, false);
    assert(model.requests == 2 && model.cancels == 1);
    assert(model.last.event == TextModelEvent::ArrivedInFront);
    const uint32_t second = model.last.id;
    assert(second != first);
    model.word(first, "DU");
    model.emoji(second, PanelGlyph::Wave);
    texts.service(300);
    assert(panel.glyphAtRow(300, 0) == PanelGlyph::Wave);
    panel.wrappedLine(300, 1, line, sizeof(line));
    assert(line[0] == '\0');

    // Bewegt sich asks nothing and shows nothing.
    PresenceEvent moves;
    moves.kind = PresenceEventKind::Moves;
    texts.onPresence(400, moves, mood, false);
    assert(model.requests == 2 && panel.growing());
  }

  puts("PASS: reaction texts choose model or fixed texts and move model pieces into the panel");
}
