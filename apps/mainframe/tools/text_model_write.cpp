// Host tool for the text model (add-text-model, task 7.1): writes texts with
// the firmware's writer (masks, sampling schedule, length limit) on a
// model image, one line per seed, for comparison with the device: seed, end,
// the confidence gate's decision (kept or rejected), its token and running
// mean, as the device's TEXT line prints them, and the text.
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/text_model_write.cpp \
//           -o text_model_write
// Usage:  text_model_write [-s key=value]... <model.bin> <event 0-11> <five mood levels 0-2> <seed>...
//         text_model_write [-s key=value]... <model.bin> -
//           reads lines "<event> <five levels> <seed>" from stdin and writes
//           them as one sequence (one repetition memory for all lines).
//
// -s overrides one writer setting (slm::WriterSettings; default the firmware's
// kWriterSettings; "plain=1" first switches to the writer before
// improve-text-model step 2): ot, ok (opening temperature, top-k), rt, rk (rest),
// rep (texts the repetition block remembers, 0 off), exact (0/1), wordpen,
// openpen,
// ngram, emojipen, mask (0/1, word mask per event), rare (penalty on
// once-only words). The seeds are written in order as one sequence: with the
// repetition block on, each text is remembered for the following ones, as the
// firmware remembers the texts it showed.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "slm/slm_kernel.h"
#include "slm/text_writer.h"
#include "text_model_gate.h"

namespace {
void* hostAlloc(size_t bytes) { return std::calloc(1, bytes); }

struct PrintSink {
  std::string text;
  decaflash::mainframe::ConfidenceGate gate;
  bool cancelled() { return false; }
  void emoji(uint16_t token, float margin) {
    text = decaflash::mainframe::text_model::kTokens[token].utf8;
    gate.add(margin);
  }
  void word(const char* utf8, const float* margins, int count) {
    text += std::string(" ") + utf8;
    for (int i = 0; i < count; ++i) gate.add(margins[i]);
  }
  void end(float margin) { gate.add(margin); }
};
}  // namespace

bool applySetting(slm::WriterSettings& s, const std::string& option) {
  const size_t eq = option.find('=');
  if (eq == std::string::npos) return false;
  const std::string key = option.substr(0, eq);
  const char* value = option.c_str() + eq + 1;
  const float f = static_cast<float>(std::atof(value));
  const int i = std::atoi(value);
  if (key == "plain") s = slm::plainWriterSettings();
  else if (key == "ot") s.openingTemperature = f;
  else if (key == "ok") s.openingTopK = i;
  else if (key == "rt") s.restTemperature = f;
  else if (key == "rk") s.restTopK = i;
  else if (key == "rep") s.repetition.texts = i;
  else if (key == "exact") s.repetition.exact = i != 0;
  else if (key == "wordpen") s.repetition.wordPenalty = f;
  else if (key == "openpen") s.repetition.openingPenalty = f;
  else if (key == "ngram") s.repetition.ngram = i;
  else if (key == "emojipen") s.repetition.emojiPenalty = f;
  else if (key == "mask") s.eventMask = i != 0;
  else if (key == "rare") s.rarePenalty = f;
  else return false;
  return true;
}

int main(int argc, char** argv) {
  namespace vocab = decaflash::mainframe::text_model;
  slm::WriterSettings settings = slm::kWriterSettings;
  while (argc > 2 && std::string(argv[1]) == "-s") {
    if (!applySetting(settings, argv[2])) {
      std::fprintf(stderr, "unknown setting: %s\n", argv[2]);
      return 2;
    }
    argc -= 2;
    argv += 2;
  }
  const bool sequence = argc == 3 && std::string(argv[2]) == "-";
  if (argc < 9 && !sequence) {
    std::fprintf(stderr, "usage: %s [-s key=value]... <model.bin> (<event> <m0> <m1> <m2> <m3> <m4> <seed>... | -)\n", argv[0]);
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
  static slm::RecentTexts recent;
  uint16_t condition[slm::kConditionTokens];
  auto setCondition = [&](int event, const int* levels) {
    condition[0] = vocab::kStartToken;
    condition[1] = static_cast<uint16_t>(vocab::kFirstEventToken + event);
    for (int value = 0; value < 5; ++value) {
      condition[2 + value] = static_cast<uint16_t>(vocab::kFirstMoodToken + 3 * value + levels[value]);
    }
  };
  std::vector<uint32_t> seeds;
  std::vector<std::vector<int>> conditions;  // event and five levels per seed
  if (sequence) {
    int e, l0, l1, l2, l3, l4;
    unsigned long seed;
    while (std::scanf("%d %d %d %d %d %d %lu", &e, &l0, &l1, &l2, &l3, &l4, &seed) == 7) {
      conditions.push_back({e, l0, l1, l2, l3, l4});
      seeds.push_back(static_cast<uint32_t>(seed));
    }
  } else {
    std::vector<int> c = {std::atoi(argv[2])};
    for (int value = 0; value < 5; ++value) c.push_back(std::atoi(argv[3 + value]));
    for (int i = 8; i < argc; ++i) {
      conditions.push_back(c);
      seeds.push_back(static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 0)));
    }
  }
  for (size_t i = 0; i < seeds.size(); ++i) {
    const uint32_t seed = seeds[i];
    setCondition(conditions[i][0], conditions[i].data() + 1);
    kernel.reset();
    PrintSink sink;
    const slm::WriteEnd end = slm::writeText(kernel, condition, seed, sink, settings, &recent);
    if (end == slm::WriteEnd::Done || end == slm::WriteEnd::LengthLimit) recent.commit();
    std::printf("%lu\t%s\tgate=%s gate_token=%d gate_mean=%.2f threshold=%.2f\t%s\n",
                static_cast<unsigned long>(seed), slm::writeEndName(end),
                sink.gate.cancelled() ? "rejected" : "kept",
                sink.gate.cancelled() ? sink.gate.cancelledAt() : sink.gate.tokens(),
                static_cast<double>(sink.gate.mean()),
                static_cast<double>(decaflash::mainframe::kGateThreshold), sink.text.c_str());
  }
  return 0;
}
