// A small text bank for host tests: one line per text event in its first lead
// group, so every lead value gives that line (the other groups are empty and
// fall back to the whole event). The lines are the former fixed texts.
#pragma once

#include "text_bank.h"
#include "text_event.h"

namespace decaflash::mainframe::test {

inline uint32_t xorshift(void* context) {
  auto* state = static_cast<uint32_t*>(context);
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

inline TextBank& textBank() {
  static const char* const lines[kTextModelEvents] = {
    u8"WOHIN GEHEN WIR? 👁", u8"KLOPF 💬", u8"WER IST DA? 🙂", u8"NICHT SO WILD. 😤",
    u8"AU. SANFTER! 😡", u8"ALLES DREHT SICH. 🫩", u8"ICH SEH NIX. 🙈",
    u8"AAH, ICH KOTZE! 🤢", u8"BIST DU AUCH GRUFTI? ⚰️", u8"HEY, WARTE! ❗",
    u8"SCHON WEG? 😢", u8"IST DA WER? 🫢"};
  static uint16_t starts[kTextModelEvents * TextBank::kLeads + 1];
  static uint32_t state = 5;
  static bool ready = false;
  if (!ready) {
    for (uint8_t e = 0; e < kTextModelEvents; ++e) {
      starts[e * TextBank::kLeads] = e;
      for (uint8_t l = 1; l < TextBank::kLeads; ++l) {
        starts[e * TextBank::kLeads + l] = static_cast<uint16_t>(e + 1);
      }
    }
    starts[kTextModelEvents * TextBank::kLeads] = kTextModelEvents;
    ready = true;
  }
  static TextBank bank(TextBank::Table{starts, lines, kTextModelEvents, kTextModelEvents},
                       xorshift, &state);
  return bank;
}

}  // namespace decaflash::mainframe::test
