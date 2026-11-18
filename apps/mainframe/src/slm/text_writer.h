// Text model (add-text-model, task 7.1): writes one text for a condition.
// The first output token is restricted to the curated emoji rows and only those
// rows of the output head are computed (design.md Decisions 3 and 4); after it
// come words and punctuation until the end token or the hard length limit of
// Decision 7. Each token is sampled from the top-k logits at a temperature with
// a per-request seed: the emoji and the first text token wide, every later
// token narrower (design.md Decision 9, sampling schedule). Finished words are
// delivered as they are complete, so the panel can show the text word by word. Hardware independent: the host test
// runs it with a fake model.
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
constexpr int kRestTopK = 3;
constexpr int kTextTopK = kOpeningTopK;   // largest top-k, size of the sampling buffer
constexpr int kConditionTokens = 7;       // start, event, five mood levels
constexpr int kMaxOutputPieces = 48;      // loop bound; the real limit is characters
constexpr size_t kWordCapacity = 48;      // bytes of one delivered word, terminator included

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

// Index of the sampled row among logits[first, first + count): top-k by value
// (lower index first on ties), softmax at `temperature`, one draw from `random`.
inline int sampleTopK(const float* logits, int first, int count, float temperature, int topK,
                      SampleRandom& random) {
  int best[kTextTopK];
  int kept = 0;
  if (topK > kTextTopK) topK = kTextTopK;
  for (int row = first; row < first + count; ++row) {
    if (kept == topK && logits[row] <= logits[best[kept - 1]]) continue;
    int at = kept < topK ? kept++ : kept - 1;
    while (at > 0 && logits[row] > logits[best[at - 1]]) {
      best[at] = best[at - 1];
      --at;
    }
    best[at] = row;
  }
  if (kept == 0) return -1;
  float weights[kTextTopK];
  float sum = 0.0f;
  for (int i = 0; i < kept; ++i) {
    weights[i] = expf((logits[best[i]] - logits[best[0]]) / temperature);
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

// Model: const float* forward(int token, int firstRow, int rowCount), as
// slm::Kernel, already reset by the caller.
// Sink: bool cancelled(); void emoji(uint16_t token); void word(const char* utf8).
// Words are delivered with their punctuation attached and without spaces.
template <class Model, class Sink>
WriteEnd writeText(Model& model, const uint16_t (&condition)[kConditionTokens], uint32_t seed,
                   Sink& sink) {
  namespace vocab = decaflash::mainframe::text_model;
  SampleRandom random(seed);
  const float* logits = nullptr;
  for (int i = 0; i < kConditionTokens; ++i) {
    if (sink.cancelled()) return WriteEnd::Cancelled;
    const bool last = i == kConditionTokens - 1;
    logits = model.forward(condition[i], last ? vocab::kFirstEmojiToken : 0,
                           last ? vocab::kOutputTokens - vocab::kFirstEmojiToken : 0);
    if (logits == nullptr) return WriteEnd::Failed;
  }
  const int emoji = sampleTopK(logits, vocab::kFirstEmojiToken,
                               vocab::kOutputTokens - vocab::kFirstEmojiToken, kOpeningTemperature,
                               kOpeningTopK, random);
  if (emoji < 0) return WriteEnd::Failed;
  sink.emoji(static_cast<uint16_t>(emoji));

  char word[kWordCapacity] = {};
  size_t wordBytes = 0;
  int bodyCharacters = 0;
  uint16_t previous = vocab::kEndToken;
  int input = emoji;
  WriteEnd end = WriteEnd::LengthLimit;
  for (int piece = 0; piece < kMaxOutputPieces; ++piece) {
    if (sink.cancelled()) return WriteEnd::Cancelled;
    logits = model.forward(input, 0, vocab::kFirstEmojiToken);
    if (logits == nullptr) return WriteEnd::Failed;
    const bool opening = piece == 0;
    const int next = sampleTopK(logits, 0, vocab::kFirstEmojiToken,
                                opening ? kOpeningTemperature : kRestTemperature,
                                opening ? kOpeningTopK : kRestTopK, random);
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
      sink.word(word);
      wordBytes = 0;
    }
    if (wordBytes + textBytes >= kWordCapacity) break;
    memcpy(word + wordBytes, text, textBytes);
    wordBytes += textBytes;
    word[wordBytes] = '\0';
    bodyCharacters = characters;
    previous = id;
    input = next;
  }
  if (wordBytes) sink.word(word);
  return end;
}

}  // namespace slm
