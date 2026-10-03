#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

#include "v2_tempo_tracker.h"

namespace {

constexpr uint32_t kSampleRateHz = 16000;
constexpr size_t kBlockSamples = 256;
constexpr float kPi = 3.14159265358979323846f;

void feedKickTrain(decaflash::mainframe::V2TempoTracker& tracker, uint16_t bpm,
                   uint32_t durationMs, uint32_t startMs = 0) {
  const uint32_t periodSamples = (60UL * kSampleRateHz) / bpm;
  const uint32_t blockCount = (durationMs * kSampleRateHz) / (1000UL * kBlockSamples);
  std::array<int16_t, kBlockSamples> samples = {};

  for (uint32_t block = 0; block < blockCount; ++block) {
    for (size_t sample = 0; sample < samples.size(); ++sample) {
      const uint32_t absoluteSample = block * kBlockSamples + sample;
      const uint32_t phase = absoluteSample % periodSamples;
      // A short, low-frequency kick plus quiet wideband-ish background.  This
      // intentionally provides multiple PCM samples per beat, not a synthetic
      // level envelope tailored to the implementation.
      const float kickEnvelope = phase < 960U ? (1.0f - phase / 960.0f) : 0.0f;
      const float kick = kickEnvelope * 9000.0f * std::sin(
        2.0f * kPi * 92.0f * absoluteSample / kSampleRateHz);
      const float background = 80.0f * std::sin(
        2.0f * kPi * 750.0f * absoluteSample / kSampleRateHz);
      samples[sample] = static_cast<int16_t>(std::lround(kick + background));
    }
    const uint32_t timestampMs = startMs + ((block + 1U) * kBlockSamples * 1000UL) / kSampleRateHz;
    tracker.feed(timestampMs, samples.data(), samples.size());
  }
}

void expectTempo(uint16_t bpm) {
  decaflash::mainframe::V2TempoTracker tracker;
  feedKickTrain(tracker, bpm, 12000);
  const auto& estimate = tracker.estimate();
  std::printf("%u BPM input -> %u BPM, confidence=%u, frames=%lu\n", bpm, estimate.bpm,
              estimate.confidence, static_cast<unsigned long>(estimate.analysisFrames));
  assert(estimate.bpm >= bpm - 2U && estimate.bpm <= bpm + 2U);
  assert(estimate.confidence >= 45U);
  assert(estimate.lastOnsetAtMs != 0);
}

void expectCaptureGapReset() {
  decaflash::mainframe::V2TempoTracker tracker;
  feedKickTrain(tracker, 120, 8000);
  assert(tracker.estimate().bpm >= 118U && tracker.estimate().bpm <= 122U);

  std::array<int16_t, kBlockSamples> silence = {};
  tracker.feed(8100, silence.data(), silence.size());
  assert(tracker.estimate().bpm == 0);
  assert(tracker.estimate().analysisFrames == 1);
}

void expectTempoTransition() {
  decaflash::mainframe::V2TempoTracker tracker;
  feedKickTrain(tracker, 100, 12000);
  assert(tracker.estimate().bpm >= 98U && tracker.estimate().bpm <= 102U);

  // A sustained new track must eventually replace the established clock.
  feedKickTrain(tracker, 120, 16000, 12000);
  const auto& estimate = tracker.estimate();
  std::printf("100 -> 120 BPM transition -> %u BPM, confidence=%u\n", estimate.bpm,
              estimate.confidence);
  assert(estimate.bpm >= 118U && estimate.bpm <= 122U);
}

}  // namespace

int main() {
  expectTempo(100);
  expectTempo(117);
  expectTempo(126);
  expectTempo(128);
  expectTempo(160);
  expectTempo(180);
  expectCaptureGapReset();
  expectTempoTransition();
  std::puts("PASS: V2 PCM tempo tracker resolves clear kick trains at multiple tempi");
}
