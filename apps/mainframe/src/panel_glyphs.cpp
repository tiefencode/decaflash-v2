#include "panel_glyphs.h"

#include <cstring>

namespace decaflash::mainframe {
namespace {

struct GlyphMap {
  const char* utf8;
  PanelGlyph glyph;
};

// Multi-codepoint emoji must appear before their shorter prefix glyphs.
static constexpr GlyphMap kGlyphs[] = {
  {u8"🙂‍↔️", PanelGlyph::HeadTurn}, {u8"😵‍💫", PanelGlyph::DizzySpiral},
  {u8"👉👈", PanelGlyph::PointTogether},
  {u8"Ä", PanelGlyph::Aumlaut}, {u8"ä", PanelGlyph::Aumlaut},
  {u8"Ö", PanelGlyph::Oumlaut}, {u8"ö", PanelGlyph::Oumlaut},
  {u8"Ü", PanelGlyph::Uumlaut}, {u8"ü", PanelGlyph::Uumlaut},
  {u8"ß", PanelGlyph::SharpS},
  {u8"🙂", PanelGlyph::Smile}, {u8"👋", PanelGlyph::Wave}, {u8"👁", PanelGlyph::Eye},
  {u8"😵", PanelGlyph::Dizzy}, {u8"🤢", PanelGlyph::Nauseous},
  {u8"❗", PanelGlyph::Warning}, {u8"💢", PanelGlyph::AngerBurst},
  {u8"💬", PanelGlyph::Speech}, {u8"🎵", PanelGlyph::Music}, {u8"↻", PanelGlyph::Loop},
  {u8"🥰", PanelGlyph::Love}, {u8"😂", PanelGlyph::Laugh}, {u8"🫠", PanelGlyph::Melt},
  {u8"😍", PanelGlyph::HeartEyes}, {u8"😘", PanelGlyph::Kiss}, {u8"🥲", PanelGlyph::TearSmile},
  {u8"🤗", PanelGlyph::Hug}, {u8"🫢", PanelGlyph::Shy}, {u8"🥹", PanelGlyph::Plead},
  {u8"😢", PanelGlyph::Cry}, {u8"😭", PanelGlyph::Scream}, {u8"😡", PanelGlyph::Angry},
  {u8"💀", PanelGlyph::Skull}, {u8"💩", PanelGlyph::Poop}, {u8"🙈", PanelGlyph::SeeNoEvil},
  {u8"👌", PanelGlyph::OkHand}, {u8"🙌", PanelGlyph::Praise}, {u8"😳", PanelGlyph::Blush},
  {u8"🙄", PanelGlyph::EyeRoll}, {u8"🫩", PanelGlyph::Exhausted},
  {u8"😌", PanelGlyph::Relaxed}, {u8"😔", PanelGlyph::Sad}, {u8"🥺", PanelGlyph::Pleading},
  {u8"😤", PanelGlyph::Huffy}, {u8"🤬", PanelGlyph::Swearing},
  {u8"☠️", PanelGlyph::SkullCrossbones}, {u8"🖤", PanelGlyph::BlackHeart},
  {u8"🔪", PanelGlyph::Knife}, {u8"🥃", PanelGlyph::Glass}, {u8"🌚", PanelGlyph::Moon},
  {u8"⚰️", PanelGlyph::Coffin}, {u8"🚬", PanelGlyph::Cigarette},
  {u8"🪦", PanelGlyph::Grave}, {u8"⚱️", PanelGlyph::Urn},
};

uint8_t utf8Length(const char* text, size_t available) {
  if (!available) return 0;
  const uint8_t first = static_cast<uint8_t>(text[0]);
  if (first < 0x80) return 1;
  const uint8_t bytes = first >= 0xf0 ? 4 : first >= 0xe0 ? 3 : first >= 0xc0 ? 2 : 1;
  return bytes <= available ? bytes : 1;
}

}  // namespace

PanelGlyphToken panelGlyphToken(const char* text, size_t available) {
  if (!text || !available) return {};
  for (const auto& entry : kGlyphs) {
    const size_t bytes = std::strlen(entry.utf8);
    if (bytes <= available && std::memcmp(text, entry.utf8, bytes) == 0) {
      const bool letter = entry.glyph == PanelGlyph::Aumlaut ||
        entry.glyph == PanelGlyph::Oumlaut || entry.glyph == PanelGlyph::Uumlaut ||
        entry.glyph == PanelGlyph::SharpS;
      PanelGlyphToken token;
      token.glyph = entry.glyph;
      token.bytes = static_cast<uint8_t>(bytes);
      token.columns = static_cast<uint8_t>(letter ? 1 : 2);
      return token;
    }
  }
  PanelGlyphToken token;
  token.bytes = utf8Length(text, available);
  return token;
}

uint8_t panelTextColumns(const char* text, size_t length) {
  uint8_t columns = 0;
  for (size_t at = 0; at < length;) {
    const auto token = panelGlyphToken(text + at, length - at);
    if (!token.bytes) break;
    columns = static_cast<uint8_t>(columns + token.columns);
    at += token.bytes;
  }
  return columns;
}

}  // namespace decaflash::mainframe
