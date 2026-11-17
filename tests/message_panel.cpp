#include "message_panel.h"
#include "panel_glyphs.h"

#include <cassert>
#include <cstring>
#include <cstdio>

using namespace decaflash::mainframe;

int main() {
  MessagePanel panel;
  assert(!panel.visible(0));
  assert(panel.show(100, "HELLO"));
  assert(panel.visible(4099)); // Minimum display time.
  assert(!panel.visible(4100));
  assert(panel.show(1600, "NEXT MESSAGE"));
  char line[64] = {};
  panel.wrappedLine(1600, 0, line, sizeof(line));
  assert(std::strcmp(line, "NEXT") == 0);

  MessagePanel timed;
  assert(timed.show(0, "EINS ZWEI DREI VIER FUNF"));
  // One second for each of five text lines plus the scrolling extra time.
  assert(timed.visible(6999));
  assert(!timed.visible(7000));

  MessagePanel fourLines;
  assert(fourLines.show(0, "EINS ZWEI DREI VIER"));
  assert(fourLines.visible(3999)); // No scrolling, no extra time.
  assert(!fourLines.visible(4000));

  MessagePanel twoLines;
  assert(twoLines.show(0, "NEXT MESSAGE"));
  assert(twoLines.visible(3999)); // Minimum display time.
  assert(!twoLines.visible(4000));

  MotionEvent event;
  event.kind = MotionKind::MultiTap;
  event.count = 3;
  event.sequence = 2;
  assert(panel.showMotion(19000, event));
  panel.wrappedLine(19000, 1, line, sizeof(line));
  assert(std::strcmp(line, "NICHT SO") == 0);
  assert(!panel.showMotion(21000, event));
  event.kind = MotionKind::Impact;
  event.sequence = 0;
  assert(panel.showMotion(21000, event));
  panel.wrappedLine(21000, 1, line, sizeof(line));
  assert(std::strcmp(line, "AU.") == 0);

  MessagePanel variants;
  event.kind = MotionKind::Tap;
  event.sequence = 0;
  assert(variants.showMotion(0, event));
  variants.wrappedLine(0, 1, line, sizeof(line));
  assert(std::strcmp(line, "KLOPF") == 0);
  assert(variants.leadingGlyph() == PanelGlyph::Speech);
  // The emoji is element 0, large in row 0; the text starts in row 1.
  assert(variants.glyphAtRow(0, 0) == PanelGlyph::Speech);
  assert(variants.glyphAtRow(0, 1) == PanelGlyph::None);
  variants.wrappedLine(0, 0, line, sizeof(line));
  assert(line[0] == '\0');
  event.sequence = 2; // 2 mod 2 = 0 repeats the last variant, so take the next
  assert(variants.showMotion(9000, event));
  variants.wrappedLine(9000, 1, line, sizeof(line));
  assert(std::strcmp(line, "WER") == 0);
  assert(variants.leadingGlyph() == PanelGlyph::Shy);

  MessagePanel doubleTap;
  event.kind = MotionKind::MultiTap;
  event.count = 2;
  event.sequence = 1;
  assert(doubleTap.showMotion(0, event));
  doubleTap.wrappedLine(0, 1, line, sizeof(line));
  assert(std::strcmp(line, "WER IST") == 0);
  assert(doubleTap.leadingGlyph() == PanelGlyph::Smile);

  // Every motion and presence text ends in a known curated emoji; emoji may
  // repeat across kinds since add-presence-mood.
  MessagePanel allTexts;
  uint32_t shownAt = 0;
  const auto verifyMotion = [&](MotionKind kind, uint8_t count, uint8_t variants) {
    for (uint8_t sequence = 0; sequence < variants; ++sequence) {
      MotionEvent choice;
      choice.kind = kind;
      choice.count = count;
      choice.sequence = sequence;
      assert(allTexts.showMotion(shownAt, choice));
      assert(allTexts.leadingGlyph() != PanelGlyph::None);
      shownAt += 9000;
    }
  };
  verifyMotion(MotionKind::Move, 0, 2);
  verifyMotion(MotionKind::Tap, 1, 2);
  verifyMotion(MotionKind::MultiTap, 2, 2);
  verifyMotion(MotionKind::MultiTap, 3, 2);
  verifyMotion(MotionKind::Impact, 1, 3);
  verifyMotion(MotionKind::Rotate, 0, 2);
  verifyMotion(MotionKind::Reposition, 0, 3);
  verifyMotion(MotionKind::Shake, 0, 3);
  const auto verifyPresence = [&](PresenceEventKind kind, uint8_t variants,
                                  const PanelGlyph* glyphs) {
    for (uint8_t sequence = 0; sequence < variants; ++sequence) {
      PresenceEvent choice;
      choice.kind = kind;
      choice.sequence = sequence;
      assert(allTexts.showPresence(shownAt, choice));
      assert(allTexts.leadingGlyph() == glyphs[sequence]);
      shownAt += 9000;
    }
  };
  const PanelGlyph arrivedGlyphs[] = {PanelGlyph::PointTogether, PanelGlyph::Coffin,
                                      PanelGlyph::Wave, PanelGlyph::Smile};
  const PanelGlyph walkedPastGlyphs[] = {PanelGlyph::Warning, PanelGlyph::Plead,
                                         PanelGlyph::Relaxed};
  const PanelGlyph leftGlyphs[] = {PanelGlyph::Cry, PanelGlyph::Pleading, PanelGlyph::Sad};
  verifyPresence(PresenceEventKind::ArrivedInFront, 4, arrivedGlyphs);
  verifyPresence(PresenceEventKind::WalkedPast, 3, walkedPastGlyphs);
  verifyPresence(PresenceEventKind::Left, 3, leftGlyphs);
  const PanelGlyph standsFarGlyphs[] = {PanelGlyph::Shy, PanelGlyph::Eye, PanelGlyph::Knife};
  verifyPresence(PresenceEventKind::StandsFar, 3, standsFarGlyphs);
  PresenceEvent moving;
  moving.kind = PresenceEventKind::Moves;
  assert(!allTexts.showPresence(shownAt, moving)); // movement shows no text

  // Presence texts are fully equal to motion texts.
  MessagePanel equal;
  PresenceEvent arrived;
  arrived.kind = PresenceEventKind::ArrivedInFront;
  arrived.sequence = 1;
  assert(equal.showPresence(1000, arrived));
  equal.wrappedLine(1000, 1, line, sizeof(line));
  assert(std::strcmp(line, "BIST DU") == 0);
  arrived.sequence = 2;
  assert(!equal.showPresence(5000, arrived)); // same kind within the cooldown
  MotionEvent tap;
  tap.kind = MotionKind::Tap;
  assert(equal.showMotion(5000, tap)); // a gesture replaces a presence text
  PresenceEvent passing;
  passing.kind = PresenceEventKind::WalkedPast;
  assert(equal.showPresence(6000, passing)); // and a presence text a gesture text
  equal.wrappedLine(6000, 1, line, sizeof(line));
  assert(std::strcmp(line, "HEY,") == 0);
  assert(equal.showPresence(10000, arrived)); // cooldown over
  PresenceEvent none;
  assert(!equal.showPresence(30000, none));

  MessagePanel poems;
  const PanelGlyph poemGlyphs[] = {
    PanelGlyph::Grave, PanelGlyph::Coffin, PanelGlyph::Urn,
    PanelGlyph::Cigarette, PanelGlyph::Moon, PanelGlyph::Grave,
    PanelGlyph::Moon, PanelGlyph::Cigarette, PanelGlyph::Glass,
    PanelGlyph::BlackHeart, PanelGlyph::Melt,
  };
  for (uint8_t sequence = 0; sequence < sizeof(poemGlyphs) / sizeof(poemGlyphs[0]); ++sequence) {
    const uint32_t at = 50000 + sequence * 6000;
    assert(poems.showDepressionPoem(at, sequence));
    assert(poems.leadingGlyph() == poemGlyphs[sequence]);
    assert(poems.glyphAtRow(at, 0) == poemGlyphs[sequence]);
    poems.wrappedLine(at, 1, line, sizeof(line));
    assert(line[0] != '\0');
  }

  // A message with an emoji and four text lines has five elements: it stands
  // still with the emoji on top, then scrolls until its last four lines show.
  MessagePanel leading;
  assert(leading.show(0, u8"EINS ZWEI DREI VIER 🙂"));
  assert(leading.glyphAtRow(0, 0) == PanelGlyph::Smile);
  leading.wrappedLine(0, 1, line, sizeof(line));
  assert(std::strcmp(line, "EINS") == 0);
  leading.wrappedLine(0, 3, line, sizeof(line));
  assert(std::strcmp(line, "DREI") == 0);
  assert(leading.moreTextBelow(0));
  assert(leading.visible(6999)); // Five elements plus the scrolling extra time.
  assert(!leading.visible(7000));
  assert(leading.glyphAtRow(4000, 0) == PanelGlyph::None);
  leading.wrappedLine(4000, 0, line, sizeof(line));
  assert(std::strcmp(line, "EINS") == 0);
  leading.wrappedLine(4000, 3, line, sizeof(line));
  assert(std::strcmp(line, "VIER") == 0);
  assert(!leading.moreTextBelow(4000));
  // A German letter at the end stays in the text.
  assert(leading.show(10000, u8"SCHÖN SÜß"));
  assert(leading.leadingGlyph() == PanelGlyph::None);
  leading.wrappedLine(10000, 0, line, sizeof(line));
  assert(std::strcmp(line, u8"SCHÖN") == 0);

  assert(panel.show(28000, "EINS ZWEI DREI VIER FUNF SECHS SIEBEN ACHT NEUN ZEHN"));
  panel.wrappedLine(28000, 0, line, sizeof(line));
  assert(std::strcmp(line, "EINS") == 0);
  panel.wrappedLine(28000, 1, line, sizeof(line));
  assert(std::strcmp(line, "ZWEI") == 0);
  panel.wrappedLine(31999, 0, line, sizeof(line));
  assert(std::strcmp(line, "EINS") == 0); // Stands still for four seconds.
  panel.wrappedLine(32000, 0, line, sizeof(line));
  assert(std::strcmp(line, "ZWEI") == 0);
  panel.wrappedLine(32800, 0, line, sizeof(line));
  assert(std::strcmp(line, "DREI") == 0);
  assert(panel.moreTextBelow(32800));
  panel.wrappedLine(100000, 0, line, sizeof(line));
  assert(std::strcmp(line, "SIEBEN") == 0);
  panel.wrappedLine(100000, 3, line, sizeof(line));
  assert(std::strcmp(line, "ZEHN") == 0);
  assert(!panel.moreTextBelow(100000));

  assert(panelTextColumns(u8"ÄÖÜß 🙂", std::strlen(u8"ÄÖÜß 🙂")) == 7);
  const auto dizzy = panelGlyphToken(u8"😵‍💫", std::strlen(u8"😵‍💫"));
  assert(dizzy.glyph == PanelGlyph::DizzySpiral && dizzy.columns == 2);
  const auto pointing = panelGlyphToken(u8"👉👈", std::strlen(u8"👉👈"));
  assert(pointing.glyph == PanelGlyph::PointTogether && pointing.columns == 2);
  const auto mistyEyes = panelGlyphToken(u8"🥹", std::strlen(u8"🥹"));
  const auto pleadingEyes = panelGlyphToken(u8"🥺", std::strlen(u8"🥺"));
  assert(mistyEyes.glyph == PanelGlyph::Plead);
  assert(pleadingEyes.glyph == PanelGlyph::Pleading);
  const char* const supported[] = {
    u8"🙂", u8"👋", u8"👁", u8"😵", u8"🤢", u8"❗", u8"💢", u8"💬", u8"🎵", u8"↻",
    u8"🥰", u8"😂", u8"🫠", u8"😍", u8"😘", u8"🥲", u8"🤗", u8"🫢", u8"🙂‍↔️", u8"🥹",
    u8"😢", u8"😭", u8"😡", u8"💀", u8"💩", u8"🙈", u8"👌", u8"👉👈", u8"🙌", u8"😳",
    u8"🙄", u8"🫩", u8"😌", u8"😔", u8"😵‍💫", u8"🥺", u8"😤", u8"🤬", u8"☠️", u8"🖤",
    u8"🔪", u8"🥃", u8"🌚", u8"⚰️", u8"🚬", u8"🪦", u8"⚱️",
  };
  for (const char* emoji : supported) {
    assert(panelGlyphToken(emoji, std::strlen(emoji)).glyph != PanelGlyph::None);
  }
  assert(panel.show(40000, u8"HÜHÜ 🙂 MUSIK 🎵"));
  panel.wrappedLine(40000, 1, line, sizeof(line));
  assert(std::strcmp(line, u8"HÜHÜ 🙂") == 0); // the inline emoji stays in the text
  assert(panel.leadingGlyph() == PanelGlyph::Music);
  assert(panel.glyphAtRow(40000, 0) == PanelGlyph::Music);

  puts("PASS: panel lifetime, motion and poetry sources, leading emoji, UTF-8 wrapping, and vertical scroll");
}
