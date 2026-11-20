// Host test of the text model writer (apps/mainframe/src/slm/text_writer.h,
// add-text-model task 7.1) with a fake model: masks, sampling with a seed,
// hard length limit, end token and cancel; and the writer settings of
// improve-text-model step 2: defaults, word mask per event, once-only word
// penalty and the repetition block.
#include <algorithm>
#include <cmath>
#include <cstring>
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
  std::vector<int> ranks;  // rank of each chosen output token in its pass (0 = emoji)
  std::vector<int> chosen;  // each chosen output token (0 = emoji)

  const float* forward(int token, int firstRow, int rowCount) {
    if (token < 0 || token >= vocab::kInputTokens) return nullptr;
    if (position >= slm::kConditionTokens) {  // `token` was chosen from the previous pass
      int rank = 0;
      for (int row = firstRows.back(); row < firstRows.back() + rowCounts.back(); ++row) {
        if (logits[row] > logits[token]) ++rank;
      }
      ranks.push_back(rank);
      chosen.push_back(token);
    }
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
  std::vector<float> margins;      // all margins in delivery order
  std::vector<int> wordTokens;     // margins per delivered word
  int ends = 0;
  void emoji(uint16_t token, float margin) {
    emojis.push_back(token);
    margins.push_back(margin);
  }
  void word(const char* utf8, const float* values, int count) {
    words.emplace_back(utf8);
    wordTokens.push_back(count);
    margins.insert(margins.end(), values, values + count);
  }
  void end(float margin) {
    ++ends;
    margins.push_back(margin);
  }
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

// The writer before improve-text-model step 2; the checks of add-text-model
// run with it.
const slm::WriterSettings kPlain = slm::plainWriterSettings();

Run run(uint32_t seed, int endAtPiece, bool noEnd = false, int cancelAfterEmoji = -1,
        const slm::WriterSettings& settings = kPlain, slm::RecentTexts* recent = nullptr) {
  Run r;
  r.model.endAtPiece = endAtPiece;
  r.model.noEnd = noEnd;
  r.sink.model = &r.model;
  r.sink.cancelAfterEmoji = cancelAfterEmoji;
  r.end = slm::writeText(r.model, kCondition, seed, r.sink, settings, recent);
  return r;
}

// Body tokens of a run: the chosen tokens after the emoji that were fed back
// to the model (a final end token or a token past the length limit is not).
std::vector<int> bodyTokens(const Run& r) {
  std::vector<int> out;
  for (size_t i = 1; i < r.model.chosen.size(); ++i) out.push_back(r.model.chosen[i]);
  return out;
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
    void emoji(uint16_t, float) {}
    void word(const char*, const float*, int) {}
    void end(float) {}
  } now;
  check(slm::writeText(g.model, kCondition, 1, now, kPlain) == slm::WriteEnd::Cancelled &&
            g.model.position == 0,
        "cancel before the first pass");

  // Schedule: the emoji and the first text token come from the top 8, every
  // later token from the top 3.
  int openingMax = 0, restMax = 0;
  for (uint32_t seed = 1; seed < 200; ++seed) {
    Run s = run(seed, -1, true);
    for (size_t i = 0; i < s.model.ranks.size(); ++i) {
      int& most = i < 2 ? openingMax : restMax;
      if (s.model.ranks[i] > most) most = s.model.ranks[i];
    }
  }
  check(openingMax >= kPlain.restTopK && openingMax < kPlain.openingTopK,
        "opening tokens sampled from the wide top-k");
  check(restMax < kPlain.restTopK, "later tokens sampled from the narrow top-k");

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

  // Confidence (add-text-model-confidence-gate): the margin of the two best
  // logits divided by the temperature; reading it does not change the draw.
  float margin = -1.0f;
  slm::SampleRandom withMargin(11), without(11);
  bool same = true;
  for (int i = 0; i < 500; ++i) {
    const float temperature = i % 2 ? 0.8f : 0.5f;
    const int topK = i % 2 ? 8 : 3;
    same = same && slm::sampleTopK(logits, 0, 12, temperature, topK, withMargin, &margin) ==
                       slm::sampleTopK(logits, 0, 12, temperature, topK, without);
  }
  check(same, "sampling unchanged when the margin is read");
  slm::sampleTopK(logits, 0, 12, 0.5f, 3, random, &margin);
  check(std::fabs(margin - (10.0f - 9.0f) / 0.5f) < 1e-6f, "margin of the two best at the temperature");
  slm::sampleTopK(logits, 0, 12, 0.8f, 1, random, &margin);
  check(margin == slm::kSingleChoiceMargin, "single choice has the fixed margin");

  // Margins per piece: one for the emoji, one per word token, one for the end
  // token when the model wrote it.
  for (uint32_t seed = 1; seed < 50; ++seed) {
    Run m = run(seed, 6);
    int tokens = 0;
    for (int count : m.sink.wordTokens) tokens += count;
    check(m.sink.wordTokens.size() == m.sink.words.size(), "every word carries margins");
    check(m.end != slm::WriteEnd::Done || (m.sink.ends == 1 &&
          m.sink.margins.size() == static_cast<size_t>(1 + tokens + 1)),
          "emoji, word tokens and end each have a margin");
    for (int count : m.sink.wordTokens) check(count >= 1, "a word has at least one token");
    for (float value : m.sink.margins) check(value >= 0.0f, "margins are not negative");
    Run plain = run(seed, 6);
    check(plain.sink.words == m.sink.words, "fixed seed, same text with margins");
  }
  Run limit = run(5, -1, true);
  check(limit.end == slm::WriteEnd::LengthLimit && limit.sink.ends == 0,
        "no end margin after the length limit");


  // --- improve-text-model step 2: writer settings -----------------------------

  // Firmware values (design.md, step 2).
  const slm::WriterSettings& w = slm::kWriterSettings;
  check(w.openingTemperature == 0.8f && w.openingTopK == 8, "opening schedule unchanged");
  check(w.restTemperature == 0.5f && w.restTopK == 4, "rest: temperature 0.5, top-k 4");
  check(w.repetition.texts == 8 && w.repetition.openingPenalty == 2.0f && w.repetition.ngram == 3,
        "repetition block: 8 texts, opening penalty 2, word triples");
  check(!w.repetition.exact && w.repetition.wordPenalty == 0.0f && w.repetition.emojiPenalty == 0.0f,
        "measured and dropped parts of the block are off");
  check(w.eventMask && w.rarePenalty == 2.0f, "word mask per event on, once-only penalty 2");
  check(w.repetition.texts <= slm::kRecentTextCapacity, "memory fits the ring");

  // Rest schedule of the firmware: later tokens from the top 4.
  int restMaxNew = 0;
  slm::WriterSettings wide = kPlain;
  wide.restTopK = slm::kWriterSettings.restTopK;
  for (uint32_t seed = 1; seed < 200; ++seed) {
    Run s = run(seed, -1, true, -1, wide);
    for (size_t i = 2; i < s.model.ranks.size(); ++i) restMaxNew = std::max(restMaxNew, s.model.ranks[i]);
  }
  check(restMaxNew == slm::kWriterSettings.restTopK - 1, "later tokens from the top 4");

  // An empty adjustment changes no draw.
  {
    slm::SampleRandom r1(5), r2(5);
    slm::SampleAdjust none;
    bool equal = true;
    for (int i = 0; i < 300; ++i) {
      equal = equal && slm::sampleTopK(logits, 0, 12, 0.8f, 8, r1) ==
                           slm::sampleTopK(logits, 0, 12, 0.8f, 8, r2, nullptr, &none);
    }
    check(equal, "empty adjustment, same draws");
  }

  // Adjustment rows: allowed, banned, penalised, rare.
  {
    slm::SampleRandom r(9);
    uint32_t allowed[1] = {(1u << 2) | (1u << 4)};
    uint32_t banned[1] = {1u << 11};
    uint32_t low[1] = {1u << 3};
    slm::SampleAdjust only;
    only.allowed = allowed;
    bool inside = true;
    for (int i = 0; i < 200; ++i) {
      const int pick = slm::sampleTopK(logits, 0, 12, 0.8f, 8, r, nullptr, &only);
      inside = inside && (pick == 2 || pick == 4);
    }
    check(inside, "only allowed rows are drawn");
    slm::SampleAdjust ban;
    ban.banned = banned;
    check(slm::sampleTopK(logits, 0, 12, 0.8f, 1, r, nullptr, &ban) == 3, "banned maximum skipped");
    slm::SampleAdjust pen;
    pen.penalised = low;
    pen.penalty = 5.0f;
    pen.banned = banned;
    check(slm::sampleTopK(logits, 0, 12, 0.8f, 1, r, nullptr, &pen) == 5, "penalised row drops");
    slm::SampleAdjust rare;
    rare.rare = low;
    rare.rarePenalty = 5.0f;
    rare.banned = banned;
    check(slm::sampleTopK(logits, 0, 12, 0.8f, 1, r, nullptr, &rare) == 5, "rare row drops");
    float m = 0.0f;
    slm::sampleTopK(logits, 0, 12, 0.5f, 3, r, &m, &ban);
    check(std::fabs(m - (9.0f - 8.0f) / 0.5f) < 1e-6f, "margin on the adjusted rows");
  }

  // Word mask per event: every body token is a word of the event's corpus
  // lines, the end token or punctuation.
  {
    slm::WriterSettings masked = kPlain;
    masked.eventMask = true;
    const int event = kCondition[1] - vocab::kFirstEventToken;
    bool inMask = true, differs = false;
    for (uint32_t seed = 1; seed < 100; ++seed) {
      Run m = run(seed, -1, true, -1, masked);
      Run p = run(seed, -1, true);
      for (int t : bodyTokens(m)) inMask = inMask && slm::rowBit(vocab::kEventRows[event], t);
      differs = differs || bodyTokens(m) != bodyTokens(p);
    }
    check(inMask, "masked text uses only the event's rows");
    check(differs, "the mask changes texts");
    check(slm::rowBit(vocab::kEventRows[event], vocab::kEndToken), "end token always allowed");
    for (int t = vocab::kFirstTextToken; t < vocab::kFirstWordToken; ++t) {
      check(slm::rowBit(vocab::kEventRows[event], t), "punctuation always allowed");
    }
  }

  // Once-only penalty: a large one keeps once-only words out.
  {
    slm::WriterSettings rare = kPlain;
    rare.rarePenalty = 1000.0f;
    bool none = true, some = false;
    for (uint32_t seed = 1; seed < 100; ++seed) {
      for (int t : bodyTokens(run(seed, -1, true, -1, rare))) none = none && !slm::rowBit(vocab::kOnceWordRows, t);
      for (int t : bodyTokens(run(seed, -1, true))) some = some || slm::rowBit(vocab::kOnceWordRows, t);
    }
    check(some, "the fake model writes once-only words without the penalty");
    check(none, "no once-only word with a large penalty");
  }

  // Repetition memory: ring of kRecentTextCapacity texts, newest first.
  {
    static slm::RecentTexts ring;
    for (int i = 0; i < 20; ++i) {
      ring.current[0] = static_cast<uint16_t>(vocab::kFirstEmojiToken);
      ring.current[1] = static_cast<uint16_t>(100 + i);
      ring.currentLength = 2;
      ring.commit();
    }
    int n = 0;
    check(ring.count == slm::kRecentTextCapacity, "ring keeps its capacity");
    check(ring.text(0, n)[1] == 119 && n == 2, "newest text first");
    check(ring.text(slm::kRecentTextCapacity - 1, n)[1] == 104, "oldest kept text last");
    ring.currentLength = 0;
    ring.commit();
    check(ring.count == slm::kRecentTextCapacity && ring.text(0, n)[1] == 119, "empty text not kept");

    // Word triples and exact repeats.
    ring.clear();
    const uint16_t e = vocab::kFirstEmojiToken;
    const uint16_t old[] = {e, 10, 11, 12, 13};
    memcpy(ring.current, old, sizeof(old));
    ring.currentLength = 5;
    ring.commit();
    slm::RepetitionRule rule;
    rule.ngram = 3;
    rule.exact = true;
    const uint16_t now1[] = {e, 10, 11};
    memcpy(ring.current, now1, sizeof(now1));
    ring.currentLength = 3;
    slm::banRepeats(ring, rule, 1);
    check(slm::rowBit(ring.banned, 12) && !slm::rowBit(ring.banned, 13), "triple completion banned");
    check(!slm::rowBit(ring.banned, vocab::kEndToken), "no exact ban before the text is equal");
    const uint16_t now2[] = {e, 10, 11, 12, 13};
    memcpy(ring.current, now2, sizeof(now2));
    ring.currentLength = 5;
    slm::banRepeats(ring, rule, 1);
    check(slm::rowBit(ring.banned, vocab::kEndToken), "exact repeat: end token banned");
    rule.exact = false;
    slm::banRepeats(ring, rule, 1);
    check(!slm::rowBit(ring.banned, vocab::kEndToken), "exact off: end token free");
    slm::banRepeats(ring, rule, 0);
    check(!slm::rowBit(ring.banned, 12), "nothing remembered, nothing banned");
  }

  // The block in the writer: the same seed twice in a row.
  {
    static slm::RecentTexts recent;
    slm::WriterSettings block = kPlain;
    block.repetition.texts = 8;
    block.repetition.openingPenalty = 1000.0f;
    recent.clear();
    Run first = run(21, 5, false, -1, block, &recent);
    Run uncommitted = run(21, 5, false, -1, block, &recent);
    check(bodyTokens(first) == bodyTokens(uncommitted), "an uncommitted text is not remembered");
    recent.commit();
    Run second = run(21, 5, false, -1, block, &recent);
    check(!bodyTokens(first).empty() && !bodyTokens(second).empty() &&
              bodyTokens(first)[0] != bodyTokens(second)[0],
          "opening penalty: another first word after a remembered text");
    Run without = run(21, 5, false, -1, block);
    check(bodyTokens(without) == bodyTokens(first), "no memory, no block");

    slm::WriterSettings triples = kPlain;
    triples.repetition.texts = 8;
    triples.repetition.ngram = 3;
    for (uint32_t seed = 1; seed < 40; ++seed) {
      recent.clear();
      Run a1 = run(seed, -1, true, -1, triples, &recent);
      recent.commit();
      Run a2 = run(seed, -1, true, -1, triples, &recent);
      // The fake model sees only kept tokens (the one that crossed the
      // length limit is never fed back).
      const std::vector<int> t1 = bodyTokens(a1), t2 = bodyTokens(a2);
      bool shared = false;
      for (size_t i = 0; i + 2 < t2.size() && !shared; ++i) {
        for (size_t j = 0; j + 2 < t1.size() && !shared; ++j) {
          shared = t2[i] == t1[j] && t2[i + 1] == t1[j + 1] && t2[i + 2] == t1[j + 2];
        }
      }
      check(!shared, "no word triple of the remembered text");
    }
  }

  if (failures == 0) std::printf("text_writer: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
