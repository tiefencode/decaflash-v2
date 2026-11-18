// Host tool for the text model (add-text-model): runs the kernel on a model
// image for one condition and prints the greedy output classes, one per line,
// up to the end token or the piece limit. Output ids are input ids, so each
// class is fed back as the next input token.
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/text_model_generate.cpp \
//           -o text_model_generate
// Usage:  text_model_generate <model.bin> <max_pieces> <condition id>...
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "slm/slm_kernel.h"
#include "slm/text_model_vocab.h"

namespace {
void* hostAlloc(size_t bytes) { return std::calloc(1, bytes); }
}  // namespace

int main(int argc, char** argv) {
  if (argc < 4) {
    std::fprintf(stderr, "usage: %s <model.bin> <max_pieces> <condition id>...\n", argv[0]);
    return 2;
  }
  std::vector<uint8_t> image;
  if (FILE* file = std::fopen(argv[1], "rb")) {
    uint8_t buffer[65536];
    size_t count;
    while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
      image.insert(image.end(), buffer, buffer + count);
    }
    std::fclose(file);
  }
  static slm::Kernel kernel;
  const slm::LoadResult result = kernel.load(image.data(), image.size());
  if (result != slm::LoadResult::Ok) {
    std::fprintf(stderr, "%s: %s\n", argv[1], slm::loadResultName(result));
    return 1;
  }
  if (!kernel.allocate(hostAlloc, hostAlloc, hostAlloc)) {
    std::fprintf(stderr, "allocation failed\n");
    return 1;
  }
  if (kernel.outputVocab() != decaflash::mainframe::text_model::kOutputTokens ||
      kernel.inputVocab() != decaflash::mainframe::text_model::kInputTokens) {
    std::fprintf(stderr, "image vocabulary %d/%d does not match the text model's %d/%d\n",
                 kernel.inputVocab(), kernel.outputVocab(),
                 decaflash::mainframe::text_model::kInputTokens,
                 decaflash::mainframe::text_model::kOutputTokens);
    return 1;
  }
  const int maxPieces = std::atoi(argv[2]);
  int next = -1;
  for (int i = 3; i < argc; ++i) {
    next = kernel.step(std::atoi(argv[i]));
    if (next < 0) {
      std::fprintf(stderr, "condition token %s refused\n", argv[i]);
      return 1;
    }
  }
  for (int piece = 0; piece < maxPieces && next > 0; ++piece) {
    std::printf("%d\n", next);
    next = kernel.step(next);
  }
  return 0;
}
