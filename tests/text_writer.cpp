// Host test of the text model writer (apps/mainframe/src/slm/text_writer.h,
// add-text-model task 7.1) with a fake model: masks, sampling with a seed,
// hard length limit, end token and cancel.
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "slm/text_writer.h"

namespace vocab = decaflash::mainframe::text_model;

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

// Deterministic pseudo logits; rows outside the requested range are poisoned
// so that reading them would pick them.
struct FakeModel {
  std::vector<float> logits = std::vector<float>(vocab::kOutputTokens);
  int position = 0;
  int endAtPiece = -1;  // output position (0 = emoji) at which the end token wins
  bool noEnd = false;   // the end token never wins
  std::vector<int> firstRows, rowCounts;

  const float* forward(int token, int firstRow, int rowCount) {
    if (token < 0 || token >= vocab::kInputTokens) return nullptr;
    firstRows.push_back(firstRow);
    rowCounts.push_back(rowCount);
    for (float& value : logits) value = 1e30f;
    const int piece = position - (slm::kConditionTokens - 1);
    for (int row = firstRow; row < firstRow + rowCount; ++row) {
      uint32_t h = static_cast<uint32_t>(row) * 2654435761u ^ static_cast<uint32_t>(position * 40503);
      h ^= h >> 15;
      logits[row] = static_cast<float>(h % 1000) / 100.0f;
      if (row == vocab::kEndToken) logits[row] = noEnd ? -1e3f : (piece == endAtPiece ? 1e3f : -1e3f);
    }
    ++position;
    return logits.data();
  }
};

struct Sink {
  std::vector<uint16_t> emojis;
  std::vector<std::string> words;
  FakeModel* model = nullptr;
  int cancelAfterEmoji = -1;  // forward passes after the emoji before cancelling
  int passesAtCancel = -1;
  bool cancelled() {
    if (cancelAfterEmoji < 0 || emojis.empty()) return false;
    const int after = model->position - slm::kConditionTokens;
    if (after >= cancelAfterEmoji) {
      if (passesAtCancel < 0) passesAtCancel = model->position;
      return true;
    }
    return false;
  }
  void emoji(uint16_t token) { emojis.push_back(token); }
  void word(const char* utf8) { words.emplace_back(utf8); }
  std::string body() const {
    std::string out;
    for (const std::string& w : words) out += (out.empty() ? "" : " ") + w;
    return out;
  }
};

const uint16_t kCondition[slm::kConditionTokens] = {
    vocab::kStartToken, vocab::kFirstEventToken + 1, vocab::kFirstMoodToken + 0, vocab::kFirstMoodToken + 4,
    vocab::kFirstMoodToken + 8, vocab::kFirstMoodToken + 9, vocab::kFirstMoodToken + 14};

struct Run {
  slm::WriteEnd end;
  Sink sink;
  FakeModel model;
};

Run run(uint32_t seed, int endAtPiece, bool noEnd = false, int cancelAfterEmoji = -1) {
  Run r;
  r.model.endAtPiece = endAtPiece;
  r.model.noEnd = noEnd;
  r.sink.model = &r.model;
  r.sink.cancelAfterEmoji = cancelAfterEmoji;
  r.end = slm::writeText(r.model, kCondition, seed, r.sink);
  return r;
}

int characters(const std::string& text) { return slm::pieceCharacters(text.c_str()); }

}  // namespace

int main() {
  // Masks: condition tokens compute no head, the last one only the emoji rows,
  // every later pass only the text rows.
  Run a = run(12345, 3);
  check(a.end == slm::WriteEnd::Done, "end token ends the text");
  check(a.sink.emojis.size() == 1, "exactly one emoji");
  check(!a.sink.emojis.empty() && a.sink.emojis[0] >= vocab::kFirstEmojiToken &&
            a.sink.emojis[0] < vocab::kOutputTokens,
        "emoji from the curated rows");
  bool masks = a.model.firstRows.size() >= slm::kConditionTokens;
  for (int i = 0; masks && i < slm::kConditionTokens - 1; ++i) masks = a.model.rowCounts[i] == 0;
  masks = masks && a.model.firstRows[slm::kConditionTokens - 1] == vocab::kFirstEmojiToken &&
          a.model.rowCounts[slm::kConditionTokens - 1] == vocab::kOutputTokens - vocab::kFirstEmojiToken;
  for (size_t i = slm::kConditionTokens; masks && i < a.model.firstRows.size(); ++i) {
    masks = a.model.firstRows[i] == 0 && a.model.rowCounts[i] == vocab::kFirstEmojiToken;
  }
  check(masks, "head rows per pass follow the masks");
  check(!a.sink.words.empty(), "words delivered");
  for (const std::string& w : a.sink.words) {
    check(!w.empty() && w.find(' ') == std::string::npos, "a delivered word has no space");
  }

  // Fixed seed gives fixed output, another seed another one.
  Run b = run(12345, 3);
  check(a.sink.emojis == b.sink.emojis && a.sink.words == b.sink.words, "fixed seed, fixed text");
  bool differs = false;
  for (uint32_t seed = 1; seed < 20 && !differs; ++seed) {
    Run c = run(seed, 3);
    differs = c.sink.emojis != a.sink.emojis || c.sink.words != a.sink.words;
  }
  check(differs, "other seeds give other texts");

  // Hard length limit: without an end token the text stops before 47 characters are crossed.
  for (uint32_t seed = 1; seed < 50; ++seed) {
    Run c = run(seed, -1, true);
    check(c.end == slm::WriteEnd::LengthLimit, "no end token: length limit");
    check(characters(c.sink.body()) <= vocab::kMaxTextCharacters, "body within 47 characters");
    check(characters(c.sink.body()) >= vocab::kMaxTextCharacters - 20, "limit reached, not stopped early");
  }

  // End token as the first body piece: emoji only.
  Run e = run(7, 1);
  check(e.end == slm::WriteEnd::Done && e.sink.emojis.size() == 1 && e.sink.words.empty(),
        "emoji-only text");

  // Cancel stops within one forward pass.
  Run f = run(99, -1, true, 2);
  check(f.end == slm::WriteEnd::Cancelled, "cancel ends the text");
  check(f.model.position == f.sink.passesAtCancel, "no forward pass after the cancel");
  Run g;
  g.sink.model = &g.model;
  struct CancelNow {
    bool cancelled() { return true; }
    void emoji(uint16_t) {}
    void word(const char*) {}
  } now;
  check(slm::writeText(g.model, kCondition, 1, now) == slm::WriteEnd::Cancelled &&
            g.model.position == 0,
        "cancel before the first pass");

  // Sampling itself: top-1 is greedy; temperature spreads over the top k only.
  float logits[12] = {0, 5, 1, 9, 2, 8, 3, 7, 4, 6, 0.5f, 10};
  slm::SampleRandom random(3);
  check(slm::sampleTopK(logits, 0, 12, 0.8f, 1, random) == 11, "top-1 is the maximum");
  bool inTop = true;
  for (int i = 0; i < 500; ++i) {
    const int pick = slm::sampleTopK(logits, 0, 12, 0.8f, 8, random);
    inTop = inTop && logits[pick] >= 3.0f;
  }
  check(inTop, "samples stay within the top 8");
  check(slm::sampleTopK(logits, 2, 3, 0.8f, 8, random) >= 2, "sampling stays in its range");

  if (failures == 0) std::printf("text_writer: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
