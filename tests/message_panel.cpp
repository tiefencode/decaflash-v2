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
  assert(panel.visible(5099));
  assert(!panel.visible(5100));
  assert(panel.show(5100, "NEXT MESSAGE"));
  char line[64] = {};
  panel.wrappedLine(5100, 0, line, sizeof(line));
  assert(std::strcmp(line, "NEXT") == 0);

  MotionEvent event;
  event.kind = MotionKind::MultiTap;
  event.count = 3;
  event.sequence = 2;
  assert(panel.showMotion(19000, event));
  panel.wrappedLine(19000, 0, line, sizeof(line));
  assert(std::strcmp(line, "NICHT SO") == 0);
  assert(!panel.showMotion(21000, event));
  event.kind = MotionKind::Impact;
  event.sequence = 0;
  assert(panel.showMotion(21000, event));
  panel.wrappedLine(21000, 0, line, sizeof(line));
  assert(std::strcmp(line, "AU.") == 0);

  MessagePanel variants;
  event.kind = MotionKind::Tap;
  event.sequence = 0;
  assert(variants.showMotion(0, event));
  variants.wrappedLine(0, 0, line, sizeof(line));
  assert(std::strcmp(line, "HUHU") == 0);
  assert(variants.trailingGlyph() == PanelGlyph::Smile);
  assert(variants.glyphAtRow(0, 1) == PanelGlyph::Smile);
  event.sequence = 3;
  assert(variants.showMotion(9000, event));
  variants.wrappedLine(9000, 0, line, sizeof(line));
  assert(std::strcmp(line, "KLOPF") == 0);

  assert(panel.show(28000, "EINS ZWEI DREI VIER FUNF SECHS SIEBEN ACHT NEUN ZEHN"));
  panel.wrappedLine(28000, 0, line, sizeof(line));
  assert(std::strcmp(line, "EINS") == 0);
  panel.wrappedLine(28000, 1, line, sizeof(line));
  assert(std::strcmp(line, "ZWEI") == 0);
  panel.wrappedLine(30000, 0, line, sizeof(line));
  assert(std::strcmp(line, "ZWEI") == 0);
  assert(panel.moreTextBelow(30000));
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
  panel.wrappedLine(40000, 0, line, sizeof(line));
  assert(std::strcmp(line, u8"HÜHÜ 🙂") == 0);
  assert(panel.trailingGlyph() == PanelGlyph::Music);

  puts("PASS: panel lifetime, motion source, UTF-8 wrapping, and vertical scroll");
}
