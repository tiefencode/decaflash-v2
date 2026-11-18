// Form check for text model texts (add-text-model, design.md Decision 9): reads
// texts, one per line, and reports every line that fails.
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src -I apps/mainframe/tools \
//           apps/mainframe/tools/text_model_form_check.cpp -o text_model_form_check
// Usage:  text_model_form_check [--training <corpus>] <texts>
// A line is "<emoji> <words>", optionally after tags that end in a tab. Lines
// of the training corpus count as copies when their words equal a text's words.
// Exit status 1 if any line fails.
#include <cstdio>
#include <fstream>
#include <set>
#include <string>

#include "text_model_form.h"

using namespace decaflash::mainframe::text_model;

int main(int argc, char** argv) {
  std::set<std::string> training;
  const char* textsPath = nullptr;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--training" && i + 1 < argc) {
      std::ifstream corpus(argv[++i]);
      if (!corpus) {
        std::fprintf(stderr, "cannot read %s\n", argv[i]);
        return 2;
      }
      for (std::string line; std::getline(corpus, line);) {
        if (!line.empty()) training.insert(wordsOf(stripTags(line)));
      }
    } else {
      textsPath = argv[i];
    }
  }
  if (!textsPath) {
    std::fprintf(stderr, "usage: %s [--training <corpus>] <texts>\n", argv[0]);
    return 2;
  }
  std::ifstream texts(textsPath);
  if (!texts) {
    std::fprintf(stderr, "cannot read %s\n", textsPath);
    return 2;
  }
  unsigned lines = 0;
  unsigned failed = 0;
  unsigned number = 0;
  for (std::string line; std::getline(texts, line);) {
    ++number;
    if (line.empty()) continue;
    ++lines;
    const FormResult result = checkForm(line, training);
    if (result != FormResult::Ok) {
      ++failed;
      std::printf("FAIL line %u (%s): %s\n", number, formResultName(result), line.c_str());
    }
  }
  std::printf("%u texts, %u failed\n", lines, failed);
  return failed ? 1 : 0;
}
