#include "v2_tempo_tracker.h"

#include <algorithm>
#include <cmath>

namespace decaflash::mainframe {
namespace {

constexpr float kDcAlpha = 0.015466f;       // High-pass close to 40 Hz at 16 kHz.
constexpr float kBassLowPassAlpha = 0.066018f;  // Two poles close to 180 Hz.
// Four one-pole cutoffs at about 200, 800, 2500 and 6000 Hz.  Their
// differences form a cheap, causal filter bank for spectral flux without an
// FFT or another dependency.
constexpr float kBandLowPassAlphas[] = {0.075535f, 0.269597f, 0.625000f, 0.905219f};
constexpr float kNoiseFloor = 40.0f;
constexpr float kOnsetMeanAlpha = 0.025f;
constexpr float kOnsetDeviationAlpha = 0.04f;
constexpr float kOnsetThresholdDeviations = 2.2f;
constexpr uint32_t kOnsetRefractoryMs = 96;
constexpr uint32_t kTempoUpdateIntervalMs = 250;
// A slower hypothesis remains available to resolve half-time.
constexpr uint16_t kMinimumTempoBpm = 70;
constexpr uint16_t kMaximumTempoBpm = 190;
constexpr uint16_t kMinimumOutputTempoBpm = 80;
constexpr uint16_t kMaximumOutputTempoBpm = 180;
constexpr uint16_t kMinimumHistoryFrames = 384;  // 6.14 s at the 16 ms hop.
constexpr uint8_t kCorrelationDecimation = 2;
constexpr uint16_t kCoarseTempoStepBpm = 2;
constexpr uint8_t kOnsetTempoTolerancePercent = 8;
constexpr uint8_t kMaximumOnsetTempoMultiple = 8;
constexpr uint16_t kMinimumHarmonicOnsetSupportGain = 3;
// Diagnostics only: roughly 0.5 s and 8 s RMS envelopes expose a real
// fade-out/fade-in transition without treating momentary quiet passages as
// a track boundary.
constexpr float kFastRmsAlpha = 0.0315f;
constexpr float kSlowRmsAlpha = 0.0020f;

float clampUnit(float value) {
  return std::max(0.0f, std::min(1.0f, value));
}

}  // namespace

void V2TempoTracker::reset() {
  *this = V2TempoTracker{};
}

void V2TempoTracker::feed(uint32_t timestampMs, const int16_t* samples, size_t count) {
  if (samples == nullptr || count == 0) return;

  // A capture discontinuity invalidates the onset history.  It is important not
  // to turn foreground-loop stalls or an audio restart into apparent rhythm.
  if (previousTimestampMs_ != 0 && timestampMs - previousTimestampMs_ > 40U) {
    reset();
    previousTimestampMs_ = timestampMs;
  }
  previousTimestampMs_ = timestampMs;

  float widePower = 0;
  float bassPower = 0;
  float bandPowers[4] = {};
  for (size_t i = 0; i < count; ++i) {
    const float raw = samples[i];
    dcEstimate_ += kDcAlpha * (raw - dcEstimate_);
    const float highPassed = raw - dcEstimate_;
    bassLow1_ += kBassLowPassAlpha * (highPassed - bassLow1_);
    bassLow2_ += kBassLowPassAlpha * (bassLow1_ - bassLow2_);
    widePower += highPassed * highPassed;
    bassPower += bassLow2_ * bassLow2_;
    float previousBand = 0;
    for (size_t band = 0; band < 4; ++band) {
      bandLow_[band] += kBandLowPassAlphas[band] * (highPassed - bandLow_[band]);
      const float bandSample = bandLow_[band] - previousBand;
      bandPowers[band] += bandSample * bandSample;
      previousBand = bandLow_[band];
    }
  }

  const float wideRms = std::sqrt(widePower / static_cast<float>(count));
  const float bassRms = std::sqrt(bassPower / static_cast<float>(count));
  if (fastRms_ == 0.0f) {
    fastRms_ = wideRms;
    slowRms_ = wideRms;
  } else {
    fastRms_ += kFastRmsAlpha * (wideRms - fastRms_);
    slowRms_ += kSlowRmsAlpha * (wideRms - slowRms_);
  }
  estimate_.fastRms = static_cast<uint16_t>(std::min(65535.0f, fastRms_));
  estimate_.slowRms = static_cast<uint16_t>(std::min(65535.0f, slowRms_));
  const float relativeTrend = slowRms_ > 1.0f ?
    (fastRms_ - slowRms_) * 1000.0f / slowRms_ : 0.0f;
  estimate_.levelTrendPermille = static_cast<int16_t>(std::lround(
    std::max(-1000.0f, std::min(1000.0f, relativeTrend))));
  const float wideRise = std::max(0.0f, wideRms - previousWideRms_);
  const float bassRise = std::max(0.0f, bassRms - previousBassRms_);
  previousWideRms_ = wideRms;
  previousBassRms_ = bassRms;

  // Use the stronger of a broadband transient and a bass transient.  Both are
  // normalised against their current level, so source volume does not define
  // the beat threshold.
  const float wideStrength = wideRise / std::max(kNoiseFloor, wideRms);
  const float bassStrength = bassRise / std::max(kNoiseFloor, bassRms);
  float spectralFlux = 0;
  for (size_t band = 0; band < 4; ++band) {
    const float bandRms = std::sqrt(bandPowers[band] / static_cast<float>(count));
    // Log-energy makes the flux insensitive to source volume while retaining
    // changes in the snare/high-frequency bands that a bass-only detector
    // misses.  The value is in roughly the same range as the existing rises.
    spectralFlux += std::max(0.0f, std::log1pf(bandRms) - std::log1pf(previousBandRms_[band]));
    previousBandRms_[band] = bandRms;
  }
  spectralFlux *= 0.25f;
  recordOnsetStrength(timestampMs, std::max({wideStrength, bassStrength, spectralFlux}));

  if (historyCount_ >= kMinimumHistoryFrames &&
      (lastTempoUpdateAtMs_ == 0 || timestampMs - lastTempoUpdateAtMs_ >= kTempoUpdateIntervalMs)) {
    lastTempoUpdateAtMs_ = timestampMs;
    updateTempo();
  }
}

void V2TempoTracker::recordOnsetStrength(uint32_t timestampMs, float strength) {
  onsetHistory_[historyIndex_] = strength;
  historyIndex_ = static_cast<uint16_t>((historyIndex_ + 1U) % kHistorySize);
  if (historyCount_ < kHistorySize) ++historyCount_;
  ++estimate_.analysisFrames;

  const float threshold = onsetMean_ + kOnsetThresholdDeviations * onsetDeviation_;
  const bool localPeak = previousOnset_ > previousPreviousOnset_ && previousOnset_ >= strength;
  const bool outsideRefractory = estimate_.lastOnsetAtMs == 0 ||
    previousOnsetAtMs_ - estimate_.lastOnsetAtMs >= kOnsetRefractoryMs;
  if (localPeak && previousOnset_ > threshold && outsideRefractory) {
    estimate_.lastOnsetAtMs = previousOnsetAtMs_;
    onsetTimestampsMs_[onsetTimestampIndex_] = previousOnsetAtMs_;
    onsetTimestampIndex_ = static_cast<uint8_t>((onsetTimestampIndex_ + 1U) % 16U);
    if (onsetTimestampCount_ < 16U) ++onsetTimestampCount_;
  }

  const float delta = std::fabs(strength - onsetMean_);
  onsetMean_ += kOnsetMeanAlpha * (strength - onsetMean_);
  onsetDeviation_ += kOnsetDeviationAlpha * (delta - onsetDeviation_);
  previousPreviousOnset_ = previousOnset_;
  previousOnset_ = strength;
  previousOnsetAtMs_ = timestampMs;
  estimate_.onsetStrengthPermille = static_cast<uint16_t>(
    std::min(1000.0f, std::max(0.0f, strength * 1000.0f)));
}

float V2TempoTracker::historyAt(uint16_t age) const {
  const uint16_t newest = static_cast<uint16_t>((historyIndex_ + kHistorySize - 1U) % kHistorySize);
  const uint16_t index = static_cast<uint16_t>((newest + kHistorySize - age) % kHistorySize);
  return onsetHistory_[index];
}

bool V2TempoTracker::directOnsetTempo(uint16_t& bpm) const {
  bpm = 0;
  if (onsetTimestampCount_ < 5U) return false;

  const uint8_t intervals = static_cast<uint8_t>(std::min<uint8_t>(onsetTimestampCount_ - 1U, 8U));
  uint32_t intervalSum = 0;
  for (uint8_t age = 0; age < intervals; ++age) {
    const uint8_t newerIndex = static_cast<uint8_t>((onsetTimestampIndex_ + 15U - age) % 16U);
    const uint8_t olderIndex = static_cast<uint8_t>((onsetTimestampIndex_ + 14U - age) % 16U);
    const uint32_t intervalMs = onsetTimestampsMs_[newerIndex] - onsetTimestampsMs_[olderIndex];
    if (intervalMs < 300U || intervalMs > 900U) return false;
    intervalSum += intervalMs;
  }
  const uint32_t averageMs = intervalSum / intervals;
  for (uint8_t age = 0; age < intervals; ++age) {
    const uint8_t newerIndex = static_cast<uint8_t>((onsetTimestampIndex_ + 15U - age) % 16U);
    const uint8_t olderIndex = static_cast<uint8_t>((onsetTimestampIndex_ + 14U - age) % 16U);
    const uint32_t intervalMs = onsetTimestampsMs_[newerIndex] - onsetTimestampsMs_[olderIndex];
    const uint32_t difference = intervalMs > averageMs ? intervalMs - averageMs : averageMs - intervalMs;
    if (difference * 100U > averageMs * 8U) return false;
  }
  const uint32_t directBpm = (60000UL + averageMs / 2U) / averageMs;
  if (directBpm < kMinimumTempoBpm || directBpm > kMaximumTempoBpm) return false;
  bpm = static_cast<uint16_t>(directBpm);
  return true;
}

uint16_t V2TempoTracker::onsetTempoSupport(uint16_t bpm) const {
  if (onsetTimestampCount_ < 3U) return 0;
  uint16_t score = 0;
  for (uint8_t newer = 1; newer < onsetTimestampCount_; ++newer) {
    for (uint8_t older = 0; older < newer; ++older) {
      const uint32_t intervalMs = onsetTimestampsMs_[newer] - onsetTimestampsMs_[older];
      if (intervalMs == 0 || intervalMs > 4000U) continue;
      const uint32_t multiple = (intervalMs * bpm + 30000U) / 60000U;
      if (multiple == 0 || multiple > kMaximumOnsetTempoMultiple) continue;
      const uint32_t expectedMs = (60000U * multiple + bpm / 2U) / bpm;
      const uint32_t difference = intervalMs > expectedMs ? intervalMs - expectedMs : expectedMs - intervalMs;
      const uint32_t toleranceMs = std::max<uint32_t>(28U,
        (expectedMs * kOnsetTempoTolerancePercent) / 100U);
      if (difference > toleranceMs) continue;
      // Direct inter-beat matches have more information about the musical
      // tempo than an interval spanning several beats.  This distinguishes
      // 81 from 162 when the intervening beats are actually present.
      score = static_cast<uint16_t>(std::min<uint32_t>(UINT16_MAX,
        score + (kMaximumOnsetTempoMultiple + 1U - multiple)));
    }
  }
  return score;
}

void V2TempoTracker::updateTempo() {
  ++estimate_.tempoEvaluations;
  // Estimate periodicity from the whole onset-strength history.  The onset
  // envelope is intentionally decimated from 16 to 32 ms here: this still
  // resolves 70–190 BPM well, while moving the time-critical correlation out
  // of the ~30 ms range measured on the AtomS3R.
  const uint16_t frameCount = static_cast<uint16_t>(
    (historyCount_ + kCorrelationDecimation - 1U) / kCorrelationDecimation);
  float mean = 0;
  for (uint16_t age = 0; age < frameCount; ++age) {
    correlationHistory_[age] = historyAt(static_cast<uint16_t>(age * kCorrelationDecimation));
    mean += correlationHistory_[age];
  }
  mean /= static_cast<float>(frameCount);
  for (uint16_t age = 0; age < frameCount; ++age) correlationHistory_[age] -= mean;

  auto scoreTempo = [&](uint16_t bpm) {
    // 60 seconds / 32 ms = 1875 correlation frames per minute.
    const float lagFrames = 1875.0f / static_cast<float>(bpm);
    const uint16_t lagBase = static_cast<uint16_t>(lagFrames);
    const float fraction = lagFrames - static_cast<float>(lagBase);
    if (lagBase + 1U >= frameCount) return -1.0f;

    float numerator = 0;
    float currentEnergy = 0;
    float laggedEnergy = 0;
    const uint16_t comparisons = static_cast<uint16_t>(frameCount - lagBase - 1U);
    for (uint16_t age = 0; age < comparisons; ++age) {
      const float current = correlationHistory_[age];
      const float older = correlationHistory_[age + lagBase];
      const float stillOlder = correlationHistory_[age + lagBase + 1U];
      const float lagged = older + fraction * (stillOlder - older);
      numerator += current * lagged;
      currentEnergy += current * current;
      laggedEnergy += lagged * lagged;
    }
    if (numerator <= 0 || currentEnergy <= 0 || laggedEnergy <= 0) return -1.0f;
    return numerator / std::sqrt(currentEnergy * laggedEnergy);
  };

  float bestScore = -1;
  uint16_t bestBpm = 0;
  // Coarse scan followed by a five-BPM refinement keeps a full search but
  // avoids evaluating essentially identical neighbouring tempo candidates.
  constexpr uint16_t kCoarseCandidateCount =
    (kMaximumTempoBpm - kMinimumTempoBpm) / kCoarseTempoStepBpm + 1U;
  float coarseScores[kCoarseCandidateCount] = {};
  for (uint16_t candidate = 0; candidate < kCoarseCandidateCount; ++candidate) {
    const uint16_t bpm = static_cast<uint16_t>(
      kMinimumTempoBpm + candidate * kCoarseTempoStepBpm);
    const float score = scoreTempo(bpm);
    coarseScores[candidate] = score;
    if (score > bestScore) {
      bestScore = score;
      bestBpm = bpm;
    }
  }

  if (bestBpm != 0) {
    const uint16_t lower = bestBpm > kMinimumTempoBpm + 2U ? bestBpm - 2U : kMinimumTempoBpm;
    const uint16_t upper = std::min<uint16_t>(kMaximumTempoBpm, bestBpm + 2U);
    for (uint16_t bpm = lower; bpm <= upper; ++bpm) {
      const float score = scoreTempo(bpm);
      if (score > bestScore) {
        bestScore = score;
        bestBpm = bpm;
      }
    }
  }

  if (bestBpm == 0 || bestScore <= 0) {
    estimate_.bpm = 0;
    estimate_.confidence = 0;
    estimate_.harmonicBpm = 0;
    estimate_.periodicityPermille = 0;
    estimate_.harmonicPeriodicityPermille = 0;
    estimate_.onsetSupport = 0;
    estimate_.harmonicOnsetSupport = 0;
    return;
  }

  const uint16_t correlationBpm = bestBpm;
  const float harmonicScore = correlationBpm <= kMaximumTempoBpm / 2U
    ? scoreTempo(static_cast<uint16_t>(correlationBpm * 2U)) : -1.0f;
  estimate_.harmonicBpm = harmonicScore > 0
    ? static_cast<uint16_t>(correlationBpm * 2U) : 0;
  estimate_.periodicityPermille = static_cast<uint16_t>(
    std::lround(clampUnit(bestScore) * 1000.0f));
  estimate_.harmonicPeriodicityPermille = static_cast<uint16_t>(
    std::lround(clampUnit(harmonicScore) * 1000.0f));
  estimate_.onsetSupport = onsetTempoSupport(correlationBpm);
  estimate_.harmonicOnsetSupport = estimate_.harmonicBpm == 0 ? 0 :
    onsetTempoSupport(estimate_.harmonicBpm);
  const bool harmonicHasMoreOnsetEvidence = estimate_.harmonicBpm != 0 &&
    estimate_.harmonicOnsetSupport >= estimate_.onsetSupport +
      kMinimumHarmonicOnsetSupportGain;
  const bool harmonicHasUsablePeriodicity = harmonicScore > 0 &&
    harmonicScore * 100.0f >= bestScore * 35.0f;
  if (harmonicHasMoreOnsetEvidence && harmonicHasUsablePeriodicity) {
    bestBpm = estimate_.harmonicBpm;
    bestScore = harmonicScore;
  }

  uint16_t directBpm = 0;
  bool directAgreement = false;
  if (directOnsetTempo(directBpm) && directBpm >= kMinimumOutputTempoBpm &&
      directBpm <= kMaximumOutputTempoBpm) {
    const bool directIsNearby = std::abs(static_cast<int>(directBpm) -
                                         static_cast<int>(bestBpm)) <= 4;
    const bool directIsDouble = std::abs(static_cast<int>(directBpm) -
                                         static_cast<int>(bestBpm) * 2) <= 4;
    const bool directIsHalf = std::abs(static_cast<int>(bestBpm) -
                                       static_cast<int>(directBpm) * 2) <= 4;
    if (directIsNearby || directIsDouble || directIsHalf) {
      bestBpm = directBpm;
      directAgreement = true;
    }
  }
  estimate_.directBpm = directBpm;

  // A high periodicity alone is insufficient: octave-related candidates are
  // expected.  Neighbouring BPM values are the same broad peak, so separation
  // only compares competing peaks at least four BPM away.
  float secondScore = -1;
  for (uint16_t candidate = 0; candidate < kCoarseCandidateCount; ++candidate) {
    const uint16_t bpm = static_cast<uint16_t>(
      kMinimumTempoBpm + candidate * kCoarseTempoStepBpm);
    if (std::abs(static_cast<int>(bpm) - static_cast<int>(bestBpm)) < 4) continue;
    secondScore = std::max(secondScore, coarseScores[candidate]);
  }
  const float periodicity = clampUnit(bestScore);
  const float separation = secondScore <= 0 ? periodicity :
    clampUnit((bestScore - secondScore) / std::max(0.05f, bestScore));
  float confidence = clampUnit(0.72f * periodicity + 0.28f * separation);
  // Eight recent intervals agreeing within 8% are independent evidence of a
  // steady pulse.  It may raise confidence only when it also agrees with the
  // full-history correlation (or resolves its exact octave).
  if (directAgreement) confidence = std::max(confidence, 0.55f);
  estimate_.rawBpm = bestBpm;
  estimate_.rawConfidence = static_cast<uint8_t>(std::lround(confidence * 100.0f));
  // V2 exposes the current measurement immediately.  In particular, it has
  // no song-change or octave lock state that can keep a stale tempo alive.
  estimate_.bpm = estimate_.rawBpm;
  estimate_.confidence = estimate_.rawConfidence;
}

}  // namespace decaflash::mainframe
