// Per-token confidence of the text model on the device's 4-bit path (measurement
// for the change add-text-model-confidence-gate). Runs the firmware's writer
// (text_writer.h: masks, sampling schedule, length limit) on a model image and
// records, for every token the writer chose and delivered, the values a
// confidence gate could use. The writer is not changed: a wrapper model copies
// the logits of each pass, and the token chosen from a pass is the input of the
// next pass (or the end token when the writer ends with Done). A token chosen
// but not delivered (length limit) is not recorded.
//
// Build:  c++ -std=c++17 -O2 -I apps/mainframe/src apps/mainframe/tools/text_model_measure.cpp \
//           -o text_model_measure
// Usage:  text_model_measure <model.bin> < cases
//         one case per line: <event 0-11> <five mood levels 0-2> <seed>
// Output: one JSON object per case: event, levels, seed, end, text and
//         tokens [{id, piece, space, lp, raw, entropy, margin}], where lp is the
//         log-probability of the chosen token in the sampling distribution of
//         its step (top-k at the step's temperature), raw the log-probability at
//         temperature 1 over the masked row, entropy that of the sampling
//         distribution and margin the log-probability difference of its two
//         best tokens.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "slm/slm_kernel.h"
#include "slm/text_writer.h"

namespace {
namespace vocab = decaflash::mainframe::text_model;

void* hostAlloc(size_t bytes) { return std::calloc(1, bytes); }

struct Step {
  int first = 0;
  std::vector<float> logits;
  float temperature = 0;
  int topK = 0;
};

struct TokenValues {
  int id;
  double lp, raw, entropy, margin;
};

TokenValues values(const Step& step, int chosen) {
  const std::vector<float>& row = step.logits;
  std::vector<int> order(row.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
  std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return row[a] > row[b]; });
  const int kept = std::min<int>(step.topK, static_cast<int>(row.size()));
  double rawSum = 0;
  const double top = row[order[0]];
  for (float v : row) rawSum += std::exp(v - top);
  double sum = 0;
  for (int i = 0; i < kept; ++i) sum += std::exp((row[order[i]] - top) / step.temperature);
  double entropy = 0, lp = -INFINITY;
  for (int i = 0; i < kept; ++i) {
    const double l = (row[order[i]] - top) / step.temperature - std::log(sum);
    entropy -= std::exp(l) * l;
    if (order[i] + step.first == chosen) lp = l;
  }
  const double margin = kept > 1 ? (row[order[0]] - row[order[1]]) / step.temperature : 99.0;
  const double raw = (row[chosen - step.first] - top) - std::log(rawSum);
  return {chosen, lp, raw, entropy, margin};
}

// The kernel, recording the logits of every output pass.
struct RecordingModel {
  slm::Kernel* kernel;
  int calls = 0;
  std::vector<Step> steps;
  std::vector<TokenValues> tokens;

  const float* forward(int token, int firstRow, int rowCount) {
    if (calls >= slm::kConditionTokens) tokens.push_back(values(steps.back(), token));
    const float* logits = kernel->forward(token, firstRow, rowCount);
    if (++calls >= slm::kConditionTokens && logits) {
      Step step;
      step.first = firstRow;
      step.logits.assign(logits + firstRow, logits + firstRow + rowCount);
      // Schedule of text_writer.h: emoji and first text token wide, the rest narrow.
      const bool opening = steps.size() < 2;
      step.temperature = opening ? slm::kOpeningTemperature : slm::kRestTemperature;
      step.topK = opening ? slm::kOpeningTopK : slm::kRestTopK;
      steps.push_back(step);
    }
    return logits;
  }
};

struct TextSink {
  std::string text;
  bool cancelled() { return false; }
  void emoji(uint16_t token, float) { text = vocab::kTokens[token].utf8; }
  void word(const char* utf8, const float*, int) { text += std::string(" ") + utf8; }
  void end(float) {}
};

std::string json(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') out += '\\';
    out += c;
  }
  return out + "\"";
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: %s <model.bin> < cases\n", argv[0]);
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
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    int event, levels[5];
    unsigned long seed;
    if (!(in >> event >> levels[0] >> levels[1] >> levels[2] >> levels[3] >> levels[4] >> seed)) {
      continue;
    }
    uint16_t condition[slm::kConditionTokens];
    condition[0] = vocab::kStartToken;
    condition[1] = static_cast<uint16_t>(vocab::kFirstEventToken + event);
    for (int value = 0; value < 5; ++value) {
      condition[2 + value] = static_cast<uint16_t>(vocab::kFirstMoodToken + 3 * value + levels[value]);
    }
    kernel.reset();
    RecordingModel model{&kernel};
    TextSink sink;
    const slm::WriteEnd end =
        slm::writeText(model, condition, static_cast<uint32_t>(seed), sink);
    if (end == slm::WriteEnd::Done) model.tokens.push_back(values(model.steps.back(), vocab::kEndToken));
    std::printf("{\"event\": %d, \"levels\": [%d, %d, %d, %d, %d], \"seed\": %lu, \"end\": \"%s\", "
                "\"text\": %s, \"tokens\": [",
                event, levels[0], levels[1], levels[2], levels[3], levels[4], seed,
                slm::writeEndName(end), json(sink.text).c_str());
    uint16_t previous = vocab::kEndToken;
    for (size_t i = 0; i < model.tokens.size(); ++i) {
      const TokenValues& t = model.tokens[i];
      const bool space = i > 1 && t.id != vocab::kEndToken &&
                         vocab::spaceBefore(previous, static_cast<uint16_t>(t.id));
      std::printf("%s{\"id\": %d, \"piece\": %s, \"space\": %s, \"lp\": %.6f, \"raw\": %.6f, "
                  "\"entropy\": %.6f, \"margin\": %.6f}",
                  i ? ", " : "", t.id, json(t.id ? vocab::kTokens[t.id].utf8 : "").c_str(),
                  space ? "true" : "false", t.lp, t.raw, t.entropy, t.margin);
      if (i) previous = static_cast<uint16_t>(t.id);
    }
    std::printf("]}\n");
  }
  return 0;
}
