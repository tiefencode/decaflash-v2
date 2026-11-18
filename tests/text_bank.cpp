#include "text_bank.h"
#include "text_bank_data.h"
#include "panel_glyphs.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

using namespace decaflash::mainframe;

namespace {

uint32_t xorshift(void* context) {
  auto* state = static_cast<uint32_t*>(context);
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

Mood mood(uint8_t e, uint8_t a, uint8_t t, uint8_t l, uint8_t d) {
  Mood m;
  m.energy = e;
  m.annoyance = a;
  m.attention = t;
  m.loneliness = l;
  m.depression = d;
  return m;
}

constexpr uint8_t kLow = 0, kHigh = 1;
uint8_t lead(uint8_t value, uint8_t side) { return static_cast<uint8_t>(value * 2 + side); }

}  // namespace

int main() {
  namespace data = text_bank_data;
  uint32_t state = 7;

  // The built-in bank: every event has lines, every line ends in a curated
  // emoji after a space, and no line occurs twice.
  {
    std::set<std::string> seen;
    for (uint8_t event = 0; event < data::kEvents; ++event) {
      const uint16_t begin = data::kGroupStart[event * data::kLeads];
      const uint16_t end = data::kGroupStart[(event + 1) * data::kLeads];
      assert(end > begin);
      for (uint16_t i = begin; i < end; ++i) {
        const char* text = data::kLines[i];
        const char* space = std::strrchr(text, ' ');
        assert(space && space[1]);
        const auto token = panelGlyphToken(space + 1, std::strlen(space + 1));
        assert(token.glyph != PanelGlyph::None && token.glyph != PanelGlyph::Aumlaut &&
               token.glyph != PanelGlyph::Oumlaut && token.glyph != PanelGlyph::Uumlaut &&
               token.glyph != PanelGlyph::SharpS);
        assert(token.bytes == std::strlen(space + 1));
        assert(seen.insert(text).second);
      }
    }
    assert(data::kGroupStart[data::kEvents * data::kLeads] == data::kLineCount);
    // A former fixed text is an Impact line (event 4).
    bool found = false;
    for (uint16_t i = data::kGroupStart[4 * data::kLeads];
         i < data::kGroupStart[5 * data::kLeads]; ++i) {
      if (std::strcmp(data::kLines[i], u8"NICHT SCHLAGEN! 😭") == 0) found = true;
    }
    assert(found);
  }

  TextBank bank(xorshift, &state);

  // Lead value: farthest from 50, low side below 50.
  assert(bank.leadFor(mood(50, 90, 45, 55, 60)) == lead(1, kHigh));
  assert(bank.leadFor(mood(50, 50, 50, 38, 60)) == lead(3, kLow));  // all in the middle third
  assert(bank.leadFor(mood(0, 50, 50, 50, 50)) == lead(0, kLow));
  assert(bank.leadFor(mood(50, 50, 50, 50, 100)) == lead(4, kHigh));
  // Tie: energy 10 and depression 90; both answers occur, nothing else.
  {
    bool energyLow = false, depressionHigh = false;
    for (int i = 0; i < 64; ++i) {
      const uint8_t l = bank.leadFor(mood(10, 50, 50, 50, 90));
      assert(l == lead(0, kLow) || l == lead(4, kHigh));
      energyLow |= l == lead(0, kLow);
      depressionHigh |= l == lead(4, kHigh);
    }
    assert(energyLow && depressionHigh);
  }

  // Rounds on the built-in table: every line of a group once, then a new round.
  {
    const uint8_t tap = 1, annoyanceHigh = lead(1, kHigh);
    const uint16_t size = bank.groupSize(tap, annoyanceHigh);
    assert(size >= 2);
    std::set<std::string> round;
    for (uint16_t i = 0; i < size; ++i) assert(round.insert(bank.pick(tap, annoyanceHigh)).second);
    std::set<std::string> next;
    for (uint16_t i = 0; i < size; ++i) assert(next.insert(bank.pick(tap, annoyanceHigh)).second);
    assert(round == next);
  }

  // A small table: group without lines falls back to the whole event.
  {
    // Event 0: ENERGY_LOW has 2 lines, ENERGY_HIGH 1, the rest none.
    static const char* const lines[] = {u8"A 🙂", u8"B 🙂", u8"C 💬"};
    static const uint16_t starts[11] = {0, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3};
    TextBank small(TextBank::Table{starts, lines, 3, 1}, xorshift, &state);
    assert(small.groupSize(0, 0) == 2 && small.groupSize(0, 3) == 0 && small.eventSize(0) == 3);
    std::set<std::string> all;
    for (int i = 0; i < 3; ++i) all.insert(small.pick(0, lead(1, kHigh)));  // no lines
    assert(all.size() == 3);
    assert(small.pick(1, 0) == nullptr);  // unknown event
    const std::string first = small.pick(0, 1);
    assert(first == u8"C 💬" && small.pick(0, 1) == first);  // a group of one repeats
  }

  std::printf("PASS: text bank: %u lines, lead value, rounds without repeats, fallback\n",
              data::kLineCount);
}
