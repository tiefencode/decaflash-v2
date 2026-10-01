#include "bpm_tracker.h"

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kEvaluationIntervalMs = 512;
constexpr uint16_t kMinimumHistoryFrames = 200;
constexpr uint16_t kScoringFrames = 256;
constexpr uint8_t kMinimumOnsets = 4;
constexpr uint32_t kOnsetCooldownMs = 200;
constexpr uint8_t kMinimumLockConfidence = 45;
constexpr uint32_t kScoringWindowMs = 4000;
constexpr uint16_t kTempoToleranceBpm = 3;

uint16_t difference(uint16_t left, uint16_t right) {
  return left > right ? left - right : right - left;
}

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
    onsetTimesMs_[index - 1] = onsetTimesMs_[index];
  }
  onsetTimesMs_[kEventHistorySize - 1] = timestampMs;
}

uint16_t BpmTracker::scoreCandidate(uint16_t bpm, uint16_t& periodicity,
                                    uint16_t& directSupport) const {
  periodicity = 0;
  directSupport = 0;
  if (historyCount_ == 0 || frameIntervalMs_ == 0) return 0;

  const uint32_t periodMs = 60000UL / bpm;
  const uint32_t lagQ8 = (periodMs * 256UL + frameIntervalMs_ / 2U) / frameIntervalMs_;
  const uint16_t lagFrames = static_cast<uint16_t>(lagQ8 / 256UL);
  const uint16_t lagFraction = static_cast<uint16_t>(lagQ8 % 256UL);
  if (lagFrames < 2U || lagFrames + 1U >= historyCount_) return 0;

  uint64_t matchedEnergy = 0;
  uint64_t currentEnergy = 0;
  const uint16_t firstScoreIndex = historyCount_ > kScoringFrames
    ? static_cast<uint16_t>(historyCount_ - kScoringFrames)
    : 0U;
  const uint16_t firstIndex = firstScoreIndex > lagFrames
    ? firstScoreIndex
    : static_cast<uint16_t>(lagFrames + 1U);
  for (uint16_t index = firstIndex;
       index < historyCount_; ++index) {
    const uint16_t current = historyAt(index);
    const uint16_t newer = historyAt(static_cast<uint16_t>(index - lagFrames));
    const uint16_t older = historyAt(static_cast<uint16_t>(index - lagFrames - 1U));
    const uint32_t delayed =
      (static_cast<uint32_t>(newer) * (256U - lagFraction) +
       static_cast<uint32_t>(older) * lagFraction + 128U) >> 8U;
    matchedEnergy += current < delayed ? current : delayed;
    currentEnergy += current;
  }
  if (currentEnergy != 0) {
    periodicity = static_cast<uint16_t>(
      (matchedEnergy * 1000ULL) / currentEnergy);
    if (periodicity > 1000U) periodicity = 1000U;
  }

  if (onsetCount_ < 2U) return static_cast<uint16_t>(periodicity * 7U / 10U);
  uint32_t primaryWeight = 0;
  uint32_t allWeight = 0;
  const uint32_t onsetCutoff = lastOnsetAtMs_ > kScoringWindowMs
    ? lastOnsetAtMs_ - kScoringWindowMs
    : 0U;
  uint32_t intervalCount = 0;
  for (uint8_t index = 1; index < onsetCount_; ++index) {
    if (onsetTimesMs_[index - 1U] < onsetCutoff) continue;
    const uint32_t intervalMs = onsetTimesMs_[index] - onsetTimesMs_[index - 1U];
    const uint32_t multiple = (intervalMs + periodMs / 2U) / periodMs;
    if (multiple == 0 || multiple > 8U) continue;
    const uint32_t expectedMs = periodMs * multiple;
    const uint32_t toleranceMs = expectedMs / 12U + 20U;
    const uint32_t errorMs = difference(intervalMs, expectedMs);
    if (errorMs > toleranceMs) continue;
    // A candidate that is just inside the tolerance must not score the same
    // as one whose period fits the observed onset interval exactly.
    const uint32_t fitWeight = (toleranceMs - errorMs) * 100U / toleranceMs;
    if (multiple == 1U) primaryWeight += fitWeight;
    allWeight += fitWeight / multiple;
    ++intervalCount;
  }
  if (intervalCount == 0) return static_cast<uint16_t>(periodicity * 7U / 10U);
  const uint32_t primaryCoverage = primaryWeight * 10UL / intervalCount;
  const uint32_t allCoverage = allWeight * 10UL / intervalCount;
  directSupport = static_cast<uint16_t>(
    primaryCoverage * 7UL / 10UL + allCoverage * 3UL / 10UL);
  if (directSupport > 1000U) directSupport = 1000U;

  return static_cast<uint16_t>(periodicity * 7U / 10U + directSupport * 3U / 10U);
}

void BpmTracker::evaluate(uint32_t timestampMs) {
  if (lastEvaluationAtMs_ != 0 && timestampMs - lastEvaluationAtMs_ < kEvaluationIntervalMs) {
    return;
  }
  lastEvaluationAtMs_ = timestampMs;
  estimate_.analyzedFrames++;
  estimate_.onsetCount = onsetCount_;

  if (historyCount_ < kMinimumHistoryFrames || onsetCount_ < kMinimumOnsets) return;

  uint16_t bestBpm = 0;
  uint16_t bestScore = 0;
  uint16_t bestPeriodicity = 0;
  uint16_t bestDirectSupport = 0;
  uint16_t runnerUpDistantScore = 0;
  for (uint16_t candidate = kMinimumBpm; candidate <= kMaximumBpm; ++candidate) {
    uint16_t periodicity = 0;
    uint16_t directSupport = 0;
    const uint16_t score = scoreCandidate(candidate, periodicity, directSupport);
    if (score > bestScore ||
        (score == bestScore && directSupport > bestDirectSupport)) {
      if (bestBpm != 0 && difference(bestBpm, candidate) > kTempoToleranceBpm &&
          bestScore > runnerUpDistantScore) {
        runnerUpDistantScore = bestScore;
      }
      bestBpm = candidate;
      bestScore = score;
      bestPeriodicity = periodicity;
      bestDirectSupport = directSupport;
    } else if (bestBpm != 0 && difference(bestBpm, candidate) > kTempoToleranceBpm &&
               score > runnerUpDistantScore) {
      runnerUpDistantScore = score;
    }
  }
  if (bestBpm == 0) return;

  const uint16_t baseConfidence = bestScore / 10U;
  const uint16_t separation = bestScore == 0 ? 0 : static_cast<uint16_t>(
    (static_cast<uint32_t>(bestScore - runnerUpDistantScore) * 100U) / bestScore);
  const uint16_t rawConfidence = static_cast<uint16_t>(
    (baseConfidence * 3U + separation) / 4U);
  estimate_.rawBpm = bestBpm;
  estimate_.rawConfidence = rawConfidence > 100U ? 100U : static_cast<uint8_t>(rawConfidence);
  estimate_.periodicityPermille = bestPeriodicity;
  estimate_.directSupportPermille = bestDirectSupport;

  if (estimate_.rawConfidence < kMinimumLockConfidence) {
    estimate_.bpm = 0;
    estimate_.confidence = 0;
    return;
  }
  // This component estimates the current audio tempo only. It deliberately
  // has no fade, silence or song-change state machine: transitions are not a
  // reproducible input to validate here.
  estimate_.bpm = bestBpm;
  estimate_.confidence = estimate_.rawConfidence;
}

void BpmTracker::feed(uint32_t timestampMs, uint32_t level) {
  if (previousFrameAtMs_ != 0) {
    const uint32_t intervalMs = timestampMs - previousFrameAtMs_;
    if (intervalMs >= 8U && intervalMs <= 32U) {
      // VoiceBaseInput timestamps each PCM block from its sample count.  The
      // current interval is therefore more faithful than an integer moving
      // average, which can get stuck one millisecond low after a short gap.
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
