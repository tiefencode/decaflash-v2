#include "slm/text_model_vocab.h"

#include <cassert>
#include <cstdio>
#include <cstring>

#include "panel_glyphs.h"
#include "reaction_texts.h"

using namespace decaflash::mainframe;
using namespace decaflash::mainframe::text_model;

int main() {
  static_assert(kOutputTokens <= kInputTokens, "output tokens are input tokens too");
  assert(kTokens[kEndToken].kind == TokenKind::End);
  assert(kTokens[kStartToken].kind == TokenKind::Start);
  assert(kFirstMoodToken - kFirstEventToken == kTextModelEvents);
  assert(kInputTokens - kFirstMoodToken == 15);

  assert(kFirstTextToken == 1 && kFirstTextToken < kFirstWordToken &&
         kFirstWordToken < kFirstEmojiToken);
  assert(std::strcmp(kTokens[kHyphenToken].utf8, "-") == 0);

  // Every output token is drawable by the panel: punctuation and emoji are one
  // glyph each, words are made only of A-Z, 0-9, ' and the umlaut glyphs.
  for (uint16_t id = kFirstTextToken; id < kOutputTokens; ++id) {
    const Token& token = kTokens[id];
    const size_t bytes = std::strlen(token.utf8);
    if (id >= kFirstEmojiToken) {
      const auto glyph = panelGlyphToken(token.utf8, bytes);
      assert(token.kind == TokenKind::Emoji && glyph.bytes == bytes);
      assert(glyph.glyph == token.glyph && glyph.glyph != PanelGlyph::None);
    } else if (id < kFirstWordToken) {
      assert(token.kind == TokenKind::Punct && bytes == 1 &&
             std::strchr(".,!?-", token.utf8[0]) != nullptr);
    } else {
      assert(token.kind == TokenKind::Word && bytes > 0);
      for (size_t at = 0; at < bytes;) {
        const char c = token.utf8[at];
        const auto glyph = panelGlyphToken(token.utf8 + at, bytes - at);
        const bool ascii = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '\'';
        const bool umlaut = glyph.glyph == PanelGlyph::Aumlaut ||
                            glyph.glyph == PanelGlyph::Oumlaut ||
                            glyph.glyph == PanelGlyph::Uumlaut;
        assert((ascii && glyph.bytes == 1) || umlaut);
        at += glyph.bytes;
      }
      // Sorted by bytes, so the firmware can search them.
      if (id > kFirstWordToken) assert(std::strcmp(kTokens[id - 1].utf8, token.utf8) < 0);
    }
  }

  // Spacing: a space before words, none before punctuation or after a hyphen.
  const uint16_t word = kFirstWordToken;
  assert(!spaceBefore(kEndToken, word));
  assert(spaceBefore(word, word));
  assert(spaceBefore(kFirstTextToken, word));  // after "."
  assert(!spaceBefore(word, kFirstTextToken));
  assert(!spaceBefore(word, kHyphenToken) && !spaceBefore(kHyphenToken, word));

  // Every curated emoji has exactly one token.
  unsigned curated = 0;
  for (unsigned value = static_cast<unsigned>(PanelGlyph::Smile);
       value <= static_cast<unsigned>(PanelGlyph::Urn); ++value) {
    unsigned found = 0;
    for (uint16_t id = 0; id < kInputTokens; ++id) {
      if (kTokens[id].glyph == static_cast<PanelGlyph>(value)) ++found;
    }
    assert(found == 1);
    ++curated;
  }
  assert(curated == 47 && kOutputTokens - kFirstEmojiToken == 47);

  // No token text appears twice.
  for (uint16_t a = 0; a < kInputTokens; ++a) {
    for (uint16_t b = a + 1; b < kInputTokens; ++b) {
      assert(std::strcmp(kTokens[a].utf8, kTokens[b].utf8) != 0);
    }
  }
  std::printf("PASS: text model vocabulary: %u output, %u input tokens (%u words), all drawable\n",
              kOutputTokens, kInputTokens, kFirstEmojiToken - kFirstWordToken);
}
