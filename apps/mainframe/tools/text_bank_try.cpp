// Try the text bank by hand (add-text-bank, task 2.3), with the firmware's own
// selection code (text_bank.cpp) and table (text_bank_data.h).
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/text_bank_try.cpp \
//           apps/mainframe/src/text_bank.cpp -o text_bank_try
// Usage:  text_bank_try                      ask in a loop: pick the event in a list
//                                             with the arrow keys and Enter, type the
//                                             five values; q ends it
//         text_bank_try <event> <five values>  answer once, e.g. 2 30 90 50 40 60
// An event number (1-12) and five mood values 0-100 (energy, annoyance,
// attention, loneliness, depression). It prints the lead value the bank
// derives, the size of that group and the next three texts of its round.
// Rounds continue while the tool runs.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include <termios.h>
#include <unistd.h>

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

// Event list with a cursor: arrow keys or j/k move, Enter picks, q ends.
// Returns the event number 1-12, or 0 to end.
int pickEvent(int current) {
  termios saved;
  tcgetattr(STDIN_FILENO, &saved);
  termios raw = saved;
  raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);
  std::cout << "\nEreignis wählen (Pfeiltasten, Enter; q beendet):\n";
  int chosen = -1;
  bool first = true;
  while (chosen < 0) {
    if (!first) std::cout << "\x1b[12A";  // back to the top of the list
    first = false;
    for (int i = 0; i < 12; ++i) {
      std::cout << "\r\x1b[2K" << (i == current ? "\x1b[7m > " : "   ") << i + 1 << " "
                << kEvents[i] << (i == current ? " \x1b[0m" : "") << "\n";
    }
    std::cout << std::flush;
    char key = 0;
    if (read(STDIN_FILENO, &key, 1) != 1) {
      chosen = 0;
      break;
    }
    if (key == '\x1b') {
      char seq[2] = {0, 0};
      if (read(STDIN_FILENO, &seq[0], 1) == 1 && read(STDIN_FILENO, &seq[1], 1) == 1 &&
          seq[0] == '[') {
        if (seq[1] == 'A') current = (current + 11) % 12;
        if (seq[1] == 'B') current = (current + 1) % 12;
      }
    } else if (key == 'k') {
      current = (current + 11) % 12;
    } else if (key == 'j') {
      current = (current + 1) % 12;
    } else if (key == '\n' || key == '\r') {
      chosen = current + 1;
    } else if (key == 'q') {
      chosen = 0;
    }
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &saved);
  return chosen;
}

bool ask(const char* prompt, std::string& answer) {
  std::cout << prompt << std::flush;
  if (!std::getline(std::cin, answer)) return false;
  return answer != "q";
}

void answer(TextBank& bank, int event, const int values[5]) {
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

bool valid(int event, const int values[5]) {
  if (event < 1 || event > 12) return false;
  for (int i = 0; i < 5; ++i) {
    if (values[i] < 0 || values[i] > 100) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  uint32_t state = std::random_device{}() | 1u;  // a new order on every start
  TextBank bank(xorshift, &state);

  if (argc == 7) {  // one answer from the command line
    const int event = std::atoi(argv[1]);
    int values[5];
    for (int i = 0; i < 5; ++i) values[i] = std::atoi(argv[2 + i]);
    if (!valid(event, values)) {
      std::cerr << "Ereignis 1-12 und fünf Werte 0-100, z. B.: text_bank_try 2 30 90 50 40 60\n";
      return 2;
    }
    answer(bank, event, values);
    return 0;
  }

  std::cout << "Textbank. Stimmung als fünf Zahlen 0-100: Energie Genervt Aufmerksam "
               "Einsam Depression, z. B. 30 90 50 40 60. Beenden mit q.\n";
  std::string line;
  const bool terminal = isatty(STDIN_FILENO);  // a list with a cursor, or plain lines
  int last = 0;
  while (true) {
    int event = 0;
    if (terminal) {
      event = pickEvent(last);
      if (!event) return 0;
      last = event - 1;
      std::cout << "  " << kEvents[event - 1] << "\n";
    } else {
      if (!ask("\nEreignis (1-12): ", line)) return 0;
      event = std::atoi(line.c_str());
      if (event < 1 || event > 12) {
        std::cout << "  Ungültig, nochmal.\n";
        continue;
      }
    }
    int values[5] = {-1, -1, -1, -1, -1};
    while (!valid(event, values)) {
      if (!ask("Stimmung (5 Zahlen 0-100): ", line)) return 0;
      std::istringstream in(line);
      for (int& v : values) {
        if (!(in >> v)) v = -1;
      }
      if (!valid(event, values)) std::cout << "  Ungültig, nochmal.\n";
    }
    answer(bank, event, values);
  }
}
