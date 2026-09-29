#pragma once

#include <cstddef>
#include <cstdint>

namespace decaflash::mainframe {

// Deliberately small, authored subset of Unicode for the message panel. It is
// a visual vocabulary, not a general-purpose emoji font.
enum class PanelGlyph : uint8_t {
  None,
  Aumlaut, Oumlaut, Uumlaut, SharpS,
  Smile, Wave, Eye, Dizzy, Nauseous, Warning, AngerBurst, Speech, Music, Loop,
  Love, Laugh, Melt, HeartEyes, Kiss, TearSmile, Hug, Shy, HeadTurn, Plead,
  Pleading,
  Cry, Scream, Angry, Skull, Poop, SeeNoEvil, OkHand, PointTogether, Praise,
  Blush, EyeRoll, Exhausted, Relaxed, Sad, DizzySpiral, Huffy, Swearing,
  SkullCrossbones, BlackHeart, Knife, Glass, Moon, Coffin, Cigarette, Grave, Urn,
};

struct PanelGlyphToken {
  PanelGlyph glyph = PanelGlyph::None;
  uint8_t bytes = 0;
  uint8_t columns = 1;
};

PanelGlyphToken panelGlyphToken(const char* text, size_t available);
uint8_t panelTextColumns(const char* text, size_t length);

}  // namespace decaflash::mainframe
