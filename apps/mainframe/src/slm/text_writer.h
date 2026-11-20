// Text model (add-text-model, task 7.1): writes one text for a condition.
// The first output token is restricted to the curated emoji rows and only those
// rows of the output head are computed (design.md Decisions 3 and 4); after it
// come words and punctuation until the end token or the hard length limit of
// Decision 7. Each token is sampled from the top-k logits at a temperature with
// a per-request seed: the emoji and the first text token wide, every later
// token narrower (design.md Decision 9, sampling schedule). Finished words are
// delivered as they are complete, so the panel can show the text word by word.
// Every delivered piece carries the confidence of the tokens that formed it
// (add-text-model-confidence-gate): the log-probability difference of the two
// best tokens of the step's sampling distribution, which the confidence gate
// averages. Hardware independent: the host test runs it with a fake model.
//
// Writer settings (improve-text-model, step 2; values in its design.md): the
// sampling schedule, a repetition block against the texts written recently
// (RecentTexts, kept by the caller), a word mask per event (only the words of
// the event's corpus lines) and a penalty on words that occur once in the
// corpus. All of them act on the logits before the top-k.
#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "text_model_vocab.h"

namespace slm {

// design.md calibration values: the opening (emoji and first text token) and
// every later token.
constexpr float kOpeningTemperature = 0.8f;
constexpr int kOpeningTopK = 8;
constexpr float kRestTemperature = 0.5f;
constexpr int kRestTopK = 4;              // 3 before improve-text-model step 2
constexpr int kTextTopK = 16;             // largest top-k, size of the sampling buffer
constexpr int kConditionTokens = 7;       // start, event, five mood levels
constexpr int kMaxOutputPieces = 48;      // loop bound; the real limit is characters
constexpr size_t kWordCapacity = 48;      // bytes of one delivered word, terminator included
constexpr int kMaxWordTokens = 16;        // tokens of one delivered word (confidence values)
constexpr float kSingleChoiceMargin = 99.0f;  // margin when only one token was possible

enum class WriteEnd : uint8_t {
  Done,         // the model wrote the end token
  LengthLimit,  // ended before the word that would cross the hard length limit
  Cancelled,    // the sink cancelled between forward passes
  Failed,       // the model refused a step
};

inline const char* writeEndName(WriteEnd end) {
  switch (end) {
    case WriteEnd::Done: return "end";
    case WriteEnd::LengthLimit: return "limit";
    case WriteEnd::Cancelled: return "cancelled";
    case WriteEnd::Failed: return "failed";
  }
  return "?";
}

// Small deterministic generator (xorshift32). The seed is mixed first, so
// small or similar seeds do not start with similar draws; 0 is replaced.
class SampleRandom {
 public:
  explicit SampleRandom(uint32_t seed) : state_(mix(seed)) {}
  // Uniform in [0, 1).
  float next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return static_cast<float>(state_ >> 8) * (1.0f / 16777216.0f);
  }

 private:
  static uint32_t mix(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x ? x : 0x9E3779B9u;
  }
  uint32_t state_;
};

inline bool rowBit(const uint32_t* bits, int row) {
  return bits && ((bits[row >> 5] >> (row & 31)) & 1u);
}
inline void setRowBit(uint32_t* bits, int row) { bits[row >> 5] |= 1u << (row & 31); }

// What one sampling step changes before the top-k, as bitmaps over the output
// rows (a null bitmap changes nothing): only `allowed` rows are considered,
// `banned` rows are skipped, `penalised` rows lose `penalty` and `rare` rows
// lose `rarePenalty` logit points.
struct SampleAdjust {
  const uint32_t* allowed = nullptr;
  const uint32_t* banned = nullptr;
  const uint32_t* penalised = nullptr;
  float penalty = 0.0f;
  const uint32_t* rare = nullptr;
  float rarePenalty = 0.0f;
};

// Index of the sampled row among logits[first, first + count): top-k by value
// (lower index first on ties), softmax at `temperature`, one draw from `random`.
// `adjust` (optional) masks and lowers rows before the top-k. If `margin` is
// given, it receives the confidence of the step: the difference of the two
// best (adjusted) logits divided by the temperature, which is the
// log-probability difference of the two best tokens in the sampling
// distribution. Reading it does not change the draw.
inline int sampleTopK(const float* logits, int first, int count, float temperature, int topK,
                      SampleRandom& random, float* margin = nullptr,
                      const SampleAdjust* adjust = nullptr) {
  int best[kTextTopK];
  float bestValue[kTextTopK];
  int kept = 0;
  if (topK > kTextTopK) topK = kTextTopK;
  for (int row = first; row < first + count; ++row) {
    float value = logits[row];
    if (adjust) {
      if (adjust->allowed && !rowBit(adjust->allowed, row)) continue;
      if (rowBit(adjust->banned, row)) continue;
      if (rowBit(adjust->penalised, row)) value -= adjust->penalty;
      if (rowBit(adjust->rare, row)) value -= adjust->rarePenalty;
    }
    if (kept == topK && value <= bestValue[kept - 1]) continue;
    int at = kept < topK ? kept++ : kept - 1;
    while (at > 0 && value > bestValue[at - 1]) {
      best[at] = best[at - 1];
      bestValue[at] = bestValue[at - 1];
      --at;
    }
    best[at] = row;
    bestValue[at] = value;
  }
  if (kept == 0) return -1;
  if (margin) {
    *margin = kept > 1 ? (bestValue[0] - bestValue[1]) / temperature : kSingleChoiceMargin;
  }
  float weights[kTextTopK];
  float sum = 0.0f;
  for (int i = 0; i < kept; ++i) {
    weights[i] = expf((bestValue[i] - bestValue[0]) / temperature);
    sum += weights[i];
  }
  float draw = random.next() * sum;
  for (int i = 0; i < kept; ++i) {
    draw -= weights[i];
    if (draw < 0.0f) return best[i];
  }
  return best[kept - 1];
}

// Characters (UTF-8 code points) of a text piece.
inline int pieceCharacters(const char* utf8) {
  int count = 0;
  for (const char* c = utf8; *c; ++c) {
    if ((static_cast<uint8_t>(*c) & 0xC0) != 0x80) ++count;
  }
  return count;
}

// Repetition block: what the writer avoids from the texts it wrote recently.
// texts == 0 switches it off. Defaults: the firmware's values
// (improve-text-model design.md, step 2); exact, wordPenalty and emojiPenalty
// were measured and are off.
struct RepetitionRule {
  int texts = 8;                // recent texts remembered (at most kRecentTextCapacity)
  bool exact = false;           // no text equal to a recent one (end token banned there)
  float wordPenalty = 0.0f;     // logit points off a word of a recent text
  float openingPenalty = 2.0f;  // logit points off the first word of a recent text, first word only
  int ngram = 3;                // ban completing a word sequence of this many tokens (0 off)
  float emojiPenalty = 0.0f;    // logit points off a recent emoji
};

// Everything the writer can be told; the defaults are the firmware's values
// (kWriterSettings).
struct WriterSettings {
  float openingTemperature = kOpeningTemperature;
  int openingTopK = kOpeningTopK;
  float restTemperature = kRestTemperature;
  int restTopK = kRestTopK;
  RepetitionRule repetition;
  bool eventMask = true;     // only the words of the event's corpus lines
  float rarePenalty = 2.0f;  // logit points off a word that occurs once in the corpus
};

inline constexpr WriterSettings kWriterSettings{};

// The writer before improve-text-model step 2: rest top-k 3, no block, no
// mask, no penalty (the harness baseline).
inline WriterSettings plainWriterSettings() {
  WriterSettings s{};
  s.restTopK = 3;
  s.repetition.texts = 0;
  s.repetition.openingPenalty = 0.0f;
  s.repetition.ngram = 0;
  s.eventMask = false;
  s.rarePenalty = 0.0f;
  return s;
}

constexpr int kRecentTextCapacity = 16;
constexpr int kRecentTextTokens = kMaxOutputPieces + 1;  // emoji and body pieces

// Memory of recent texts across requests, kept by the caller (the firmware
// keeps one for the whole run). writeText records the text it writes in
// `current`; the caller adds it with commit() when the text was shown.
struct RecentTexts {
  uint16_t tokens[kRecentTextCapacity][kRecentTextTokens] = {};
  uint8_t length[kRecentTextCapacity] = {};
  int count = 0;
  int next = 0;
  uint16_t current[kRecentTextTokens] = {};
  int currentLength = 0;
  // Scratch bitmaps of one request.
  uint32_t penalised[decaflash::mainframe::text_model::kOutputBitWords] = {};
  uint32_t banned[decaflash::mainframe::text_model::kOutputBitWords] = {};
  uint32_t emoji[decaflash::mainframe::text_model::kOutputBitWords] = {};
  uint32_t opening[decaflash::mainframe::text_model::kOutputBitWords] = {};

  void clear() {
    count = 0;
    next = 0;
    currentLength = 0;
  }
  void commit() {
    if (currentLength == 0) return;
    memcpy(tokens[next], current, sizeof(uint16_t) * currentLength);
    length[next] = static_cast<uint8_t>(currentLength);
    next = (next + 1) % kRecentTextCapacity;
    if (count < kRecentTextCapacity) ++count;
    currentLength = 0;
  }
  // Text `age` back (0 = newest).
  const uint16_t* text(int age, int& n) const {
    const int at = (next - 1 - age + 2 * kRecentTextCapacity) % kRecentTextCapacity;
    n = length[at];
    return tokens[at];
  }
};

// Fills recent.banned for the next body step from the text so far
// (recent.current, index 0 is the emoji): `exact` bans the end token where the
// body so far equals a recent body; `ngram` bans every token that would
// complete a sequence of rule.ngram body tokens that a recent text contains.
inline void banRepeats(RecentTexts& recent, const RepetitionRule& rule, int remembered) {
  namespace vocab = decaflash::mainframe::text_model;
  memset(recent.banned, 0, sizeof(recent.banned));
  const uint16_t* cur = recent.current;
  const int len = recent.currentLength;
  for (int age = 0; age < remembered; ++age) {
    int n = 0;
    const uint16_t* t = recent.text(age, n);
    if (rule.exact && n == len && len > 1 &&
        memcmp(t + 1, cur + 1, sizeof(uint16_t) * (n - 1)) == 0) {
      setRowBit(recent.banned, vocab::kEndToken);
    }
    if (rule.ngram > 1) {
      const int context = rule.ngram - 1;
      if (len - 1 < context) continue;
      for (int i = 1; i + context < n; ++i) {
        if (memcmp(t + i, cur + len - context, sizeof(uint16_t) * context) == 0) {
          setRowBit(recent.banned, t[i + context]);
        }
      }
    }
  }
}

// Model: const float* forward(int token, int firstRow, int rowCount), as
// slm::Kernel, already reset by the caller.
// Sink: bool cancelled(); void emoji(uint16_t token, float margin);
//       void word(const char* utf8, const float* margins, int count);
//       void end(float margin).
// Words are delivered with their punctuation attached and without spaces, with
// the margins of their tokens in order; end() is called with the margin of the
// end token when the model wrote it (not after the length limit).
// `recent` (optional) is the memory of the repetition block; without it the
// block is off.
template <class Model, class Sink>
WriteEnd writeText(Model& model, const uint16_t (&condition)[kConditionTokens], uint32_t seed,
                   Sink& sink, const WriterSettings& settings = kWriterSettings,
                   RecentTexts* recent = nullptr) {
  namespace vocab = decaflash::mainframe::text_model;
  SampleRandom random(seed);
  const RepetitionRule& rule = settings.repetition;
  const bool block = recent != nullptr && rule.texts > 0;
  const int remembered = block ? (rule.texts < recent->count ? rule.texts : recent->count) : 0;
  if (recent) recent->currentLength = 0;
  SampleAdjust emojiAdjust;
  SampleAdjust bodyAdjust;
  const int event = condition[1] - vocab::kFirstEventToken;
  if (settings.eventMask && event >= 0 && event < 12) bodyAdjust.allowed = vocab::kEventRows[event];
  if (settings.rarePenalty > 0.0f) {
    bodyAdjust.rare = vocab::kOnceWordRows;
    bodyAdjust.rarePenalty = settings.rarePenalty;
  }
  if (block) {
    memset(recent->penalised, 0, sizeof(recent->penalised));
    memset(recent->emoji, 0, sizeof(recent->emoji));
    memset(recent->opening, 0, sizeof(recent->opening));
    for (int age = 0; age < remembered; ++age) {
      int n = 0;
      const uint16_t* t = recent->text(age, n);
      if (n > 0) setRowBit(recent->emoji, t[0]);
      if (n > 1) setRowBit(recent->opening, t[1]);
      for (int i = 1; i < n; ++i) {
        if (vocab::kTokens[t[i]].kind == vocab::TokenKind::Word) setRowBit(recent->penalised, t[i]);
      }
    }
    if (rule.emojiPenalty > 0.0f) {
      emojiAdjust.penalised = recent->emoji;
      emojiAdjust.penalty = rule.emojiPenalty;
    }
    if (rule.wordPenalty > 0.0f) {
      bodyAdjust.penalised = recent->penalised;
      bodyAdjust.penalty = rule.wordPenalty;
    }
    bodyAdjust.banned = recent->banned;
  }
  // The first body token: the body adjustment plus the opening penalty, which
  // replaces the word penalty there when it is set.
  SampleAdjust openingAdjust = bodyAdjust;
  if (block && rule.openingPenalty > 0.0f) {
    openingAdjust.penalised = recent->opening;
    openingAdjust.penalty = rule.openingPenalty;
  }
  const float* logits = nullptr;
  for (int i = 0; i < kConditionTokens; ++i) {
    if (sink.cancelled()) return WriteEnd::Cancelled;
    const bool last = i == kConditionTokens - 1;
    logits = model.forward(condition[i], last ? vocab::kFirstEmojiToken : 0,
                           last ? vocab::kOutputTokens - vocab::kFirstEmojiToken : 0);
    if (logits == nullptr) return WriteEnd::Failed;
  }
  float margin = 0.0f;
  const int emoji = sampleTopK(logits, vocab::kFirstEmojiToken,
                               vocab::kOutputTokens - vocab::kFirstEmojiToken,
                               settings.openingTemperature, settings.openingTopK, random, &margin,
                               &emojiAdjust);
  if (emoji < 0) return WriteEnd::Failed;
  if (recent) recent->current[recent->currentLength++] = static_cast<uint16_t>(emoji);
  sink.emoji(static_cast<uint16_t>(emoji), margin);

  char word[kWordCapacity] = {};
  size_t wordBytes = 0;
  float wordMargins[kMaxWordTokens] = {};
  int wordTokens = 0;
  int bodyCharacters = 0;
  uint16_t previous = vocab::kEndToken;
  int input = emoji;
  WriteEnd end = WriteEnd::LengthLimit;
  for (int piece = 0; piece < kMaxOutputPieces; ++piece) {
    if (sink.cancelled()) return WriteEnd::Cancelled;
    logits = model.forward(input, 0, vocab::kFirstEmojiToken);
    if (logits == nullptr) return WriteEnd::Failed;
    const bool opening = piece == 0;
    if (block) banRepeats(*recent, rule, remembered);
    const int next = sampleTopK(logits, 0, vocab::kFirstEmojiToken,
                                opening ? settings.openingTemperature : settings.restTemperature,
                                opening ? settings.openingTopK : settings.restTopK, random, &margin,
                                opening ? &openingAdjust : &bodyAdjust);
    if (next < 0) return WriteEnd::Failed;
    if (next == vocab::kEndToken) {
      end = WriteEnd::Done;
      break;
    }
    const uint16_t id = static_cast<uint16_t>(next);
    const bool space = vocab::spaceBefore(previous, id);
    const char* text = vocab::kTokens[id].utf8;
    const size_t textBytes = strlen(text);
    const int characters = bodyCharacters + (space ? 1 : 0) + pieceCharacters(text);
    if (characters > vocab::kMaxTextCharacters) break;  // end before the crossing word
    if (space && wordBytes) {
      sink.word(word, wordMargins, wordTokens);
      wordBytes = 0;
      wordTokens = 0;
    }
    if (wordBytes + textBytes >= kWordCapacity || wordTokens == kMaxWordTokens) break;
    if (recent && recent->currentLength < kRecentTextTokens) {
      recent->current[recent->currentLength++] = id;
    }
    wordMargins[wordTokens++] = margin;
    memcpy(word + wordBytes, text, textBytes);
    wordBytes += textBytes;
    word[wordBytes] = '\0';
    bodyCharacters = characters;
    previous = id;
    input = next;
  }
  if (wordBytes) sink.word(word, wordMargins, wordTokens);
  if (end == WriteEnd::Done) sink.end(margin);
  return end;
}

}  // namespace slm
