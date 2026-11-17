// Host tool for the language model prototype (docs/slm-prototype.md): writes a
// model image with random weights in the format of apps/mainframe/src/slm/llm.h.
// Random weights cost the same per output piece as trained ones, so such images
// serve load measurements. The same arguments always give the same file.
//
// Build:  c++ -std=c++17 -O2 apps/mainframe/tools/slm_make_image.cpp -o slm_make_image
// Usage:  slm_make_image <out.bin> <seed> <dim> <layers> <heads> <ffn> <ple_dim>
//                        <vocab_in> <vocab_out> <seq_len> <group>
// Reference size ("Barista"): dim 128, layers 6, heads 4, ffn 384, ple_dim 128,
// vocab_in 8057, vocab_out 854, seq_len 128, group 128 (4,600,186 bytes).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t kMagic = 0x00454C50u;
constexpr uint32_t kVersion = 1;
constexpr uint32_t kHeaderBytes = 56;

struct Rng {
  uint64_t state;
  uint32_t next() {  // splitmix64, upper half
    state += 0x9E3779B97F4A7C15ull;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return static_cast<uint32_t>((z ^ (z >> 31)) >> 32);
  }
};

uint16_t floatToHalf(float value) {  // positive normal values only
  uint32_t bits;
  std::memcpy(&bits, &value, 4);
  const int exponent = static_cast<int>((bits >> 23) & 0xFF) - 127 + 15;
  const uint32_t mantissa = (bits >> 13) & 0x3FF;
  if (exponent <= 0) return 1;  // smallest subnormal instead of zero
  if (exponent >= 31) return 0x7BFF;
  return static_cast<uint16_t>((exponent << 10) | mantissa);
}

struct Writer {
  std::vector<uint8_t> bytes;
  Rng rng;
  int group;

  void u32(uint32_t value) { raw(&value, 4); }
  void raw(const void* data, size_t size) {
    const auto* p = static_cast<const uint8_t*>(data);
    bytes.insert(bytes.end(), p, p + size);
  }
  // Group-wise 4-bit tensor: group prefix, packed codes per row, fp16 scales.
  void quant(int rows, int cols) {
    u32(static_cast<uint32_t>(group));
    const int rowBytes = (cols + 1) / 2;
    const int groups = (cols + group - 1) / group;
    for (int r = 0; r < rows; ++r) {
      for (int b = 0; b < rowBytes; ++b) {
        const uint32_t v = rng.next();
        const uint8_t low = static_cast<uint8_t>(1 + v % 15);          // value -7..7
        const uint8_t high = static_cast<uint8_t>(1 + (v >> 8) % 15);
        bytes.push_back(static_cast<uint8_t>(low | (high << 4)));
      }
    }
    // Keeps a row's output near unit size for unit-size input.
    const float base = 1.0f / (4.0f * std::sqrt(static_cast<float>(cols)));
    for (int i = 0; i < rows * groups; ++i) {
      const float scale = base * (0.5f + static_cast<float>(rng.next() % 1024) / 1024.0f);
      const uint16_t half = floatToHalf(scale);
      raw(&half, 2);
    }
  }
  void ones(int count) {
    const float one = 1.0f;
    for (int i = 0; i < count; ++i) raw(&one, 4);
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc != 12) {
    std::fprintf(stderr,
                 "usage: %s <out.bin> <seed> <dim> <layers> <heads> <ffn> <ple_dim> "
                 "<vocab_in> <vocab_out> <seq_len> <group>\n",
                 argv[0]);
    return 2;
  }
  const uint64_t seed = std::strtoull(argv[2], nullptr, 10);
  int v[9];
  for (int i = 0; i < 9; ++i) v[i] = std::atoi(argv[3 + i]);
  const int dim = v[0], layers = v[1], heads = v[2], ffn = v[3], pleDim = v[4];
  const int vocabIn = v[5], vocabOut = v[6], seqLen = v[7], group = v[8];
  if (dim <= 0 || layers <= 0 || layers > 32 || heads <= 0 || dim % heads != 0 ||
      (dim / heads) % 2 != 0 || ffn <= 0 || pleDim <= 0 || vocabIn <= 0 || vocabOut <= 0 ||
      vocabOut > vocabIn || seqLen <= 0 || group <= 0) {
    std::fprintf(stderr, "invalid dimensions\n");
    return 2;
  }

  Writer w{{}, Rng{seed}, group};
  w.u32(kMagic);
  w.u32(kVersion);
  w.u32(kHeaderBytes);
  w.u32(0);  // flags: separate output head, as vocab_out may be smaller
  w.u32(static_cast<uint32_t>(vocabIn));
  w.u32(static_cast<uint32_t>(vocabOut));
  for (int value : {dim, layers, heads, ffn, pleDim, seqLen, group}) {
    w.u32(static_cast<uint32_t>(value));
  }
  const float ropeTheta = 10000.0f;
  w.raw(&ropeTheta, 4);

  w.quant(vocabIn, dim);            // token embedding
  w.quant(layers * pleDim, dim);    // per-layer input projection
  w.ones(pleDim);
  w.quant(vocabIn, layers * pleDim);  // per-layer token table
  for (int layer = 0; layer < layers; ++layer) {
    w.ones(dim);
    w.quant(3 * dim, dim);
    w.quant(dim, dim);
    w.ones(dim);
    w.quant(ffn, dim);
    w.quant(ffn, dim);
    w.quant(dim, ffn);
    w.quant(pleDim, dim);
    w.quant(dim, pleDim);
    w.ones(dim);
  }
  w.ones(dim);
  w.quant(vocabOut, dim);  // output head

  FILE* file = std::fopen(argv[1], "wb");
  if (file == nullptr) {
    std::perror(argv[1]);
    return 1;
  }
  const bool written = std::fwrite(w.bytes.data(), 1, w.bytes.size(), file) == w.bytes.size();
  std::fclose(file);
  if (!written) {
    std::fprintf(stderr, "short write\n");
    return 1;
  }
  std::printf("%s: %zu bytes\n", argv[1], w.bytes.size());
  return 0;
}
