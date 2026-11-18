// Form check for text model texts (add-text-model, design.md Decisions 6, 7, 9).
// Host only. A text is written as "<emoji> <body>", optionally after a tag
// prefix that ends in a tab (the corpus layout); the space after the emoji is
// for reading. The body is made of words of the vocabulary and punctuation,
// spaced as the decoder writes it (spaceBefore in slm/text_model_vocab.h).
#pragma once

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "slm/text_model_vocab.h"

namespace decaflash::mainframe::text_model {

enum class FormResult : uint8_t {
  Ok,
  Empty,             // no emoji and no words
  UnknownCharacter,  // a character outside the text alphabet and curated emoji
  NoLeadingEmoji,    // the first piece is not a curated emoji
  EmojiInText,       // a second emoji after the first
  UnknownWord,       // a word that is not in the vocabulary
  NoWords,           // only the emoji
  TooLong,           // more body characters than kMaxTextCharacters
  Spacing,           // spaces differ from the decoder's spacing
  Copy,              // the body equals a training text
};

inline const char* formResultName(FormResult result) {
  switch (result) {
    case FormResult::Ok: return "ok";
    case FormResult::Empty: return "empty";
    case FormResult::UnknownCharacter: return "unknown character";
    case FormResult::NoLeadingEmoji: return "no leading emoji";
    case FormResult::EmojiInText: return "emoji in the text";
    case FormResult::UnknownWord: return "unknown word";
    case FormResult::NoWords: return "no words";
    case FormResult::TooLong: return "too long";
    case FormResult::Spacing: return "spacing";
    case FormResult::Copy: return "copy of a training text";
  }
  return "?";
}

// The text after the last tab, so corpus lines can be passed as they are.
inline std::string stripTags(const std::string& line) {
  const size_t tab = line.rfind('\t');
  return tab == std::string::npos ? line : line.substr(tab + 1);
}

// Bytes of the word character at `at` (A-Z, 0-9, ', Ä, Ö, Ü), 0 if none.
inline size_t wordCharacterBytes(const std::string& text, size_t at) {
  const unsigned char c = static_cast<unsigned char>(text[at]);
  if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '\'') return 1;
  if (c == 0xC3 && at + 1 < text.size()) {
    const unsigned char d = static_cast<unsigned char>(text[at + 1]);
    if (d == 0x84 || d == 0x96 || d == 0x9C) return 2;  // Ä Ö Ü
  }
  return 0;
}

// Longest curated emoji at `at`, 0 if none.
inline uint16_t emojiAt(const std::string& text, size_t at, size_t& bytes) {
  uint16_t best = 0;
  bytes = 0;
  for (uint16_t id = kFirstEmojiToken; id < kOutputTokens; ++id) {
    const size_t length = std::strlen(kTokens[id].utf8);
    if (length > bytes && text.compare(at, length, kTokens[id].utf8) == 0) {
      best = id;
      bytes = length;
    }
  }
  return best;
}

// Word token of `word`, 0 if it is not in the vocabulary (binary search: the
// words are sorted by their UTF-8 bytes).
inline uint16_t wordToken(const std::string& word) {
  uint16_t low = kFirstWordToken;
  uint16_t high = kFirstEmojiToken;
  while (low < high) {
    const uint16_t mid = static_cast<uint16_t>((low + high) / 2);
    const int order = std::strcmp(kTokens[mid].utf8, word.c_str());
    if (order == 0) return mid;
    if (order < 0) {
      low = static_cast<uint16_t>(mid + 1);
    } else {
      high = mid;
    }
  }
  return 0;
}

inline uint16_t punctuationToken(char c) {
  for (uint16_t id = kFirstTextToken; id < kFirstWordToken; ++id) {
    if (kTokens[id].utf8[0] == c) return id;
  }
  return 0;
}

// Pieces of a text: emoji, words, punctuation in order, spaces skipped.
// Returns UnknownCharacter at the first character outside the alphabet and the
// curated emoji, else UnknownWord if a word has no token (it is left out of
// the pieces), otherwise Ok.
inline FormResult textPieces(const std::string& text, std::vector<uint16_t>& pieces) {
  pieces.clear();
  FormResult result = FormResult::Ok;
  size_t at = 0;
  while (at < text.size()) {
    if (text[at] == ' ') {
      ++at;
      continue;
    }
    size_t bytes = 0;
    if (const uint16_t emoji = emojiAt(text, at, bytes)) {
      pieces.push_back(emoji);
      at += bytes;
      continue;
    }
    if (const uint16_t mark = punctuationToken(text[at])) {
      pieces.push_back(mark);
      ++at;
      continue;
    }
    const size_t start = at;
    while (at < text.size() && (bytes = wordCharacterBytes(text, at)) > 0) at += bytes;
    if (at == start) return FormResult::UnknownCharacter;
    const uint16_t word = wordToken(text.substr(start, at - start));
    if (word) {
      pieces.push_back(word);
    } else {
      result = FormResult::UnknownWord;
    }
  }
  return result;
}

// Body text of pieces[from..] as the decoder writes it.
inline std::string bodyOf(const std::vector<uint16_t>& pieces, size_t from) {
  std::string body;
  uint16_t previous = kEndToken;
  for (size_t i = from; i < pieces.size(); ++i) {
    if (spaceBefore(previous, pieces[i])) body += ' ';
    body += kTokens[pieces[i]].utf8;
    previous = pieces[i];
  }
  return body;
}

// Characters (code points) of a UTF-8 string.
inline size_t characterCount(const std::string& text) {
  size_t count = 0;
  for (const char c : text) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
  }
  return count;
}

// The body of a text as the decoder writes it: everything after the leading
// emoji. Empty if the text has no valid pieces.
inline std::string wordsOf(const std::string& text) {
  std::vector<uint16_t> pieces;
  if (textPieces(text, pieces) != FormResult::Ok || pieces.empty()) return {};
  return bodyOf(pieces, kTokens[pieces[0]].kind == TokenKind::Emoji ? 1 : 0);
}

// A panel text with its emoji at the end ("HUHU 🙂") in the model's order
// ("🙂 HUHU"). Texts that do not end in a curated emoji are returned as they are.
inline std::string fromPanelText(const std::string& text) {
  for (size_t at = 0; at < text.size(); ++at) {
    size_t bytes = 0;
    if (emojiAt(text, at, bytes) && at + bytes == text.size()) {
      std::string words = text.substr(0, at);
      while (!words.empty() && words.back() == ' ') words.pop_back();
      return text.substr(at) + " " + words;
    }
  }
  return text;
}

inline FormResult checkForm(const std::string& line,
                            const std::set<std::string>& trainingWords = {}) {
  const std::string text = stripTags(line);
  std::vector<uint16_t> pieces;
  const FormResult parsed = textPieces(text, pieces);
  if (parsed == FormResult::UnknownCharacter) return parsed;
  if (pieces.empty() && parsed == FormResult::Ok) return FormResult::Empty;  // only spaces
  if (pieces.empty() || kTokens[pieces[0]].kind != TokenKind::Emoji) {
    return FormResult::NoLeadingEmoji;
  }
  for (size_t i = 1; i < pieces.size(); ++i) {
    if (kTokens[pieces[i]].kind == TokenKind::Emoji) return FormResult::EmojiInText;
  }
  if (parsed != FormResult::Ok) return parsed;
  if (pieces.size() == 1) return FormResult::NoWords;
  const std::string body = bodyOf(pieces, 1);
  if (characterCount(body) > kMaxTextCharacters) return FormResult::TooLong;
  if (text != std::string(kTokens[pieces[0]].utf8) + " " + body) return FormResult::Spacing;
  if (trainingWords.count(body)) return FormResult::Copy;
  return FormResult::Ok;
}

}  // namespace decaflash::mainframe::text_model
