// Try the text bank by hand (add-text-bank, task 2.3), with the firmware's own
// selection code (text_bank.cpp) and table (text_bank_data.h).
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/text_bank_try.cpp \
//           apps/mainframe/src/text_bank.cpp -o text_bank_try
// Usage:  text_bank_try [seed]
// Per request: an event number (1-12) and five mood values 0-100 (energy,
// annoyance, attention, loneliness, depression). It prints the lead value the
// bank derives, the size of that group and the next three texts of its round.
// Rounds continue while the tool runs. q ends it.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
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

constexpr const char* kEvents[12] = {
  "Getragen", "Klopfer", "Doppelklopfer", "3+ Klopfer", "Stoß", "Gedreht",
  "Hingestellt/umgedreht", "Geschüttelt", "Kommt davor", "Läuft vorbei", "Geht weg",
  "Steht weiter weg"};
constexpr const char* kValues[5] = {"Energie", "Genervt", "Aufmerksam", "Einsam",
                                    "Depression"};

// "<words> <emoji>" as stored -> "<emoji> <words>" as in the corpus.
std::string emojiFirst(const char* text) {
  const std::string line = text;
  const size_t space = line.rfind(' ');
  if (space == std::string::npos) return line;
  return line.substr(space + 1) + " " + line.substr(0, space);
}

bool ask(const char* prompt, std::string& answer) {
  std::cout << prompt << std::flush;
  if (!std::getline(std::cin, answer)) return false;
  return answer != "q";
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t state = argc > 1 ? static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 1;
  if (!state) state = 1;
  TextBank bank(xorshift, &state);

  std::cout << "Textbank. Ereignisse:";
  for (int i = 0; i < 12; ++i) std::cout << (i ? ", " : " ") << i + 1 << " " << kEvents[i];
  std::cout << "\nStimmung als fünf Zahlen 0-100: Energie Genervt Aufmerksam Einsam "
               "Depression, z. B. 30 90 50 40 60. Beenden mit q.\n";
  std::string answer;
  while (true) {
    if (!ask("\nEreignis (1-12): ", answer)) return 0;
    const int event = std::atoi(answer.c_str());
    if (event < 1 || event > 12) {
      std::cout << "  Ungültig, nochmal.\n";
      continue;
    }
    int values[5];
    bool valid = false;
    while (!valid) {
      if (!ask("Stimmung (5 Zahlen 0-100): ", answer)) return 0;
      std::istringstream in(answer);
      valid = true;
      for (int& v : values) {
        if (!(in >> v) || v < 0 || v > 100) valid = false;
      }
      if (!valid) std::cout << "  Ungültig, nochmal.\n";
    }
    Mood mood;
    mood.energy = static_cast<uint8_t>(values[0]);
    mood.annoyance = static_cast<uint8_t>(values[1]);
    mood.attention = static_cast<uint8_t>(values[2]);
    mood.loneliness = static_cast<uint8_t>(values[3]);
    mood.depression = static_cast<uint8_t>(values[4]);
    const uint8_t lead = bank.leadFor(mood);
    const uint8_t index = static_cast<uint8_t>(event - 1);
    std::cout << "  " << kEvents[index] << " | Leitwert " << kValues[lead / 2]
              << (lead % 2 ? " hoch" : " niedrig") << " (" << TextBank::leadName(lead)
              << ") | " << bank.groupSize(index, lead) << " Zeilen in der Gruppe\n";
    if (!bank.groupSize(index, lead)) {
      std::cout << "  Keine Zeile für diesen Leitwert, Auswahl aus allen "
                << bank.eventSize(index) << " Zeilen des Ereignisses.\n";
    }
    for (int i = 0; i < 3; ++i) {
      const char* text = bank.pick(index, lead);
      std::cout << "  " << (text ? emojiFirst(text) : std::string("(keine Zeile)")) << "\n";
    }
  }
}
