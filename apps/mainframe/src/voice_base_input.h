#pragma once

#include <Arduino.h>
#include <atomic>

#include "beat_analyzer.h"
#include "bpm_tracker.h"
#include "iris_vu.h"
#include "audio_mood_features.h"

namespace decaflash::mainframe {

struct BpmTrackerMetrics {
  uint32_t processedFrames = 0;
  uint64_t totalMicros = 0;
  uint32_t maxMicros = 0;
};

// Voice Base I2S capture. It owns capture buffers and never changes show state.
class VoiceBaseInput {
 public:
  bool begin();
  void update(BeatAnalyzer& analyzer);
  // Audio worker owns these only while the main loop has yielded capture.
  void suspend();
  void discardCompleted();

  uint8_t vuLevel(uint32_t now) const { return ready_ ? vu_.level(now) : 0; }

  bool fresh(uint32_t now) const { return ready_ && hasSamples_ && now - lastSampleAtMs_ <= 250; }

  const AudioMoodFeatures& moodFeatures() const { return moodFeatures_; }
  const BpmTracker::Estimate& bpmTracker() const { return bpmTracker_.estimate(); }
  const BpmTrackerMetrics& bpmTrackerMetrics() const { return bpmTrackerMetrics_; }
  uint32_t analysisDrops() const { return analysisDrops_.load(std::memory_order_acquire); }
  uint32_t requeueFailures() const { return requeueFailures_.load(std::memory_order_acquire); }
  uint8_t analysisBacklogHighWater() const {
    return analysisBacklogHighWater_.load(std::memory_order_acquire);
  }
  static constexpr size_t analysisRingBytes() {
    return kAnalysisRingFrames * kSampleCount * sizeof(int16_t);
  }

  bool ready() const { return ready_; }

 private:
  static void onBufferReady(void* context, void* data, size_t length);
  bool queueBuffer(uint8_t index);
  void processBuffer(const int16_t* samples, uint32_t audioNowMs, uint32_t sequence,
                     BeatAnalyzer& analyzer);
  void releaseAnalysisRing();

  static constexpr size_t kSampleCount = 256;
  // 256 ms covers the measured 129 ms longest display-side pause with ample
  // headroom.  This copy lives in PSRAM so it does not consume LLM RAM.
  static constexpr uint8_t kAnalysisRingFrames = 16;
  static constexpr uint8_t kAnalysisBlocksPerFrame = 4;
  int16_t buffers_[2][kSampleCount] = {};
  std::atomic<uint8_t> pendingQueueMask_{0};
  int16_t* analysisRing_ = nullptr;
  uint32_t analysisTimesMs_[kAnalysisRingFrames] = {};
  uint32_t analysisSequences_[kAnalysisRingFrames] = {};
  std::atomic<uint8_t> analysisRead_{0};
  std::atomic<uint8_t> analysisWrite_{0};
  std::atomic<uint32_t> analysisDrops_{0};
  std::atomic<uint32_t> requeueFailures_{0};
  std::atomic<uint8_t> analysisBacklogHighWater_{0};
  IrisVu vu_;
  AudioMoodFeatures moodFeatures_;
  BpmTracker bpmTracker_;
  BpmTrackerMetrics bpmTrackerMetrics_;
  int32_t dcEstimate_ = 0;
#if DECAFLASH_BPM_TRACE
  int32_t lowBandEstimate_ = 0;
#endif
  int32_t previousPercussiveSample_ = 0;
  uint32_t pendingLevelSum_ = 0;
  uint16_t pendingPeak_ = 0;
  uint8_t pendingBlockCount_ = 0;
  uint32_t lastSampleAtMs_ = 0;
  uint32_t sampleClockOriginMs_ = 0;
  uint64_t capturedSamples_ = 0;
  uint32_t capturedSequence_ = 0;
  uint32_t lastAnalysisSequence_ = 0;
  bool hasAnalysisSequence_ = false;
  bool hasSamples_ = false;
  std::atomic<bool> captureActive_{false};
  bool ready_ = false;
};

}  // namespace decaflash::mainframe
