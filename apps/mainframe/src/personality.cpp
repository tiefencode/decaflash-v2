#include "personality.h"
#include <algorithm>
#include <cmath>

namespace decaflash::mainframe {
namespace {
constexpr int32_t kFullEnergy = 100000;
constexpr int32_t kCreatureInitialEnergy = 100000;
constexpr int32_t kSleepThresholdEnergy = 20000;
constexpr int32_t kWakeEnergy = 25000;
constexpr uint32_t kCreatureDrainMs = 600000;
constexpr uint32_t kCreatureRechargeMs = 300000;
constexpr uint32_t kCreatureSleepInactivityMs = 10000;
constexpr uint32_t kCreatureDrainStepMs = kCreatureDrainMs / kFullEnergy;
constexpr uint32_t kCreatureRechargeStepMs = kCreatureRechargeMs / kFullEnergy;
constexpr int32_t kDepressionSocialThreshold = 80000;
constexpr int32_t kDepressionSocialMaxRate = 200;  // 0.2 points/s at 100.
constexpr int32_t kDepressionTempoMaxRate = 100;
constexpr int32_t kDepressionBassMaxRate = 120;
constexpr int32_t kDepressionMusicRecoveryRate = 50;  // 0.05 points/s.

int32_t toward(int32_t value, int32_t target, int32_t amount) {
  return value < target ? std::min(target, value + amount) : std::max(target, value - amount);
}
int32_t add(int32_t value, int32_t amount) { return std::min(100000, value + amount); }

uint32_t nextSnoreDelay(uint32_t& random) {
  random = random * 1664525UL + 1013904223UL;
  // A 30--65 second gap keeps the occasional sound readable as snoring,
  // rather than as another threshold notification.
  return 30000UL + random % 35001UL;
}
}
void Personality::update(uint32_t now, const MoodAudio& audio) {
  if (!started_) { started_ = true; lastUpdate_ = now; }
  uint32_t elapsed = now - lastUpdate_;
  lastUpdate_ = now;
  if (elapsed > 1000) { elapsed = 1000; quietPending_ = false; candidateCount_ = 0; }

  if (audio.creatureMode && !creatureMode_) {
    // The pre-analysis display is intentionally independent from the old
    // default energy value and starts in the configured low-energy state.
    energy_ = kCreatureInitialEnergy;
    creatureDrainRemainder_ = 0;
    creatureRechargeRemainder_ = 0;
    sleeping_ = false;
    lowEnergySince_ = 0;
    nextSnoreAt_ = 0;
  }
  if (!audio.creatureMode && creatureMode_) {
    sleeping_ = false;
    nextSnoreAt_ = 0;
  }
  creatureMode_ = audio.creatureMode;
  const uint32_t lonelyElapsed = elapsed + lonelinessRemainder_;
  lonelinessRemainder_ = lonelyElapsed % 10;
  loneliness_ = add(loneliness_, lonelyElapsed / 10);
  const uint32_t annoyedElapsed = elapsed + annoyanceRemainder_;
  annoyanceRemainder_ = annoyedElapsed % 10;
  annoyance_ = toward(annoyance_, 0, annoyedElapsed / 10);
  attention_ = toward(attention_, 0, elapsed * 3);

  const bool quiet = audio.fresh && audio.silent;
  if (!quiet) quietPending_ = false;
  else if (!quietPending_) { quietPending_ = true; quietSince_ = now; }
  const bool reliable = audio.fresh && !quiet && audio.bpm >= 60 && audio.bpm <= 200 &&
                        audio.confidence >= 68 && now - audio.onsetAtMs <= 1500;
  if (!reliable) candidateCount_ = 0;
  if (reliable && (!haveOnset_ || audio.onsetAtMs != lastOnset_)) {
    if (haveOnset_ && audio.onsetAtMs - lastOnset_ > 1500) candidateCount_ = 0;
    haveOnset_ = true; lastOnset_ = audio.onsetAtMs;
    if (!candidateCount_ || std::abs(static_cast<int>(audio.bpm) - candidateBpm_) > 4) {
      candidateBpm_ = audio.bpm; candidateCount_ = 1;
    } else {
      candidateBpm_ = (candidateBpm_ + audio.bpm) / 2;
      if (candidateCount_ < 3) ++candidateCount_;
    }
    if (candidateCount_ >= 3) { trustedBpm_ = candidateBpm_; tempoAt_ = now; haveTempo_ = true; }
  }
  const bool beatPresent = audio.fresh && !quiet && audio.confidence >= 68 &&
                           audio.onsetAtMs != 0 && now - audio.onsetAtMs <= 1500;
  const bool tempoUsable = audio.fresh && !quiet && haveTempo_ && now - tempoAt_ <= 15000;
  const bool energyTempoAvailable = audio.fresh && !quiet &&
                                    audio.bpm >= 60 && audio.bpm <= 200;
  const int bpm = std::max(80, std::min(160, static_cast<int>(trustedBpm_)));
  if (creatureMode_) {
    if (sleeping_) {
      const uint32_t recharge = elapsed + creatureRechargeRemainder_;
      creatureRechargeRemainder_ = recharge % kCreatureRechargeStepMs;
      energy_ = add(energy_, static_cast<int32_t>(recharge / kCreatureRechargeStepMs));
      if (energy_ == kFullEnergy) {
        // A full recharge ends sleep on its own. The normal event wake at 25
        // remains available throughout the earlier part of the nap.
        sleeping_ = false;
        lowEnergySince_ = 0;
        nextSnoreAt_ = 0;
      }
    } else {
      // 100 points drain in ten minutes. Keeping the remainder makes the
      // result identical for the 100 ms production cadence and finer tests.
      const uint32_t drain = elapsed + creatureDrainRemainder_;
      creatureDrainRemainder_ = drain % kCreatureDrainStepMs;
      energy_ = std::max(0, energy_ - static_cast<int32_t>(drain / kCreatureDrainStepMs));
      if (energy_ > kSleepThresholdEnergy) {
        lowEnergySince_ = 0;
      } else if (lowEnergySince_ == 0) {
        lowEnergySince_ = now;
      }
      if (energy_ == 0 ||
          (lowEnergySince_ != 0 && now - lowEnergySince_ >= kCreatureSleepInactivityMs)) {
        sleeping_ = true;
        creatureRechargeRemainder_ = 0;
        nextSnoreAt_ = now + nextSnoreDelay(snoreRandom_);
      }
    }
  } else if (quietPending_ && now - quietSince_ >= 1000) {
    const uint32_t quietElapsed = std::min(elapsed, now - quietSince_ - 1000);
    if (energy_ > 0) energy_ = toward(energy_, 0, quietElapsed * 20);
  } else if (energyTempoAvailable) {
    // BPM provides the primary 13–90 range, with a steeper response above
    // 120 BPM. A current, clear beat adds only
    // a small 0–10 bonus.
    const int energyBpm = std::max(80, std::min(160, static_cast<int>(audio.bpm)));
    const int32_t bpmBase = energyBpm <= 120
      ? (energyBpm - 60) * 2000 / 3
      : energyBpm <= 140
        ? 40000 + (energyBpm - 120) * 1000
        : 60000 + (energyBpm - 140) * 1500;
    const int32_t beatBonus = beatPresent
      ? (audio.confidence - 68) * 10000 / 32
      : 0;
    const int32_t energyTarget = bpmBase + beatBonus;
    const int32_t rate = energy_ > energyTarget ? elapsed * 20 : elapsed * 4;
    energy_ = toward(energy_, energyTarget, rate);
  }
  // Depression changes only while an influence is present. The social terms
  // always apply. Fresh, non-silent music gently recovers it toward zero;
  // slow tempo and weak bass add to the same rate and can override recovery.
  const int32_t lonelinessExcess = std::max(0, loneliness_ - kDepressionSocialThreshold);
  const int32_t attentionExcess = std::max(0, attention_ - kDepressionSocialThreshold);
  int32_t depressionRate = lonelinessExcess * kDepressionSocialMaxRate / 20000;
  depressionRate -= attentionExcess * kDepressionSocialMaxRate / 20000;
  if (audio.fresh && !quiet) depressionRate -= kDepressionMusicRecoveryRate;
  if (tempoUsable && bpm > 100) {
    // 80--100 BPM is intentionally neutral: this range often represents
    // half-time readings of energetic music. The melancholy contribution
    // peaks at 120 BPM and fades again by 160 BPM.
    const int32_t tempoContribution = bpm <= 120
      ? (bpm - 100) * kDepressionTempoMaxRate / 20
      : (160 - bpm) * kDepressionTempoMaxRate / 40;
    depressionRate += tempoContribution;
  }
  if (audio.fresh && !quiet && audio.bassValid) {
    // Broad bass power <8% adds 0.12 points/s; >=25% adds nothing.
    const int deficit = std::max(0, std::min(170, 250 - static_cast<int>(audio.bassPermille)));
    depressionRate += deficit * kDepressionBassMaxRate / 170;
  }
  const int64_t depressionScaled = static_cast<int64_t>(depressionRate) * elapsed +
                                   depressionRateRemainder_;
  depressionRateRemainder_ = static_cast<int16_t>(depressionScaled % 1000);
  const int32_t depressionDelta = static_cast<int32_t>(depressionScaled / 1000);
  depression_ = std::max(0, std::min(kFullEnergy, depression_ + depressionDelta));
}
void Personality::onMotion(const MotionEvent& event) {
  int attention = 0, annoyance = 0, relief = 0;
  switch (event.kind) {
    case MotionKind::Move: attention = 8; relief = 5; break;
    case MotionKind::Tap: attention = 12; relief = 8; break;
    case MotionKind::MultiTap:
      attention = 18;
      relief = 12;
      if (event.count >= 3) annoyance = 6;
      break;
    case MotionKind::Impact: attention = 20; annoyance = 15; relief = 10; break;
    case MotionKind::Rotate: attention = 10; relief = 6; break;
    case MotionKind::Tilt: break;
    case MotionKind::Shake: attention = 25; annoyance = 10; relief = 15; break;
    case MotionKind::None: return;
  }
  if (creatureMode_) {
    if (!sleeping_) {
      // A fresh event restarts the low-energy grace period. This keeps the
      // creature visibly awake long enough for the tired crossing sound.
      lowEnergySince_ = 0;
    } else if (energy_ >= kWakeEnergy) {
      // Sleep is deliberately protected until it has restored a little
      // reserve. Events below 25 still affect the moods but cannot open it.
      sleeping_ = false;
      creatureDrainRemainder_ = 0;
      nextSnoreAt_ = 0;
      lowEnergySince_ = 0;
    }
  }
  attention_ = add(attention_, attention * 1000);
  annoyance_ = add(annoyance_, annoyance * 1000);
  loneliness_ = std::max(0, loneliness_ - relief * 1000);
}
bool Personality::consumeSnore(uint32_t now) {
  if (!creatureMode_ || !sleeping_ || nextSnoreAt_ == 0 ||
      static_cast<int32_t>(now - nextSnoreAt_) < 0) return false;
  nextSnoreAt_ = now + nextSnoreDelay(snoreRandom_);
  return true;
}
Mood Personality::snapshot() const {
  Mood result;
  result.energy = energy_ / 1000;
  result.annoyance = annoyance_ / 1000;
  result.attention = attention_ / 1000;
  result.loneliness = loneliness_ / 1000;
  result.depression = depression_ / 1000;
  return result;
}
Mood Personality::debugSnapshot() const {
  Mood result;
  result.energy = std::min(100, (energy_ + 500) / 1000);
  result.annoyance = std::min(100, (annoyance_ + 500) / 1000);
  result.attention = std::min(100, (attention_ + 500) / 1000);
  result.loneliness = std::min(100, (loneliness_ + 500) / 1000);
  result.depression = std::min(100, (depression_ + 500) / 1000);
  return result;
}
}  // namespace decaflash::mainframe
