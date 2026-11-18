#include "message_panel.h"
#include "panel_glyphs.h"
#include "test_text_bank.h"

#include <cassert>
#include <cstring>
#include <cstdio>

using namespace decaflash::mainframe;

using test::xorshift;

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

  // Reaction texts come from the text bank (tests/test_text_bank.h).
  TextBank& bank = test::textBank();
  uint32_t randomState = 5;
  const Mood calm;
  panel.setTextBank(&bank);

  MotionEvent event;
  event.kind = MotionKind::MultiTap;
  event.count = 3;
  assert(panel.showMotion(19000, event, calm));
  panel.wrappedLine(19000, 1, line, sizeof(line));
  assert(std::strcmp(line, "NICHT SO") == 0);
  assert(!panel.showMotion(21000, event, calm)); // cooldown
  event.kind = MotionKind::Impact;
  assert(panel.showMotion(21000, event, calm));
  panel.wrappedLine(21000, 1, line, sizeof(line));
  assert(std::strcmp(line, "AU.") == 0);

  // Without a text bank nothing is shown and no cooldown starts.
  MessagePanel noBank;
  event.kind = MotionKind::Tap;
  assert(!noBank.showMotion(0, event, calm));
  noBank.setTextBank(&bank);
  assert(noBank.showMotion(1, event, calm));

  MessagePanel variants;
  variants.setTextBank(&bank);
  assert(variants.showMotion(0, event, calm));
  variants.wrappedLine(0, 1, line, sizeof(line));
  assert(std::strcmp(line, "KLOPF") == 0);
  assert(variants.leadingGlyph() == PanelGlyph::Speech);
  // The emoji is element 0, large in row 0; the text starts in row 1.
  assert(variants.glyphAtRow(0, 0) == PanelGlyph::Speech);
  assert(variants.glyphAtRow(0, 1) == PanelGlyph::None);
  variants.wrappedLine(0, 0, line, sizeof(line));
  assert(line[0] == '\0');

  MessagePanel doubleTap;
  doubleTap.setTextBank(&bank);
  event.kind = MotionKind::MultiTap;
  event.count = 2;
  assert(doubleTap.showMotion(0, event, calm));
  doubleTap.wrappedLine(0, 1, line, sizeof(line));
  assert(std::strcmp(line, "WER IST") == 0);
  assert(doubleTap.leadingGlyph() == PanelGlyph::Smile);

  // Every motion kind and presence event with text gets its event's line.
  MessagePanel allTexts;
  allTexts.setTextBank(&bank);
  uint32_t shownAt = 0;
  const MotionKind kinds[] = {MotionKind::Move, MotionKind::Tap, MotionKind::MultiTap,
                              MotionKind::Impact, MotionKind::Rotate, MotionKind::Reposition,
                              MotionKind::Shake};
  for (const MotionKind kind : kinds) {
    MotionEvent choice;
    choice.kind = kind;
    choice.count = 3;
    assert(allTexts.showMotion(shownAt, choice, calm));
    assert(allTexts.leadingGlyph() != PanelGlyph::None);
    shownAt += 9000;
  }
  const PresenceEventKind presence[] = {PresenceEventKind::ArrivedInFront,
                                        PresenceEventKind::WalkedPast, PresenceEventKind::Left,
                                        PresenceEventKind::StandsFar};
  const PanelGlyph presenceGlyphs[] = {PanelGlyph::Coffin, PanelGlyph::Warning, PanelGlyph::Cry,
                                       PanelGlyph::Shy};
  for (uint8_t i = 0; i < 4; ++i) {
    PresenceEvent choice;
    choice.kind = presence[i];
    assert(allTexts.showPresence(shownAt, choice, calm));
    assert(allTexts.leadingGlyph() == presenceGlyphs[i]);
    shownAt += 9000;
  }
  PresenceEvent moving;
  moving.kind = PresenceEventKind::Moves;
  assert(!allTexts.showPresence(shownAt, moving, calm)); // movement shows no text

  // The mood picks the lead group: Tap lines for annoyance high and energy low.
  {
    static const char* const lines[] = {u8"GÄHN 🫩", u8"HAU AB 💢"};
    static uint16_t starts[kTextModelEvents * TextBank::kLeads + 1];
    for (uint16_t g = 0; g <= kTextModelEvents * TextBank::kLeads; ++g) {
      starts[g] = g <= 10 ? 0 : (g <= 13 ? 1 : 2);  // Tap: group 10 ENERGY_LOW, 13 ANNOYANCE_HIGH
    }
    TextBank moodBank(TextBank::Table{starts, lines, 2, kTextModelEvents}, xorshift,
                      &randomState);
    MessagePanel moody;
    moody.setTextBank(&moodBank);
    Mood annoyed;
    annoyed.energy = 50;
    annoyed.annoyance = 95;
    annoyed.attention = 50;
    annoyed.loneliness = 50;
    annoyed.depression = 50;
    MotionEvent tap;
    tap.kind = MotionKind::Tap;
    assert(moody.showMotion(0, tap, annoyed));
    moody.wrappedLine(0, 1, line, sizeof(line));
    assert(std::strcmp(line, "HAU AB") == 0);
    Mood tired = annoyed;
    tired.annoyance = 50;
    tired.energy = 3;
    assert(moody.showMotion(9000, tap, tired));
    moody.wrappedLine(9000, 1, line, sizeof(line));
    assert(std::strcmp(line, u8"GÄHN") == 0);
  }

  // Presence texts are fully equal to motion texts.
  MessagePanel equal;
  equal.setTextBank(&bank);
  PresenceEvent arrived;
  arrived.kind = PresenceEventKind::ArrivedInFront;
  assert(equal.showPresence(1000, arrived, calm));
  equal.wrappedLine(1000, 1, line, sizeof(line));
  assert(std::strcmp(line, "BIST DU") == 0);
  assert(!equal.showPresence(5000, arrived, calm)); // same kind within the cooldown
  MotionEvent tap;
  tap.kind = MotionKind::Tap;
  assert(equal.showMotion(5000, tap, calm)); // a gesture replaces a presence text
  PresenceEvent passing;
  passing.kind = PresenceEventKind::WalkedPast;
  assert(equal.showPresence(6000, passing, calm)); // and a presence text a gesture text
  equal.wrappedLine(6000, 1, line, sizeof(line));
  assert(std::strcmp(line, "HEY,") == 0);
  assert(equal.showPresence(10000, arrived, calm)); // cooldown over
  PresenceEvent none;
  assert(!equal.showPresence(30000, none, calm));

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

  // Model texts: the request keeps the old message until the emoji arrives,
  // then the text grows word by word; display time starts at completion.
  {
    MessagePanel model;
    model.setTextBank(&bank);
    PresenceEvent front;
    front.kind = PresenceEventKind::ArrivedInFront;
    front.sequence = 1;
    assert(model.showPresence(0, front, calm));
    MotionEvent impact;
    impact.kind = MotionKind::Impact;
    assert(model.requestMotion(1000, impact, calm));
    assert(model.requestOpen() && model.busy(1000));
    model.wrappedLine(1500, 1, line, sizeof(line));
    assert(std::strcmp(line, "BIST DU") == 0); // old message until the emoji
    assert(!model.requestTimedOut(2999));
    assert(model.startModelText(1500, PanelGlyph::Angry));
    assert(!model.requestOpen() && model.growing());
    assert(model.glyphAtRow(1500, 0) == PanelGlyph::Angry);
    model.wrappedLine(1500, 1, line, sizeof(line));
    assert(line[0] == '\0');
    assert(model.appendModelWord("AU."));
    model.wrappedLine(1700, 1, line, sizeof(line));
    assert(std::strcmp(line, "AU.") == 0);
    assert(model.appendModelWord("NICHT"));
    model.wrappedLine(1900, 2, line, sizeof(line));
    assert(std::strcmp(line, "NICHT") == 0);
    assert(model.visible(20000)); // no display time while it grows
    assert(model.completeModelText(2500));
    assert(!model.growing());
    assert(model.visible(6499)); // three elements: the minimum time from completion
    assert(!model.visible(6500));
    assert(!model.appendModelWord("SPAET")); // nothing grows any more

    // Cooldown from acceptance: a second Impact within it is ignored and
    // leaves a growing text unchanged.
    MotionEvent tap;
    tap.kind = MotionKind::Tap;
    assert(model.requestMotion(10000, tap, calm));
    assert(model.startModelText(10200, PanelGlyph::Smile));
    assert(model.appendModelWord("HUHU"));
    assert(!model.requestMotion(10300, tap, calm));
    assert(model.growing());

    // New event while a text grows: the request is cancelled, the old words
    // stay until the new emoji, and the cancelled text is not completed.
    MotionEvent shake;
    shake.kind = MotionKind::Shake;
    assert(model.requestMotion(10400, shake, calm));
    assert(!model.growing() && model.requestOpen());
    model.wrappedLine(10500, 1, line, sizeof(line));
    assert(std::strcmp(line, "HUHU") == 0);
    assert(!model.appendModelWord("DU"));
    assert(model.startModelText(10600, PanelGlyph::Dizzy));
    assert(model.glyphAtRow(10600, 0) == PanelGlyph::Dizzy);

    // Emoji too late: the request times out and the fixed text is shown;
    // the cooldown still runs from the acceptance.
    MessagePanel late;
    late.setTextBank(&bank);
    tap.sequence = 0;
    assert(late.requestMotion(0, tap, calm));
    assert(!late.visible(0) && late.busy(0)); // a due poem waits
    assert(!late.requestTimedOut(1999));
    assert(late.requestTimedOut(2000));
    assert(late.fallBack(2000));
    assert(!late.requestOpen());
    late.wrappedLine(2000, 1, line, sizeof(line));
    assert(std::strcmp(line, "KLOPF") == 0);
    assert(!late.startModelText(2100, PanelGlyph::Smile)); // too late, ignored
    assert(late.glyphAtRow(2100, 0) == PanelGlyph::Speech);
    assert(!late.requestMotion(8999, tap, calm));
    tap.sequence = 1;
    assert(late.requestMotion(9000, tap, calm));
    assert(!late.startModelText(9100, PanelGlyph::Aumlaut)); // not an emoji

    // A model text that grows past four rows moves up so the newest word is
    // visible, and keeps its last four rows after completion.
    MessagePanel tall;
    tall.setTextBank(&bank);
    assert(tall.requestMotion(0, tap, calm));
    assert(tall.startModelText(100, PanelGlyph::Smile));
    assert(tall.appendModelWord("EINS"));
    assert(tall.appendModelWord("ZWEI"));
    assert(tall.appendModelWord("DREI"));
    assert(tall.glyphAtRow(200, 0) == PanelGlyph::Smile);
    assert(!tall.moreTextBelow(200));
    assert(tall.appendModelWord("VIER"));
    assert(tall.glyphAtRow(300, 0) == PanelGlyph::None);
    tall.wrappedLine(300, 0, line, sizeof(line));
    assert(std::strcmp(line, "EINS") == 0);
    tall.wrappedLine(300, 3, line, sizeof(line));
    assert(std::strcmp(line, "VIER") == 0);
    assert(tall.completeModelText(400));
    tall.wrappedLine(5000, 3, line, sizeof(line));
    assert(std::strcmp(line, "VIER") == 0);
    assert(!tall.moreTextBelow(5000));
    assert(tall.visible(7399)); // five elements plus the scrolling extra time
    assert(!tall.visible(7400));

    // A fixed text replaces an open request and a growing text at once.
    assert(tall.requestPresence(20000, front, calm));
    assert(tall.show(20100, "HELLO"));
    assert(!tall.requestOpen() && !tall.fallBack(20200));
  }

  puts("PASS: panel lifetime, motion and poetry sources, leading emoji, model texts, UTF-8 wrapping, and vertical scroll");
}
