// Form check for text model texts (add-text-model, design.md Decisions 6, 7, 9).
// Host only. A text is written as "<emoji> <words>", optionally after a tag
// prefix that ends in a tab (the corpus layout); the space after the emoji is
// for reading and is not a model piece.
#pragma once

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "slm/text_model_vocab.h"

namespace decaflash::mainframe::text_model {

// Hard limit of output pieces, emoji included, end token excluded.
constexpr size_t kMaxOutputPieces = 48;

enum class FormResult : uint8_t {
  Ok,
  Empty,             // no emoji and no words
  UnknownCharacter,  // a character outside the text alphabet and curated emoji
  NoLeadingEmoji,    // the first piece is not a curated emoji
  EmojiInText,       // a second emoji after the first
  NoWords,           // only the emoji
  TooLong,           // more pieces than the hard limit
  Copy,              // the words equal a training text
};

inline const char* formResultName(FormResult result) {
  switch (result) {
    case FormResult::Ok: return "ok";
    case FormResult::Empty: return "empty";
    case FormResult::UnknownCharacter: return "unknown character";
    case FormResult::NoLeadingEmoji: return "no leading emoji";
    case FormResult::EmojiInText: return "emoji in the text";
    case FormResult::NoWords: return "no words";
    case FormResult::TooLong: return "too long";
    case FormResult::Copy: return "copy of a training text";
  }
  return "?";
}

// The text after the last tab, so corpus lines can be passed as they are.
inline std::string stripTags(const std::string& line) {
  const size_t tab = line.rfind('\t');
  return tab == std::string::npos ? line : line.substr(tab + 1);
}

// Greedy longest match over the output tokens (end excluded). Returns false at
// the first character that no token covers.
inline bool encodeOutput(const std::string& text, std::vector<uint16_t>& tokens) {
  tokens.clear();
  size_t at = 0;
  while (at < text.size()) {
    uint16_t best = 0;
    size_t bestBytes = 0;
    for (uint16_t id = kFirstTextToken; id < kOutputTokens; ++id) {
      const size_t bytes = std::strlen(kTokens[id].utf8);
      if (bytes > bestBytes && text.compare(at, bytes, kTokens[id].utf8) == 0) {
        best = id;
        bestBytes = bytes;
      }
    }
    if (!bestBytes) return false;
    tokens.push_back(best);
    at += bestBytes;
  }
  return true;
}

// Model pieces of a text: the emoji, then the words without the reading space
// after the emoji and without trailing spaces.
inline bool modelPieces(const std::string& text, std::vector<uint16_t>& pieces) {
  std::vector<uint16_t> tokens;
  if (!encodeOutput(text, tokens)) return false;
  pieces.clear();
  size_t at = 0;
  if (!tokens.empty() && kTokens[tokens[0]].kind == TokenKind::Emoji) {
    pieces.push_back(tokens[0]);
    at = 1;
  }
  while (at < tokens.size() && kTokens[tokens[at]].utf8[0] == ' ' &&
         kTokens[tokens[at]].kind == TokenKind::Text) {
    ++at;
  }
  size_t end = tokens.size();
  while (end > at && kTokens[tokens[end - 1]].utf8[0] == ' ' &&
         kTokens[tokens[end - 1]].kind == TokenKind::Text) {
    --end;
  }
  pieces.insert(pieces.end(), tokens.begin() + at, tokens.begin() + end);
  return true;
}

// The words of a text: everything after the leading emoji, trimmed.
inline std::string wordsOf(const std::string& text) {
  std::vector<uint16_t> pieces;
  if (!modelPieces(text, pieces)) return {};
  std::string words;
  for (size_t i = 0; i < pieces.size(); ++i) {
    if (i == 0 && kTokens[pieces[0]].kind == TokenKind::Emoji) continue;
    words += kTokens[pieces[i]].utf8;
  }
  return words;
}

// A panel text with its emoji at the end ("HUHU 🙂") in the model's order
// ("🙂 HUHU"). Texts that do not end in a curated emoji are returned as they are.
inline std::string fromPanelText(const std::string& text) {
  std::vector<uint16_t> tokens;
  if (!encodeOutput(text, tokens) || tokens.empty() ||
      kTokens[tokens.back()].kind != TokenKind::Emoji) {
    return text;
  }
  const std::string emoji = kTokens[tokens.back()].utf8;
  std::string words = text.substr(0, text.size() - emoji.size());
  while (!words.empty() && words.back() == ' ') words.pop_back();
  return emoji + " " + words;
}

inline FormResult checkForm(const std::string& line,
                            const std::set<std::string>& trainingWords = {}) {
  const std::string text = stripTags(line);
  std::vector<uint16_t> pieces;
  if (!modelPieces(text, pieces)) return FormResult::UnknownCharacter;
  if (pieces.empty()) return FormResult::Empty;
  if (kTokens[pieces[0]].kind != TokenKind::Emoji) return FormResult::NoLeadingEmoji;
  for (size_t i = 1; i < pieces.size(); ++i) {
    if (kTokens[pieces[i]].kind == TokenKind::Emoji) return FormResult::EmojiInText;
  }
  if (pieces.size() == 1) return FormResult::NoWords;
  if (pieces.size() > kMaxOutputPieces) return FormResult::TooLong;
  if (trainingWords.count(wordsOf(text))) return FormResult::Copy;
  return FormResult::Ok;
}

}  // namespace decaflash::mainframe::text_model
