// Host tool for the text model (add-text-model, task 7.1): writes texts with
// the firmware's writer (masks, sampling schedule, length limit) on a
// model image, one line per seed, for comparison with the device.
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/text_model_write.cpp \
//           -o text_model_write
// Usage:  text_model_write <model.bin> <event 0-11> <five mood levels 0-2> <seed>...
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "slm/slm_kernel.h"
#include "slm/text_writer.h"

namespace {
void* hostAlloc(size_t bytes) { return std::calloc(1, bytes); }

struct PrintSink {
  std::string text;
  bool cancelled() { return false; }
  void emoji(uint16_t token) { text = decaflash::mainframe::text_model::kTokens[token].utf8; }
  void word(const char* utf8) { text += std::string(" ") + utf8; }
};
}  // namespace

int main(int argc, char** argv) {
  namespace vocab = decaflash::mainframe::text_model;
  if (argc < 9) {
    std::fprintf(stderr, "usage: %s <model.bin> <event> <m0> <m1> <m2> <m3> <m4> <seed>...\n", argv[0]);
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
  if (kernel.load(image.data(), image.size()) != slm::LoadResult::Ok ||
      !kernel.allocate(hostAlloc, hostAlloc, hostAlloc)) {
    std::fprintf(stderr, "%s: no usable image\n", argv[1]);
    return 1;
  }
  uint16_t condition[slm::kConditionTokens];
  condition[0] = vocab::kStartToken;
  condition[1] = static_cast<uint16_t>(vocab::kFirstEventToken + std::atoi(argv[2]));
  for (int value = 0; value < 5; ++value) {
    condition[2 + value] =
        static_cast<uint16_t>(vocab::kFirstMoodToken + 3 * value + std::atoi(argv[3 + value]));
  }
  for (int i = 8; i < argc; ++i) {
    const uint32_t seed = static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 0));
    kernel.reset();
    PrintSink sink;
    const slm::WriteEnd end = slm::writeText(kernel, condition, seed, sink);
    std::printf("%lu\t%s\t%s\n", static_cast<unsigned long>(seed), slm::writeEndName(end),
                sink.text.c_str());
  }
  return 0;
}
