// Host tool for the language model prototype (docs/slm-prototype.md): runs the
// kernel on a model image for a fixed prompt and prints the output classes it
// generates, one per line, greedy. The device must produce the same classes.
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/slm_generate.cpp -o slm_generate
// Usage:  slm_generate <model.bin> <out2in.txt> <bos_class> <eos_class> <max_pieces> <prompt id>...
// out2in.txt holds one input token id per output class, or "-" for identity.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "slm/slm_kernel.h"

namespace {
void* hostAlloc(size_t bytes) { return std::calloc(1, bytes); }
}  // namespace

int main(int argc, char** argv) {
  if (argc < 7) {
    std::fprintf(stderr,
                 "usage: %s <model.bin> <out2in.txt|-> <bos_class> <eos_class> <max_pieces> "
                 "<prompt id>...\n",
                 argv[0]);
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
  std::vector<int> outToIn;
  if (argv[2][0] == '-' && argv[2][1] == '\0') {
    for (int k = 0; k < kernel.outputVocab(); ++k) outToIn.push_back(k);
  } else if (FILE* file = std::fopen(argv[2], "r")) {
    int id;
    while (std::fscanf(file, "%d", &id) == 1) outToIn.push_back(id);
    std::fclose(file);
  }
  if (static_cast<int>(outToIn.size()) != kernel.outputVocab()) {
    std::fprintf(stderr, "out2in needs %d entries, got %zu\n", kernel.outputVocab(),
                 outToIn.size());
    return 1;
  }
  const int bosClass = std::atoi(argv[3]);
  const int eosClass = std::atoi(argv[4]);
  const int maxPieces = std::atoi(argv[5]);
  if (bosClass < 0 || bosClass >= kernel.outputVocab()) {
    std::fprintf(stderr, "bos class out of range\n");
    return 1;
  }
  int next = -1;
  for (int i = 6; i < argc; ++i) {
    if (kernel.step(std::atoi(argv[i])) < 0) {
      std::fprintf(stderr, "prompt token %s refused\n", argv[i]);
      return 1;
    }
  }
  next = kernel.step(outToIn[bosClass]);
  for (int piece = 0; piece < maxPieces && next >= 0 && next != eosClass; ++piece) {
    std::printf("%d\n", next);
    next = kernel.step(outToIn[next]);
  }
  return 0;
}
