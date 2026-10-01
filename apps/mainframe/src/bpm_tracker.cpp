#include "bpm_tracker.h"

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kEvaluationIntervalMs = 512;
constexpr uint16_t kMinimumHistoryFrames = 200;
constexpr uint16_t kScoringFrames = 256;
constexpr uint16_t kOctavePeriodicityPercent = 60;
constexpr uint16_t kMinimumOctaveDirectSupport = 500;
constexpr uint32_t kOnsetCooldownMs = 200;

uint32_t difference(uint32_t left, uint32_t right) {
  return left > right ? left - right : right - left;
}

uint16_t clampPulse(uint32_t value) {
  return value > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(value);
}

}  // namespace

void BpmTracker::reset() {
  *this = BpmTracker{};
}

uint16_t BpmTracker::historyAt(uint16_t chronologicalIndex) const {
  const uint16_t first = static_cast<uint16_t>(
    (historyWrite_ + kHistorySize - historyCount_) % kHistorySize);
  return pulseHistory_[(first + chronologicalIndex) % kHistorySize];
}

void BpmTracker::registerOnset(uint32_t timestampMs) {
  if (lastOnsetAtMs_ != 0 && timestampMs - lastOnsetAtMs_ < kOnsetCooldownMs) return;
  lastOnsetAtMs_ = timestampMs;
  if (onsetCount_ < kEventHistorySize) {
    onsetTimesMs_[onsetCount_++] = timestampMs;
    return;
  }
  for (uint8_t index = 1; index < kEventHistorySize; ++index) {
    onsetTimesMs_[index - 1U] = onsetTimesMs_[index];
  }
  onsetTimesMs_[kEventHistorySize - 1U] = timestampMs;
}

uint16_t BpmTracker::directSupport(uint16_t bpm) const {
  if (onsetCount_ < 2U) return 0;
  const uint32_t periodMs = 60000UL / bpm;
  uint32_t fitWeight = 0;
  for (uint8_t index = 1; index < onsetCount_; ++index) {
    const uint32_t intervalMs = onsetTimesMs_[index] - onsetTimesMs_[index - 1U];
    const uint32_t toleranceMs = periodMs / 12U + 20U;
    const uint32_t errorMs = difference(intervalMs, periodMs);
    if (errorMs > toleranceMs) continue;
    fitWeight += (toleranceMs - errorMs) * 100U / toleranceMs;
  }
  const uint32_t support = fitWeight * 10U / (onsetCount_ - 1U);
  return support > 1000U ? 1000U : static_cast<uint16_t>(support);
}

uint16_t BpmTracker::scoreCandidate(uint16_t bpm) const {
  if (historyCount_ == 0 || frameIntervalMs_ == 0) return 0;

  const uint32_t periodMs = 60000UL / bpm;
  const uint32_t lagQ8 = (periodMs * 256UL + frameIntervalMs_ / 2U) / frameIntervalMs_;
  const uint16_t lagFrames = static_cast<uint16_t>(lagQ8 / 256UL);
  const uint16_t lagFraction = static_cast<uint16_t>(lagQ8 % 256UL);
  if (lagFrames < 2U || lagFrames + 1U >= historyCount_) return 0;

  const uint16_t firstScoreIndex = historyCount_ > kScoringFrames
    ? static_cast<uint16_t>(historyCount_ - kScoringFrames)
    : 0U;
  const uint16_t firstIndex = firstScoreIndex > lagFrames
    ? firstScoreIndex
    : static_cast<uint16_t>(lagFrames + 1U);
  uint64_t matchedEnergy = 0;
  uint64_t currentEnergy = 0;
  for (uint16_t index = firstIndex; index < historyCount_; ++index) {
    const uint16_t current = historyAt(index);
    const uint16_t newer = historyAt(static_cast<uint16_t>(index - lagFrames));
    const uint16_t older = historyAt(static_cast<uint16_t>(index - lagFrames - 1U));
    const uint32_t delayed =
      (static_cast<uint32_t>(newer) * (256U - lagFraction) +
       static_cast<uint32_t>(older) * lagFraction + 128U) >> 8U;
    matchedEnergy += current < delayed ? current : delayed;
    currentEnergy += current;
  }
  if (currentEnergy == 0) return 0;
  const uint16_t periodicity = static_cast<uint16_t>(
    (matchedEnergy * 1000ULL) / currentEnergy);
  return periodicity > 1000U ? 1000U : periodicity;
}

void BpmTracker::evaluate(uint32_t timestampMs) {
  if (lastEvaluationAtMs_ != 0 && timestampMs - lastEvaluationAtMs_ < kEvaluationIntervalMs) {
    return;
  }
  lastEvaluationAtMs_ = timestampMs;
  estimate_.analyzedFrames++;
  if (historyCount_ < kMinimumHistoryFrames) return;

  uint16_t bestBpm = 0;
  uint16_t bestPeriodicity = 0;
  // A four-BPM coarse pass finds the neighbourhood, then a nine-candidate
  // refinement keeps the output at one-BPM precision. This evaluates at most
  // 35 candidates instead of all 101 every half second.
  for (uint16_t candidate = kMinimumBpm; candidate <= kMaximumBpm; candidate += 4U) {
    const uint16_t periodicity = scoreCandidate(candidate);
    if (periodicity > bestPeriodicity) {
      bestPeriodicity = periodicity;
      bestBpm = candidate;
    }
  }
  const uint16_t refinementStart = bestBpm > kMinimumBpm + 4U
    ? static_cast<uint16_t>(bestBpm - 4U)
    : kMinimumBpm;
  const uint16_t refinementEnd = bestBpm + 4U < kMaximumBpm
    ? static_cast<uint16_t>(bestBpm + 4U)
    : kMaximumBpm;
  for (uint16_t candidate = refinementStart; candidate <= refinementEnd; ++candidate) {
    const uint16_t periodicity = scoreCandidate(candidate);
    if (periodicity > bestPeriodicity) {
      bestPeriodicity = periodicity;
      bestBpm = candidate;
    }
  }

  // The lower period is the default whenever the audio repeats at both the
  // beat and every second beat. Promote exactly one octave only when actual
  // onset intervals support it and its own periodicity is still substantial.
  uint16_t selectedBpm = bestBpm;
  uint16_t selectedPeriodicity = bestPeriodicity;
  uint16_t selectedDirectSupport = directSupport(bestBpm);
  const uint32_t doubleBpm = static_cast<uint32_t>(bestBpm) * 2U;
  if (doubleBpm <= kMaximumBpm) {
    const uint16_t octaveBpm = static_cast<uint16_t>(doubleBpm);
    const uint16_t octavePeriodicity = scoreCandidate(octaveBpm);
    const uint16_t octaveDirectSupport = directSupport(octaveBpm);
    if (octaveDirectSupport >= kMinimumOctaveDirectSupport &&
        static_cast<uint32_t>(octavePeriodicity) * 100U >=
          static_cast<uint32_t>(bestPeriodicity) * kOctavePeriodicityPercent) {
      selectedBpm = octaveBpm;
      selectedPeriodicity = octavePeriodicity;
      selectedDirectSupport = octaveDirectSupport;
    }
  }

  estimate_.rawBpm = selectedBpm;
  estimate_.periodicityPermille = selectedPeriodicity;
  estimate_.directSupportPermille = selectedDirectSupport;
  if (selectedPeriodicity <= 280U) {
    estimate_.rawConfidence = 0;
  } else {
    const uint16_t confidence = (selectedPeriodicity - 280U) / 2U;
    estimate_.rawConfidence = confidence > 100U ? 100U : static_cast<uint8_t>(confidence);
  }

  // V2 estimates the present audio window only.  It deliberately has no
  // fade, silence, confidence gate or song-change state machine. Consumers
  // can use confidence as an explicit signal without losing the estimate.
  estimate_.bpm = selectedBpm;
  estimate_.confidence = estimate_.rawConfidence;
}

void BpmTracker::feed(uint32_t timestampMs, uint32_t level) {
  if (previousFrameAtMs_ != 0) {
    const uint32_t intervalMs = timestampMs - previousFrameAtMs_;
    if (intervalMs >= 8U && intervalMs <= 32U) {
      frameIntervalMs_ = static_cast<uint16_t>(intervalMs);
    }
  }
  previousFrameAtMs_ = timestampMs;

  const uint16_t clampedLevel = level > UINT16_MAX ? UINT16_MAX : static_cast<uint16_t>(level);
  const uint32_t risingFlux = clampedLevel > previousLevel_ ? clampedLevel - previousLevel_ : 0U;
  previousLevel_ = clampedLevel;
  fluxNoiseFloor_ = (fluxNoiseFloor_ * 31U + risingFlux) / 32U;
  uint32_t threshold = fluxNoiseFloor_ * 3U;
  if (threshold < 12U) threshold = 12U;
  const uint16_t pulse = risingFlux > threshold ? clampPulse(risingFlux - threshold) : 0U;

  pulseHistory_[historyWrite_] = pulse;
  historyWrite_ = static_cast<uint16_t>((historyWrite_ + 1U) % kHistorySize);
  if (historyCount_ < kHistorySize) ++historyCount_;
  if (pulse != 0) registerOnset(timestampMs);
  evaluate(timestampMs);
}

}  // namespace decaflash::mainframe
