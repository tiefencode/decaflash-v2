// Draws text bank lines for the text model harness (text_model_harness.py),
// with the firmware's own selection code (text_bank.cpp) and table: for each
// draw it derives the lead value from the five mood values, as the firmware
// does at every event, and picks the next line of that group's round.
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/text_bank_draw.cpp \
//           apps/mainframe/src/text_bank.cpp -o text_bank_draw
// Usage:  text_bank_draw <event 0-11> <five values 0-100> <count> <seed>
// Prints one line per draw: lead name, tab, the text with the emoji first.
#include <cstdio>
#include <cstdlib>
#include <string>

#include "text_bank.h"

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

// "<words> <emoji>" as stored -> "<emoji> <words>" as in the corpus.
std::string emojiFirst(const char* text) {
  const std::string line = text;
  const size_t space = line.rfind(' ');
  if (space == std::string::npos) return line;
  return line.substr(space + 1) + " " + line.substr(0, space);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 9) {
    std::fprintf(stderr, "usage: %s <event 0-11> <v0> <v1> <v2> <v3> <v4> <count> <seed>\n",
                 argv[0]);
    return 2;
  }
  const int event = std::atoi(argv[1]);
  Mood mood{};
  mood.energy = static_cast<uint8_t>(std::atoi(argv[2]));
  mood.annoyance = static_cast<uint8_t>(std::atoi(argv[3]));
  mood.attention = static_cast<uint8_t>(std::atoi(argv[4]));
  mood.loneliness = static_cast<uint8_t>(std::atoi(argv[5]));
  mood.depression = static_cast<uint8_t>(std::atoi(argv[6]));
  const int count = std::atoi(argv[7]);
  uint32_t state = static_cast<uint32_t>(std::strtoul(argv[8], nullptr, 0));
  if (state == 0) state = 1;  // xorshift stays at 0 forever
  TextBank bank(xorshift, &state);
  for (int i = 0; i < count; ++i) {
    const uint8_t lead = bank.leadFor(mood);
    const char* text = bank.pick(static_cast<uint8_t>(event), lead);
    if (!text) return 1;
    std::printf("%s\t%s\n", TextBank::leadName(lead), emojiFirst(text).c_str());
  }
  return 0;
}
