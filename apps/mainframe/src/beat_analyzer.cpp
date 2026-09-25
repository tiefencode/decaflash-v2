#include "beat_analyzer.h"

#include <cstdlib>

namespace decaflash::mainframe {
namespace {

static constexpr uint32_t kAnalysisMusicOnMargin = 22;
static constexpr uint32_t kAnalysisMusicOffMargin = 12;
static constexpr uint32_t kAnalysisOnsetMinimum = 18;
static constexpr uint32_t kAnalysisOnsetDivisor = 3;
static constexpr uint32_t kAnalysisPeakRatioLimit = 18;
static constexpr uint8_t kAnalysisLockedConfidence = 50;
static constexpr uint8_t kAnalysisEarlyIntervalPercent = 88;
static constexpr uint32_t kAnalysisOnsetCooldownMs = 300;
static constexpr uint32_t kAnalysisMinIntervalMs = 300;
static constexpr uint32_t kAnalysisMaxIntervalMs = 1200;
static constexpr uint32_t kAnalysisTempoHoldMs = 2200;
static constexpr uint16_t kAnalysisMinTempoBpm = 80;
static constexpr uint16_t kAnalysisMaxTempoBpm = 180;
static constexpr uint8_t kAnalysisHistorySize = 8;
static constexpr uint8_t kAnalysisTempoBucketCount =
  static_cast<uint8_t>(kAnalysisMaxTempoBpm - kAnalysisMinTempoBpm + 1U);
static constexpr uint8_t kAnalysisTempoMaxMultiple = 8;
static constexpr uint8_t kAnalysisTempoTolerancePercent = 9;
static constexpr uint8_t kAnalysisTempoContinuityBonus = 18;
static constexpr uint8_t kAnalysisTempoCoverageTarget = 8;
static constexpr uint8_t kAnalysisTempoBucketDecayShift = 3;
static constexpr uint8_t kAnalysisTempoNeighborWeightPercent = 30;
static constexpr uint8_t kAnalysisTempoDirectTolerancePercent = 8;
static constexpr uint8_t kAnalysisTempoHoldConfidence = 60;
static constexpr uint8_t kAnalysisTempoHoldPercent = 112;
static constexpr uint8_t kAnalysisTempoFamilyToleranceBpm = 4;
static constexpr uint16_t kAnalysisTempoFamilyPreferredMinBpm = 90;
static constexpr uint16_t kAnalysisTempoFamilyPreferredMaxBpm = 170;
static constexpr uint16_t kAnalysisTempoFamilyPreferredCenterBpm = 130;
static constexpr uint8_t kAnalysisTempoRepresentativePairwiseWeight = 2;
static constexpr uint8_t kAnalysisTempoRepresentativeMemoryDivisor = 4;
static constexpr uint8_t kAnalysisTempoRepresentativeFamilyPenalty = 5;
static constexpr uint8_t kAnalysisTempoRepresentativeFamilyBonus = 10;
static constexpr uint8_t kAnalysisTempoPrimaryScoreWeight = 6;
static constexpr uint8_t kAnalysisTempoPrimaryCoverageTarget = 4;
static constexpr uint8_t kAnalysisTempoSameFamilySwitchPercent = 125;
static constexpr uint8_t kAnalysisClockSubdivisionSwitchHits = 4;
static constexpr uint8_t kAnalysisHalfIntervalMinPercent = 45;
static constexpr uint8_t kAnalysisHalfIntervalMaxPercent = 65;
static constexpr uint8_t kAnalysisPulseHistorySize = 64;
static constexpr uint8_t kAnalysisPulseScaleShift = 2;
static constexpr uint8_t kAnalysisPulseScoreWeight = 6;
static constexpr uint16_t kPeriodicityMinBpm = 90;
static constexpr uint16_t kPeriodicityMaxBpm = 160;
static constexpr uint16_t kPeriodicityHistorySize = 256;
static constexpr uint16_t kPeriodicityMinimumFrames = 128;
static constexpr uint8_t kPeriodicityMinimumConfidence = 68;
uint16_t bpmDifference(uint16_t left, uint16_t right) {
  return (left > right) ? (left - right) : (right - left);
}

uint16_t distanceToTempoWindow(uint16_t bpm) {
  if (bpm < kAnalysisTempoFamilyPreferredMinBpm) {
    return kAnalysisTempoFamilyPreferredMinBpm - bpm;
  }
  if (bpm > kAnalysisTempoFamilyPreferredMaxBpm) {
    return bpm - kAnalysisTempoFamilyPreferredMaxBpm;
  }
  return 0;
}

uint16_t canonicalTempoFamilyBpm(uint16_t bpm) {
  uint16_t bestBpm = bpm;
  uint16_t bestWindowDistance = distanceToTempoWindow(bestBpm);
  uint16_t bestCenterDistance = bpmDifference(bestBpm, kAnalysisTempoFamilyPreferredCenterBpm);

  const uint32_t candidateValues[3] = {
    static_cast<uint32_t>(bpm),
    static_cast<uint32_t>(bpm / 2U),
    static_cast<uint32_t>(static_cast<uint32_t>(bpm) * 2UL),
  };

  for (uint32_t candidateValue : candidateValues) {
    if (candidateValue < kAnalysisMinTempoBpm || candidateValue > kAnalysisMaxTempoBpm) {
      continue;
    }

    const uint16_t candidateBpm = static_cast<uint16_t>(candidateValue);
    const uint16_t candidateWindowDistance = distanceToTempoWindow(candidateBpm);
    const uint16_t candidateCenterDistance =
      bpmDifference(candidateBpm, kAnalysisTempoFamilyPreferredCenterBpm);

    if (candidateWindowDistance < bestWindowDistance ||
        (candidateWindowDistance == bestWindowDistance &&
         candidateCenterDistance < bestCenterDistance)) {
      bestBpm = candidateBpm;
      bestWindowDistance = candidateWindowDistance;
      bestCenterDistance = candidateCenterDistance;
    }
  }

  return bestBpm;
}

bool inSameTempoFamily(uint16_t left, uint16_t right) {
  if (left == 0 || right == 0) {
    return false;
  }

  const uint16_t leftCanonical = canonicalTempoFamilyBpm(left);
  const uint16_t rightCanonical = canonicalTempoFamilyBpm(right);
  if (leftCanonical == rightCanonical) {
    return true;
  }

  const uint16_t lower = (leftCanonical < rightCanonical) ? leftCanonical : rightCanonical;
  const uint16_t higher = (leftCanonical < rightCanonical) ? rightCanonical : leftCanonical;
  return static_cast<uint16_t>(abs(
           static_cast<int32_t>(lower * 2U) - static_cast<int32_t>(higher)
         )) <= kAnalysisTempoFamilyToleranceBpm;
}

}  // namespace


void BeatAnalyzer::recordPulseFrame(uint32_t now, uint32_t transientLevel, uint32_t onsetThreshold) {
  if (lastAnalysisFrameAtMs_ != 0) {
    const uint32_t frameIntervalMs = now - lastAnalysisFrameAtMs_;
    if (frameIntervalMs >= 20UL && frameIntervalMs <= 120UL) {
      analysisFrameIntervalMs_ = static_cast<uint16_t>(
        ((analysisFrameIntervalMs_ * 7UL) + frameIntervalMs) / 8UL
      );
    }
  }
  lastAnalysisFrameAtMs_ = now;

  uint32_t pulseStrength =
    (transientLevel > onsetThreshold) ? (transientLevel - onsetThreshold) : 0UL;
  pulseStrength >>= kAnalysisPulseScaleShift;
  if (pulseStrength > 255UL) {
    pulseStrength = 255UL;
  }

  pulseHistory_[pulseHistoryIndex_] = static_cast<uint8_t>(pulseStrength);
  pulseHistoryIndex_ = static_cast<uint8_t>((pulseHistoryIndex_ + 1U) % kAnalysisPulseHistorySize);
  if (pulseHistoryCount_ < kAnalysisPulseHistorySize) {
    pulseHistoryCount_++;
  }
}

void BeatAnalyzer::recordEnergyFrame(uint32_t blockLevel) {
  const uint16_t level = static_cast<uint16_t>(
    blockLevel > UINT16_MAX ? UINT16_MAX : blockLevel);
  const uint16_t flux = level > previousEnergyLevel_ ? level - previousEnergyLevel_ : 0;
  previousEnergyLevel_ = level;
  fluxHistory_[energyHistoryIndex_] = flux;
  energyHistoryIndex_ = static_cast<uint16_t>(
    (energyHistoryIndex_ + 1U) % kPeriodicityHistorySize);
  if (energyHistoryCount_ < kPeriodicityHistorySize) ++energyHistoryCount_;
}

void BeatAnalyzer::feed(uint32_t now, uint32_t blockLevel, uint16_t peakLevel) {
  recordEnergyFrame(blockLevel);
  if (analysisFastLevel_ == 0) {
    analysisFastLevel_ = blockLevel;
  } else if (blockLevel > analysisFastLevel_) {
    analysisFastLevel_ = ((analysisFastLevel_ * 2UL) + blockLevel) / 3UL;
  } else {
    analysisFastLevel_ = ((analysisFastLevel_ * 5UL) + blockLevel) / 6UL;
  }

  if (analysisSlowLevel_ == 0) {
    analysisSlowLevel_ = blockLevel;
  } else if (blockLevel > analysisSlowLevel_) {
    analysisSlowLevel_ = ((analysisSlowLevel_ * 15UL) + blockLevel) / 16UL;
  } else {
    analysisSlowLevel_ = ((analysisSlowLevel_ * 31UL) + blockLevel) / 32UL;
  }

  if (analysisFloor_ == 0) {
    analysisFloor_ = analysisSlowLevel_;
  } else if (analysisSlowLevel_ < analysisFloor_) {
    analysisFloor_ = ((analysisFloor_ * 7UL) + analysisSlowLevel_) / 8UL;
  } else {
    analysisFloor_ = ((analysisFloor_ * 255UL) + analysisSlowLevel_) / 256UL;
  }

  const uint32_t musicOnThreshold = analysisFloor_ + kAnalysisMusicOnMargin;
  const uint32_t musicOffThreshold = analysisFloor_ + kAnalysisMusicOffMargin;
  if (musicPresent_) {
    musicPresent_ = analysisSlowLevel_ > musicOffThreshold;
  } else {
    musicPresent_ = analysisSlowLevel_ > musicOnThreshold;
  }

  const uint32_t transientLevel =
    (analysisFastLevel_ > analysisSlowLevel_) ? (analysisFastLevel_ - analysisSlowLevel_) : 0;
  onsetStrength_ = transientLevel;

  if ((now - lastOnsetAtMs_) > kAnalysisTempoHoldMs) {
    if (!musicPresent_) {
      detectedBpm_ = 0;
      clockBpm_ = 0;
      clockSubdivisionCandidateBpm_ = 0;
      clockSubdivisionCandidateCount_ = 0;
      onsetTimestampCount_ = 0;
      onsetIntervalCount_ = 0;
      pulseHistoryCount_ = 0;
      pulseHistoryIndex_ = 0;
      previousEnergyLevel_ = 0;
      energyHistoryCount_ = 0;
      energyHistoryIndex_ = 0;
      lastAnalysisFrameAtMs_ = 0;
      for (uint32_t& bucketScore : tempoBucketScores_) {
        bucketScore = 0;
      }
    }
    confidence_ = 0;
  }

  const uint32_t energySpan =
    (analysisSlowLevel_ > analysisFloor_) ? (analysisSlowLevel_ - analysisFloor_) : 0;
  const uint32_t onsetThreshold = kAnalysisOnsetMinimum + (energySpan / kAnalysisOnsetDivisor);
  recordPulseFrame(now, transientLevel, musicPresent_ ? onsetThreshold : UINT32_MAX);

  if (!musicPresent_) {
    return;
  }

  const bool cooldownElapsed =
    (lastOnsetAtMs_ == 0) || ((now - lastOnsetAtMs_) >= kAnalysisOnsetCooldownMs);
  const bool peakLooksMusical =
    (blockLevel == 0) || (peakLevel <= (blockLevel * kAnalysisPeakRatioLimit));

  if (!cooldownElapsed || !peakLooksMusical || transientLevel <= onsetThreshold) {
    return;
  }

  uint32_t intervalMs = (lastOnsetAtMs_ == 0) ? 0 : (now - lastOnsetAtMs_);
  if (intervalMs != 0 && intervalMs < kAnalysisMinIntervalMs) {
    return;
  }

  if (intervalMs != 0 && detectedBpm_ != 0 && confidence_ >= kAnalysisLockedConfidence) {
    const uint32_t expectedIntervalMs = 60000UL / detectedBpm_;
    const uint32_t earlyIntervalMs =
      (expectedIntervalMs * kAnalysisEarlyIntervalPercent) / 100UL;
    const uint32_t halfIntervalMinMs =
      (expectedIntervalMs * kAnalysisHalfIntervalMinPercent) / 100UL;
    const uint32_t halfIntervalMaxMs =
      (expectedIntervalMs * kAnalysisHalfIntervalMaxPercent) / 100UL;
    if (intervalMs < earlyIntervalMs &&
        (intervalMs < halfIntervalMinMs || intervalMs > halfIntervalMaxMs)) {
      return;
    }
  }

  lastOnsetAtMs_ = now;
  registerOnset(now, transientLevel, intervalMs);
}

void BeatAnalyzer::registerOnset(uint32_t now, uint32_t onsetStrength, uint32_t intervalMs) {
  (void)onsetStrength;
  if (onsetTimestampCount_ < kAnalysisHistorySize) {
    onsetTimestampsMs_[onsetTimestampCount_++] = now;
  } else {
    for (uint8_t i = 1; i < kAnalysisHistorySize; ++i) {
      onsetTimestampsMs_[i - 1] = onsetTimestampsMs_[i];
    }
    onsetTimestampsMs_[kAnalysisHistorySize - 1] = now;
  }

  if (intervalMs >= kAnalysisMinIntervalMs && intervalMs <= kAnalysisMaxIntervalMs) {
    if (onsetIntervalCount_ < kAnalysisHistorySize) {
      onsetIntervalsMs_[onsetIntervalCount_++] = intervalMs;
    } else {
      for (uint8_t i = 1; i < kAnalysisHistorySize; ++i) {
        onsetIntervalsMs_[i - 1] = onsetIntervalsMs_[i];
      }
      onsetIntervalsMs_[kAnalysisHistorySize - 1] = intervalMs;
    }
    updateTempoEstimate();
  } else if (intervalMs > kAnalysisMaxIntervalMs) {
    onsetTimestampCount_ = 1;
    onsetTimestampsMs_[0] = now;
    onsetIntervalCount_ = 0;
    confidence_ = 0;
  }

}


void BeatAnalyzer::updateTempoEstimate() {
  if (onsetTimestampCount_ < 3) {
    return;
  }

  uint16_t periodicBpm = 0;
  uint8_t periodicConfidence = 0;
  if (resolvePeriodicTempo(periodicBpm, periodicConfidence) &&
      periodicConfidence >= kPeriodicityMinimumConfidence) {
    detectedBpm_ = periodicBpm;
    clockBpm_ = periodicBpm;
    confidence_ = periodicConfidence;
    clockSubdivisionCandidateBpm_ = 0;
    clockSubdivisionCandidateCount_ = 0;
    return;
  }

  const uint16_t previousDetectedBpm = detectedBpm_;
  const uint16_t previousClockBpm = clockBpm_;
  const uint16_t previousFamilyBpm =
    (previousDetectedBpm == 0) ? 0 : canonicalTempoFamilyBpm(previousDetectedBpm);
  uint32_t instantScores[kAnalysisTempoBucketCount] = {0};
  uint32_t instantErrorSums[kAnalysisTempoBucketCount] = {0};
  uint8_t instantMatchCounts[kAnalysisTempoBucketCount] = {0};
  uint32_t primaryScores[kAnalysisTempoBucketCount] = {0};
  uint8_t primaryMatchCounts[kAnalysisTempoBucketCount] = {0};
  uint32_t exactIntervalScores[kAnalysisTempoBucketCount] = {0};
  uint32_t pulseScores[kAnalysisTempoBucketCount] = {0};
  uint32_t combinedScores[kAnalysisTempoBucketCount] = {0};
  uint32_t familyScores[kAnalysisTempoBucketCount] = {0};
  uint32_t directFamilyScores[kAnalysisTempoBucketCount] = {0};
  uint8_t pulseFrames[kAnalysisPulseHistorySize] = {0};

  if (pulseHistoryCount_ > 0) {
    for (uint8_t i = 0; i < pulseHistoryCount_; ++i) {
      const uint8_t historyIndex = static_cast<uint8_t>(
        (pulseHistoryIndex_ + kAnalysisPulseHistorySize - pulseHistoryCount_ + i) %
        kAnalysisPulseHistorySize
      );
      pulseFrames[i] = pulseHistory_[historyIndex];
    }
  }

  for (uint16_t bpm = kAnalysisMinTempoBpm; bpm <= kAnalysisMaxTempoBpm; ++bpm) {
    uint32_t score = 0;
    uint32_t errorSum = 0;
    uint8_t matchCount = 0;
    uint32_t primaryScore = 0;
    uint8_t primaryMatchCount = 0;

    for (uint8_t newer = 1; newer < onsetTimestampCount_; ++newer) {
      for (uint8_t older = 0; older < newer; ++older) {
        const uint32_t diffMs = onsetTimestampsMs_[newer] - onsetTimestampsMs_[older];
        if (diffMs == 0 || diffMs > kAnalysisTempoHoldMs) {
          continue;
        }

        const uint32_t candidateMultiple =
          ((diffMs * static_cast<uint32_t>(bpm)) + 30000UL) / 60000UL;
        if (candidateMultiple == 0 || candidateMultiple > kAnalysisTempoMaxMultiple) {
          continue;
        }

        const uint32_t expectedDiffMs =
          ((60000UL * candidateMultiple) + (bpm / 2U)) / bpm;
        uint32_t toleranceMs =
          (expectedDiffMs * kAnalysisTempoTolerancePercent) / 100UL;
        if (toleranceMs < 24UL) {
          toleranceMs = 24UL;
        }

        const uint32_t errorMs = static_cast<uint32_t>(abs(
          static_cast<int32_t>(diffMs) - static_cast<int32_t>(expectedDiffMs)
        ));
        if (errorMs > toleranceMs) {
          continue;
        }

        const uint32_t recencyWeight = 1UL + newer;
        const uint32_t multipleWeight =
          (kAnalysisTempoMaxMultiple + 1UL) - candidateMultiple;
        const uint32_t matchScore =
          (recencyWeight * multipleWeight * 8UL) + (toleranceMs - errorMs);
        score += matchScore;
        errorSum += errorMs;
        matchCount++;

        if (candidateMultiple == 1UL) {
          primaryScore += matchScore * 2UL;
          if (primaryMatchCount < 0xFFU) {
            primaryMatchCount++;
          }
        } else if (candidateMultiple == 2UL) {
          primaryScore += matchScore / 4UL;
        }
      }
    }

    const uint8_t bucketIndex = static_cast<uint8_t>(bpm - kAnalysisMinTempoBpm);
    instantScores[bucketIndex] = score;
    instantErrorSums[bucketIndex] = errorSum;
    instantMatchCounts[bucketIndex] = matchCount;
    primaryScores[bucketIndex] = primaryScore;
    primaryMatchCounts[bucketIndex] = primaryMatchCount;
  }

  for (uint8_t intervalIndex = 0; intervalIndex < onsetIntervalCount_; ++intervalIndex) {
    const uint32_t intervalMs = onsetIntervalsMs_[intervalIndex];
    const uint32_t recencyWeight = 2UL + intervalIndex;

    for (uint16_t bpm = kAnalysisMinTempoBpm; bpm <= kAnalysisMaxTempoBpm; ++bpm) {
      const uint8_t bucketIndex = static_cast<uint8_t>(bpm - kAnalysisMinTempoBpm);
      const uint32_t expectedIntervalMs = 60000UL / bpm;
      uint32_t toleranceMs =
        (expectedIntervalMs * kAnalysisTempoDirectTolerancePercent) / 100UL;
      if (toleranceMs < 24UL) {
        toleranceMs = 24UL;
      }

      const uint32_t errorMs = static_cast<uint32_t>(abs(
        static_cast<int32_t>(intervalMs) - static_cast<int32_t>(expectedIntervalMs)
      ));
      if (errorMs > toleranceMs) {
        continue;
      }

      exactIntervalScores[bucketIndex] +=
        (recencyWeight * 24UL) + ((toleranceMs - errorMs) * 2UL);
    }
  }

  if (pulseHistoryCount_ >= 8 && analysisFrameIntervalMs_ != 0) {
    for (uint16_t bpm = kAnalysisMinTempoBpm; bpm <= kAnalysisMaxTempoBpm; ++bpm) {
      const uint8_t bucketIndex = static_cast<uint8_t>(bpm - kAnalysisMinTempoBpm);
      const uint32_t expectedIntervalMs = 60000UL / bpm;
      const uint32_t lagFrames =
        (expectedIntervalMs + (analysisFrameIntervalMs_ / 2U)) / analysisFrameIntervalMs_;
      if (lagFrames < 2UL || lagFrames >= pulseHistoryCount_) {
        continue;
      }

      uint32_t pulseScore = 0;
      for (uint8_t newer = static_cast<uint8_t>(lagFrames); newer < pulseHistoryCount_; ++newer) {
        const uint8_t currentPulse = pulseFrames[newer];
        const uint8_t laggedPulse = pulseFrames[newer - static_cast<uint8_t>(lagFrames)];
        if (currentPulse == 0 || laggedPulse == 0) {
          continue;
        }
        pulseScore += (currentPulse < laggedPulse) ? currentPulse : laggedPulse;
      }

      pulseScores[bucketIndex] = pulseScore;
    }
  }

  for (uint8_t i = 0; i < kAnalysisTempoBucketCount; ++i) {
    tempoBucketScores_[i] -= tempoBucketScores_[i] >> kAnalysisTempoBucketDecayShift;
    tempoBucketScores_[i] += instantScores[i] + primaryScores[i] + (pulseScores[i] / 2UL);
  }

  for (uint8_t i = 0; i < kAnalysisTempoBucketCount; ++i) {
    if (instantMatchCounts[i] == 0 || tempoBucketScores_[i] == 0) {
      continue;
    }

    const uint16_t bpm = static_cast<uint16_t>(kAnalysisMinTempoBpm + i);
    uint32_t score = tempoBucketScores_[i] +
                     (exactIntervalScores[i] * 2UL) +
                     (primaryScores[i] * kAnalysisTempoPrimaryScoreWeight) +
                     (pulseScores[i] * kAnalysisPulseScoreWeight);
    if (i > 0) {
      score += (tempoBucketScores_[i - 1] * kAnalysisTempoNeighborWeightPercent) / 100UL;
    }
    if ((i + 1U) < kAnalysisTempoBucketCount) {
      score += (tempoBucketScores_[i + 1] * kAnalysisTempoNeighborWeightPercent) / 100UL;
    }

    if (previousDetectedBpm != 0) {
      const uint16_t bpmDelta = bpmDifference(previousDetectedBpm, bpm);
      if (bpmDelta <= 2U) {
        score += static_cast<uint32_t>((3U - bpmDelta) * kAnalysisTempoContinuityBonus * 6UL);
      } else if (confidence_ >= kAnalysisTempoHoldConfidence &&
                 inSameTempoFamily(previousDetectedBpm, bpm)) {
        score += static_cast<uint32_t>(kAnalysisTempoContinuityBonus * 4UL);
      }
    }

    combinedScores[i] = score;
    const uint16_t familyBpm = canonicalTempoFamilyBpm(bpm);
    const uint8_t familyIndex = static_cast<uint8_t>(familyBpm - kAnalysisMinTempoBpm);
    familyScores[familyIndex] += score;

    // Direct adjacent-onset evidence is the least ambiguous signal we have.
    // Keep it separate from pairwise multiples and pulse autocorrelation so a
    // high harmonic cannot turn a clear tempo into a half-tempo family.
    if (primaryMatchCounts[i] >= 2U && exactIntervalScores[i] > 0) {
      directFamilyScores[familyIndex] +=
        (primaryScores[i] * kAnalysisTempoPrimaryScoreWeight) + exactIntervalScores[i];
    }
  }

  uint16_t bestFamilyBpm = 0;
  uint32_t bestFamilyScore = 0;
  uint32_t secondBestFamilyScore = 0;

  for (uint8_t i = 0; i < kAnalysisTempoBucketCount; ++i) {
    if (familyScores[i] == 0) {
      continue;
    }

    uint32_t familyScore = familyScores[i];
    const uint16_t familyBpm = static_cast<uint16_t>(kAnalysisMinTempoBpm + i);
    if (previousFamilyBpm != 0 && familyBpm == previousFamilyBpm) {
      familyScore += static_cast<uint32_t>(kAnalysisTempoContinuityBonus * 12UL);
    }

    if (familyScore > bestFamilyScore) {
      secondBestFamilyScore = bestFamilyScore;
      bestFamilyScore = familyScore;
      bestFamilyBpm = familyBpm;
    } else if (familyScore > secondBestFamilyScore) {
      secondBestFamilyScore = familyScore;
    }
  }

  uint16_t bestDirectFamilyBpm = 0;
  uint32_t bestDirectFamilyScore = 0;
  for (uint8_t i = 0; i < kAnalysisTempoBucketCount; ++i) {
    if (directFamilyScores[i] > bestDirectFamilyScore) {
      bestDirectFamilyScore = directFamilyScores[i];
      bestDirectFamilyBpm = static_cast<uint16_t>(kAnalysisMinTempoBpm + i);
    }
  }
  if (bestDirectFamilyBpm != 0) {
    bestFamilyBpm = bestDirectFamilyBpm;
    bestFamilyScore = familyScores[static_cast<uint8_t>(bestFamilyBpm - kAnalysisMinTempoBpm)];
  }

  if (previousFamilyBpm >= kAnalysisMinTempoBpm && previousFamilyBpm <= kAnalysisMaxTempoBpm &&
      confidence_ >= kAnalysisTempoHoldConfidence) {
    const uint8_t currentFamilyIndex =
      static_cast<uint8_t>(previousFamilyBpm - kAnalysisMinTempoBpm);
    const uint32_t currentFamilyScore = familyScores[currentFamilyIndex];
    if (bestFamilyBpm == 0 && currentFamilyScore > 0) {
      bestFamilyBpm = previousFamilyBpm;
      bestFamilyScore = currentFamilyScore;
    } else if (bestFamilyBpm != 0 && currentFamilyScore > 0 &&
               bestFamilyBpm != previousFamilyBpm) {
      if ((bestFamilyScore * 100UL) < (currentFamilyScore * kAnalysisTempoHoldPercent)) {
        secondBestFamilyScore = bestFamilyScore;
        bestFamilyBpm = previousFamilyBpm;
        bestFamilyScore = currentFamilyScore;
      }
    }
  }

  if (bestFamilyBpm == 0 || bestFamilyScore == 0) {
    confidence_ = (confidence_ > 6U) ? static_cast<uint8_t>(confidence_ - 6U) : 0;
    return;
  }

  uint16_t bestBpm = 0;
  uint8_t bestBucketIndex = 0;
  uint32_t bestRepresentativeScore = 0;
  uint32_t bestErrorSum = 0;
  uint8_t bestMatchCount = 0;
  const uint8_t familyAnchorIndex =
    static_cast<uint8_t>(bestFamilyBpm - kAnalysisMinTempoBpm);
  const uint32_t familyAnchorPrimaryScore = primaryScores[familyAnchorIndex];
  const uint8_t familyAnchorPrimaryMatches = primaryMatchCounts[familyAnchorIndex];

  for (uint8_t i = 0; i < kAnalysisTempoBucketCount; ++i) {
    const uint16_t bpm = static_cast<uint16_t>(kAnalysisMinTempoBpm + i);
    if (canonicalTempoFamilyBpm(bpm) != bestFamilyBpm) {
      continue;
    }
    if (instantMatchCounts[i] == 0 || combinedScores[i] == 0) {
      continue;
    }

    uint32_t candidateScore =
      (instantScores[i] * kAnalysisTempoRepresentativePairwiseWeight) +
      (primaryScores[i] * (kAnalysisTempoPrimaryScoreWeight + 2UL)) +
      exactIntervalScores[i] +
      (pulseScores[i] * (kAnalysisPulseScoreWeight + 2UL)) +
      (tempoBucketScores_[i] / kAnalysisTempoRepresentativeMemoryDivisor);

    if (i > 0) {
      candidateScore += (instantScores[i - 1] * kAnalysisTempoNeighborWeightPercent) / 100UL;
    }
    if ((i + 1U) < kAnalysisTempoBucketCount) {
      candidateScore += (instantScores[i + 1] * kAnalysisTempoNeighborWeightPercent) / 100UL;
    }

    const uint16_t familyDistance = bpmDifference(bestFamilyBpm, bpm);
    if (familyDistance == 0) {
      candidateScore += static_cast<uint32_t>(kAnalysisTempoContinuityBonus *
                                              kAnalysisTempoRepresentativeFamilyBonus);
    } else {
      uint32_t familyPenalty = static_cast<uint32_t>(familyDistance) *
                               kAnalysisTempoContinuityBonus *
                               kAnalysisTempoRepresentativeFamilyPenalty;
      if (primaryScores[i] > familyAnchorPrimaryScore ||
          primaryMatchCounts[i] > familyAnchorPrimaryMatches) {
        familyPenalty /= 4UL;
      }
      candidateScore = (candidateScore > familyPenalty) ? (candidateScore - familyPenalty) : 0UL;
    }

    if (previousDetectedBpm != 0 && inSameTempoFamily(previousDetectedBpm, bpm)) {
      const uint16_t bpmDelta = bpmDifference(previousDetectedBpm, bpm);
      if (bpmDelta <= 2U) {
        candidateScore += static_cast<uint32_t>((3U - bpmDelta) * kAnalysisTempoContinuityBonus * 8UL);
      } else if (bpmDelta <= 6U) {
        candidateScore += static_cast<uint32_t>((7U - bpmDelta) * kAnalysisTempoContinuityBonus * 2UL);
      }
    }

    if (candidateScore > bestRepresentativeScore) {
      bestRepresentativeScore = candidateScore;
      bestBpm = bpm;
      bestBucketIndex = i;
      bestErrorSum = instantErrorSums[i];
      bestMatchCount = instantMatchCounts[i];
    }
  }

  if (bestBpm == 0 || bestMatchCount == 0 || bestRepresentativeScore == 0) {
    confidence_ = (confidence_ > 6U) ? static_cast<uint8_t>(confidence_ - 6U) : 0;
    return;
  }

  const uint32_t bestIntervalMs = 60000UL / bestBpm;
  const uint32_t averageErrorMs = bestErrorSum / bestMatchCount;
  uint32_t precisionScore = 100UL;
  if (bestIntervalMs > 0) {
    const uint32_t precisionPenalty = (averageErrorMs * 320UL) / bestIntervalMs;
    precisionScore = (precisionPenalty >= 100UL) ? 0UL : (100UL - precisionPenalty);
  }

  uint32_t coverageScore = (bestMatchCount * 100UL) / kAnalysisTempoCoverageTarget;
  if (coverageScore > 100UL) {
    coverageScore = 100UL;
  }

  const uint32_t separationScore =
    (bestFamilyScore > 0 && bestFamilyScore > secondBestFamilyScore)
      ? ((bestFamilyScore - secondBestFamilyScore) * 100UL) / bestFamilyScore
      : 0UL;

  uint32_t primaryCoverageScore =
    (primaryMatchCounts[bestBucketIndex] * 100UL) / kAnalysisTempoPrimaryCoverageTarget;
  if (primaryCoverageScore > 100UL) {
    primaryCoverageScore = 100UL;
  }

  uint32_t continuityScore = 60UL;
  if (previousFamilyBpm != 0) {
    if (previousFamilyBpm == bestFamilyBpm) {
      continuityScore = 100UL;
    } else {
      const uint16_t familyDelta = bpmDifference(previousFamilyBpm, bestFamilyBpm);
      const uint32_t continuityPenalty = familyDelta * 6UL;
      continuityScore = (continuityPenalty >= 100UL) ? 0UL : (100UL - continuityPenalty);
    }
  }

  const uint32_t confidence =
    ((precisionScore * 2UL) + coverageScore + primaryCoverageScore + separationScore +
     continuityScore) / 6UL;
  confidence_ = static_cast<uint8_t>((confidence > 100UL) ? 100UL : confidence);

  const uint16_t bestRawBpm = static_cast<uint16_t>(kAnalysisMinTempoBpm + bestBucketIndex);
  uint16_t resolvedRawBpm = bestRawBpm;
  if (previousDetectedBpm != 0 && inSameTempoFamily(previousDetectedBpm, bestRawBpm)) {
    const uint8_t previousBucketIndex =
      static_cast<uint8_t>(previousDetectedBpm - kAnalysisMinTempoBpm);
    const uint32_t previousFamilySupport =
      (primaryScores[previousBucketIndex] * kAnalysisTempoPrimaryScoreWeight) +
      exactIntervalScores[previousBucketIndex] + pulseScores[previousBucketIndex];
    const uint32_t bestFamilySupport =
      (primaryScores[bestBucketIndex] * kAnalysisTempoPrimaryScoreWeight) +
      exactIntervalScores[bestBucketIndex] + pulseScores[bestBucketIndex];

    if (bpmDifference(previousDetectedBpm, bestRawBpm) >= 6U && previousFamilySupport > 0 &&
        (bestFamilySupport * 100UL) <
          (previousFamilySupport * kAnalysisTempoSameFamilySwitchPercent)) {
      resolvedRawBpm = previousDetectedBpm;
    } else if (bpmDifference(previousDetectedBpm, bestRawBpm) <= 2U) {
      resolvedRawBpm =
        static_cast<uint16_t>((previousDetectedBpm + bestRawBpm + 1U) / 2U);
    }
  }
  detectedBpm_ = resolvedRawBpm;

  uint16_t resolvedClockBpm = detectedBpm_;
  const uint16_t canonicalClockBpm = canonicalTempoFamilyBpm(detectedBpm_);
  if (canonicalClockBpm > detectedBpm_ && canonicalClockBpm >= kAnalysisTempoFamilyPreferredMinBpm) {
    uint32_t fastFamilyEvidence = 0;
    uint8_t fastFamilyMatches = 0;

    for (uint8_t i = 0; i < kAnalysisTempoBucketCount; ++i) {
      const uint16_t bpm = static_cast<uint16_t>(kAnalysisMinTempoBpm + i);
      if (!inSameTempoFamily(detectedBpm_, bpm)) {
        continue;
      }
      if ((bpm + 6U) < canonicalClockBpm) {
        continue;
      }

      const uint32_t evidence =
        (primaryScores[i] * kAnalysisTempoPrimaryScoreWeight) +
        exactIntervalScores[i] +
        (pulseScores[i] * kAnalysisPulseScoreWeight);
      if (evidence > fastFamilyEvidence) {
        fastFamilyEvidence = evidence;
        fastFamilyMatches = primaryMatchCounts[i];
      }
    }

    if (fastFamilyEvidence > 0 &&
        (fastFamilyMatches > 0 ||
         (fastFamilyEvidence * 100UL) >= (bestRepresentativeScore * 35UL))) {
      resolvedClockBpm = canonicalClockBpm;
    }
  }

  uint16_t targetClockBpm = resolvedClockBpm;
  if (previousClockBpm != 0 && inSameTempoFamily(previousClockBpm, resolvedClockBpm) &&
      (resolvedClockBpm + 6U) < previousClockBpm) {
    if (clockSubdivisionCandidateCount_ == 0 ||
        bpmDifference(clockSubdivisionCandidateBpm_, resolvedClockBpm) > 2U) {
      clockSubdivisionCandidateBpm_ = resolvedClockBpm;
      clockSubdivisionCandidateCount_ = 1;
      targetClockBpm = previousClockBpm;
    } else if (clockSubdivisionCandidateCount_ < kAnalysisClockSubdivisionSwitchHits) {
      clockSubdivisionCandidateCount_++;
      targetClockBpm = previousClockBpm;
    } else {
      targetClockBpm = clockSubdivisionCandidateBpm_;
    }
  } else {
    clockSubdivisionCandidateBpm_ = 0;
    clockSubdivisionCandidateCount_ = 0;
  }

  if (previousClockBpm != 0 && inSameTempoFamily(previousClockBpm, targetClockBpm) &&
      bpmDifference(previousClockBpm, targetClockBpm) <= 2U) {
    clockBpm_ = static_cast<uint16_t>((previousClockBpm + targetClockBpm + 1U) / 2U);
  } else {
    clockBpm_ = targetClockBpm;
  }
}

bool BeatAnalyzer::resolvePeriodicTempo(uint16_t& bpm, uint8_t& confidence) const {
  bpm = 0;
  confidence = 0;
  if (energyHistoryCount_ < kPeriodicityMinimumFrames || analysisFrameIntervalMs_ == 0) {
    return false;
  }

  uint32_t bestScore = 0;
  uint16_t bestBpm = 0;
  for (uint16_t candidateBpm = kPeriodicityMinBpm;
       candidateBpm <= kPeriodicityMaxBpm; ++candidateBpm) {
    const uint32_t lagSixteenths =
      (60000UL * 16UL + (candidateBpm * analysisFrameIntervalMs_ / 2U)) /
      (candidateBpm * analysisFrameIntervalMs_);
    const uint16_t lagFrames = static_cast<uint16_t>(lagSixteenths / 16UL);
    const uint16_t lagFraction = static_cast<uint16_t>(lagSixteenths % 16UL);
    if (lagFrames < 2U || lagFrames + 1U >= energyHistoryCount_) continue;

    int64_t currentSum = 0;
    int64_t laggedSum = 0;
    uint16_t pairCount = 0;
    for (uint16_t i = static_cast<uint16_t>(lagFrames + 1U);
         i < energyHistoryCount_; ++i) {
      const uint16_t currentIndex = static_cast<uint16_t>(
        (energyHistoryIndex_ + kPeriodicityHistorySize - energyHistoryCount_ + i) %
        kPeriodicityHistorySize);
      const uint16_t recentIndex = static_cast<uint16_t>(
        (energyHistoryIndex_ + kPeriodicityHistorySize - energyHistoryCount_ + i - lagFrames) %
        kPeriodicityHistorySize);
      const uint16_t olderIndex = static_cast<uint16_t>(
        (energyHistoryIndex_ + kPeriodicityHistorySize - energyHistoryCount_ + i - lagFrames - 1U) %
        kPeriodicityHistorySize);
      const uint32_t lagged =
        (static_cast<uint32_t>(fluxHistory_[recentIndex]) * (16U - lagFraction) +
         static_cast<uint32_t>(fluxHistory_[olderIndex]) * lagFraction) / 16U;
      currentSum += fluxHistory_[currentIndex];
      laggedSum += lagged;
      ++pairCount;
    }
    if (pairCount == 0) continue;

    const int64_t currentMean = currentSum / pairCount;
    const int64_t laggedMean = laggedSum / pairCount;
    int64_t numerator = 0;
    uint64_t currentEnergy = 0;
    uint64_t laggedEnergy = 0;
    for (uint16_t i = static_cast<uint16_t>(lagFrames + 1U);
         i < energyHistoryCount_; ++i) {
      const uint16_t currentIndex = static_cast<uint16_t>(
        (energyHistoryIndex_ + kPeriodicityHistorySize - energyHistoryCount_ + i) %
        kPeriodicityHistorySize);
      const uint16_t recentIndex = static_cast<uint16_t>(
        (energyHistoryIndex_ + kPeriodicityHistorySize - energyHistoryCount_ + i - lagFrames) %
        kPeriodicityHistorySize);
      const uint16_t olderIndex = static_cast<uint16_t>(
        (energyHistoryIndex_ + kPeriodicityHistorySize - energyHistoryCount_ + i - lagFrames - 1U) %
        kPeriodicityHistorySize);
      const int64_t current = static_cast<int64_t>(fluxHistory_[currentIndex]) - currentMean;
      const int64_t lagged = static_cast<int64_t>(
        (static_cast<uint32_t>(fluxHistory_[recentIndex]) * (16U - lagFraction) +
         static_cast<uint32_t>(fluxHistory_[olderIndex]) * lagFraction) / 16U) - laggedMean;
      numerator += current * lagged;
      currentEnergy += static_cast<uint64_t>(current * current);
      laggedEnergy += static_cast<uint64_t>(lagged * lagged);
    }
    if (numerator <= 0 || currentEnergy + laggedEnergy == 0) continue;
    const uint32_t score = static_cast<uint32_t>(
      (static_cast<uint64_t>(numerator) * 1000UL) / (currentEnergy + laggedEnergy));
    if (score > bestScore) {
      bestScore = score;
      bestBpm = candidateBpm;
    }
  }

  if (bestBpm == 0) return false;
  bpm = bestBpm;
  confidence = static_cast<uint8_t>(bestScore > 100UL ? 100UL : bestScore);
  return true;
}


}  // namespace decaflash::mainframe
