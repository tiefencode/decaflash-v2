// Host test of the language model kernel wrapper (apps/mainframe/src/slm).
// Argument: a tiny image written by apps/mainframe/tools/slm_make_image.cpp
// with the arguments in tests/run_slm_kernel.sh.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "slm/slm_kernel.h"

namespace {

int failures = 0;

void check(bool condition, const char* what) {
  if (!condition) {
    std::printf("FAIL: %s\n", what);
    ++failures;
  }
}

std::vector<uint8_t> readFile(const char* path) {
  std::vector<uint8_t> bytes;
  FILE* file = std::fopen(path, "rb");
  if (file == nullptr) return bytes;
  uint8_t buffer[4096];
  size_t count;
  while ((count = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
    bytes.insert(bytes.end(), buffer, buffer + count);
  }
  std::fclose(file);
  return bytes;
}

void* hostAlloc(size_t bytes) { return std::calloc(1, bytes); }
void* failingAlloc(size_t) { return nullptr; }

void putU32(std::vector<uint8_t>& image, size_t offset, uint32_t value) {
  std::memcpy(image.data() + offset, &value, 4);
}

// Prompt of three tokens, then greedy generation. Output classes are fed back
// as input tokens directly; the tiny image has no vocabulary mapping.
std::vector<int> generate(slm::Kernel& kernel, int pieces) {
  std::vector<int> out;
  kernel.reset();
  kernel.step(1);
  kernel.step(2);
  int next = kernel.step(3);
  for (int i = 0; i < pieces && next >= 0; ++i) {
    out.push_back(next);
    next = kernel.step(next);
  }
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <tiny.bin>\n", argv[0]);
    return 2;
  }
  const std::vector<uint8_t> image = readFile(argv[1]);
  check(!image.empty(), "tiny image is readable");
  if (image.empty()) return 1;

  // A valid image loads, reports its dimensions and consumes the whole file.
  static slm::Kernel kernel;
  check(kernel.load(image.data(), image.size()) == slm::LoadResult::Ok, "valid image loads");
  check(kernel.imageBytes() == image.size(), "image size equals file size");
  check(kernel.inputVocab() == 40 && kernel.outputVocab() == 20, "vocabulary sizes");
  check(kernel.layers() == 2 && kernel.dimension() == 16 && kernel.contextLength() == 24,
        "dimensions");
  check(!kernel.ready() && kernel.step(1) == -1, "no step before allocation");
  check(kernel.allocate(hostAlloc, hostAlloc, hostAlloc) && kernel.ready(), "allocation");

  // Fixed input gives fixed output pieces. Recorded from this implementation on
  // the host (regression value, not an independent reference).
  const std::vector<int> first = generate(kernel, 12);
  const std::vector<int> expected = {12, 19, 6, 4, 8, 6, 4, 8, 4, 8, 8, 4};
  check(first == expected, "output pieces equal the recorded ones");
  if (first != expected) {
    std::printf("got:");
    for (int piece : first) std::printf(" %d,", piece);
    std::printf("\n");
  }
  check(generate(kernel, 12) == first, "reset gives the same pieces again");

  // The context is bounded and tokens are range-checked.
  kernel.reset();
  for (int i = 0; i < 24; ++i) check(kernel.step(i % 40) >= 0, "step inside the context");
  check(kernel.step(0) == -1, "step beyond the context is refused");
  kernel.reset();
  check(kernel.step(40) == -1 && kernel.step(-1) == -1, "token out of range is refused");

  // Missing or invalid images are rejected without reading past the buffer.
  static slm::Kernel other;
  check(other.load(nullptr, 0) == slm::LoadResult::TooSmall, "no image");
  check(other.load(image.data(), 55) == slm::LoadResult::TooSmall, "shorter than a header");
  const std::vector<uint8_t> erased(4096, 0xFF);
  check(other.load(erased.data(), erased.size()) == slm::LoadResult::BadMagic, "erased flash");
  std::vector<uint8_t> broken = image;
  putU32(broken, 0, 0x12345678u);
  check(other.load(broken.data(), broken.size()) == slm::LoadResult::BadMagic, "wrong magic");
  broken = image;
  putU32(broken, 4, 2);
  check(other.load(broken.data(), broken.size()) == slm::LoadResult::BadVersion, "newer version");
  broken = image;
  putU32(broken, 12, 0x80);
  check(other.load(broken.data(), broken.size()) == slm::LoadResult::BadVersion, "unknown flag");
  broken = image;
  putU32(broken, 24, 0);
  check(other.load(broken.data(), broken.size()) == slm::LoadResult::BadHeader, "dimension zero");
  broken = image;
  putU32(broken, 28, 33);
  check(other.load(broken.data(), broken.size()) == slm::LoadResult::BadHeader, "too many layers");
  broken = image;
  putU32(broken, 56, 0);  // group prefix of the first tensor
  check(other.load(broken.data(), broken.size()) == slm::LoadResult::BadHeader, "group zero");
  check(other.load(image.data(), image.size() - 1) == slm::LoadResult::Truncated,
        "one byte short");
  check(other.load(image.data(), 60) == slm::LoadResult::Truncated, "header only");
  check(!other.ready() && other.step(1) == -1, "rejected image leaves the kernel off");

  // A failed allocation leaves the kernel off.
  check(other.load(image.data(), image.size()) == slm::LoadResult::Ok, "reload");
  check(!other.allocate(failingAlloc, hostAlloc, hostAlloc) && !other.ready(),
        "failed weight allocation");
  static slm::Kernel third;
  check(third.load(image.data(), image.size()) == slm::LoadResult::Ok, "third load");
  check(!third.allocate(hostAlloc, hostAlloc, failingAlloc) && !third.ready() &&
            third.step(1) == -1,
        "failed cache allocation");

  if (failures == 0) std::printf("slm_kernel: all checks passed\n");
  return failures == 0 ? 0 : 1;
}
