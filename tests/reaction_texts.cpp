#include "reaction_texts.h"
#include "test_text_bank.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

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
  // Pieces without margins (marginCount 0) are never checked by the gate;
  // a margin below 0 means "none".
  void emoji(uint32_t id, PanelGlyph glyph, float margin = -1.0f) {
    TextModelPiece& piece = queue[tail++];
    piece = TextModelPiece{};
    piece.requestId = id;
    piece.kind = TextModelPiece::Kind::Emoji;
    piece.emoji = glyph;
    std::strncpy(piece.word, "\xF0\x9F\x99\x82", sizeof(piece.word) - 1);
    if (margin >= 0.0f) {
      piece.marginCount = 1;
      piece.margins[0] = margin;
    }
  }
  void word(uint32_t id, const char* text, std::initializer_list<float> margins = {}) {
    TextModelPiece& piece = queue[tail++];
    piece = TextModelPiece{};
    piece.requestId = id;
    piece.kind = TextModelPiece::Kind::Word;
    std::strncpy(piece.word, text, sizeof(piece.word) - 1);
    for (float m : margins) piece.margins[piece.marginCount++] = m;
  }
  void end(uint32_t id, float margin = -1.0f) {
    TextModelPiece& piece = queue[tail++];
    piece = TextModelPiece{};
    piece.requestId = id;
    piece.kind = TextModelPiece::Kind::End;
    if (margin >= 0.0f) {
      piece.marginCount = 1;
      piece.margins[0] = margin;
    }
  }
  void clear() { head = tail = 0; }
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

  // No runtime: every event takes the text bank path at once.
  {
    MessagePanel panel;
    panel.setTextBank(&test::textBank());
    ReactionTexts texts(panel, nullptr);
    texts.onMotion(0, tap, mood, false);
    panel.wrappedLine(0, 1, line, sizeof(line));
    assert(std::strcmp(line, "KLOPF") == 0);
    texts.service(5000);
    assert(!panel.requestOpen());
  }

  // Runtime off (no valid image): text bank line, the model is not asked.
  {
    MessagePanel panel;
    panel.setTextBank(&test::textBank());
    FakeModel model;
    model.on = false;
    ReactionTexts texts(panel, &model);
    MotionEvent shake;
    shake.kind = MotionKind::Shake;
    texts.onMotion(0, shake, mood, false);
    assert(model.requests == 0);
    assert(panel.leadingGlyph() == PanelGlyph::Nauseous);
  }

  // Model running: request with event and mood levels, emoji first, then words.
  {
    MessagePanel panel;
    panel.setTextBank(&test::textBank());
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

  // Emoji too late: cancel and text bank line; a stale emoji is ignored.
  {
    MessagePanel panel;
    panel.setTextBank(&test::textBank());
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

  // Panel hidden (sleep or debug screen): text bank line without asking the model,
  // and an open request or growing text is cancelled.
  {
    MessagePanel panel;
    panel.setTextBank(&test::textBank());
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
    panel.setTextBank(&test::textBank());
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

  // Confidence gate (add-text-model-confidence-gate): running mean of the
  // margins, emoji and end counted, checked from token 5 on.
  {
    MessagePanel panel;
    panel.setTextBank(&test::textBank());
    FakeModel model;
    ReactionTexts texts(panel, &model);
    MotionEvent single;
    single.kind = MotionKind::Tap;
    const float low = kGateThreshold * 0.5f;
    const float high = kGateThreshold * 2.0f;

    // Unsure at token 5: cancel, the growing text and its emoji go, the
    // text bank's Tap line appears whole with the text bank frame.
    texts.onMotion(1000, single, mood, false);
    uint32_t id = model.last.id;
    model.emoji(id, PanelGlyph::Smile, low);
    model.word(id, "EINS", {low});
    model.word(id, "ZWEI", {low});
    model.word(id, "DREI,", {low, low});  // tokens 4 and 5
    model.word(id, "VIER", {low});
    model.end(id, low);
    texts.service(1200);
    assert(texts.gateCancels() == 1 && model.cancels == 1);
    assert(texts.lastGateCancel().token == kGateCheckToken);
    assert(texts.lastGateCancel().mean < kGateThreshold);
    assert(std::strstr(texts.lastGateCancel().partial, "EINS ZWEI DREI,") != nullptr);
    assert(!panel.growing() && panel.bankText());
    {
      TextModelEvent shown;
      assert(panel.textEvent(shown) && shown == TextModelEvent::Tap);
    }
    panel.wrappedLine(1200, 1, line, sizeof(line));
    assert(std::strcmp(line, "KLOPF") == 0);
    assert(std::strcmp(texts.lastGateCancel().bank, panel.text()) == 0);
    assert(panel.visible(1200));
    model.clear();

    // Low before token 5, back above the threshold from token 5 on: kept.
    // A later drop below the threshold still cancels.
    texts.onMotion(20000, single, mood, false);
    id = model.last.id;
    model.emoji(id, PanelGlyph::Smile, low);
    model.word(id, "EINS", {low});
    model.word(id, "ZWEI", {low});
    model.word(id, "DREI", {low});
    model.word(id, "VIER", {kGateThreshold * 10.0f});
    texts.service(20200);
    assert(texts.gateCancels() == 1 && panel.growing());
    model.end(id, high);
    texts.service(20300);
    assert(texts.gateCancels() == 1 && !panel.growing() && !panel.bankText());
    model.clear();

    // A text shorter than the check token completes without a check.
    texts.onMotion(40000, single, mood, false);
    id = model.last.id;
    model.emoji(id, PanelGlyph::Smile, low);
    model.word(id, "EINS", {low});
    model.word(id, "ZWEI!", {low, low});
    model.end(id);  // ended at the length limit: no end margin
    texts.service(40200);
    assert(texts.gateCancels() == 1 && !panel.growing() && !panel.bankText());
    panel.wrappedLine(40200, 1, line, sizeof(line));
    assert(std::strcmp(line, "EINS") == 0);
    model.clear();

    // Confident text completes; the end token is the checked token here.
    texts.onMotion(60000, single, mood, false);
    id = model.last.id;
    model.emoji(id, PanelGlyph::Smile, high);
    model.word(id, "EINS", {high});
    model.word(id, "ZWEI", {high});
    model.word(id, "DREI", {high});
    model.end(id, high);
    texts.service(60200);
    assert(texts.gateCancels() == 1 && !panel.growing() && !panel.bankText());
    model.clear();

    // The end token can cancel: the whole text was shown, then replaced.
    texts.onMotion(80000, single, mood, false);
    id = model.last.id;
    model.emoji(id, PanelGlyph::Smile, high);
    model.word(id, "EINS", {low});
    model.word(id, "ZWEI", {low});
    model.word(id, "DREI", {low});
    model.end(id, 0.0f);
    texts.service(80200);
    assert(texts.gateCancels() == 2 && texts.lastGateCancel().token == 5);
    assert(!panel.growing() && panel.bankText());

    // The gate decides the same as ConfidenceGate (shared with the host tool
    // and the TEXT line).
    ConfidenceGate gate;
    for (float m : {high, low, low, low}) assert(!gate.add(m));
    assert(gate.add(0.0f) && gate.cancelledAt() == 5 && !gate.add(0.0f));
  }

  puts("PASS: reaction texts choose model or text bank and move model pieces into the panel");
}
