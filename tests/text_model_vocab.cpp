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

  // Every output token is drawable by the panel.
  for (uint16_t id = kFirstTextToken; id < kOutputTokens; ++id) {
    const Token& token = kTokens[id];
    const size_t bytes = std::strlen(token.utf8);
    const auto glyph = panelGlyphToken(token.utf8, bytes);
    assert(glyph.bytes == bytes);  // one panel glyph per token
    if (token.kind == TokenKind::Emoji) {
      assert(glyph.glyph == token.glyph && glyph.glyph != PanelGlyph::None);
      assert(id >= kFirstEmojiToken);
    } else {
      assert(token.kind == TokenKind::Text && id < kFirstEmojiToken);
      const bool ascii = bytes == 1 && token.utf8[0] >= ' ' && token.utf8[0] <= '~' &&
                         !(token.utf8[0] >= 'a' && token.utf8[0] <= 'z');
      const bool umlaut = glyph.glyph == PanelGlyph::Aumlaut ||
                          glyph.glyph == PanelGlyph::Oumlaut ||
                          glyph.glyph == PanelGlyph::Uumlaut;
      assert(ascii || umlaut);
    }
  }

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
  std::printf("PASS: text model vocabulary: %u output, %u input tokens, all drawable\n",
              kOutputTokens, kInputTokens);
}
