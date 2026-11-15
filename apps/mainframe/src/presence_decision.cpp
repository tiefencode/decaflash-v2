#include "presence_decision.h"

namespace decaflash::mainframe {

void PresenceDecision::reset() {
  windowHead_ = 0;
  windowCount_ = 0;
  activity_ = 0;
  present_ = false;
  arrived_ = false;
  movesArmed_ = true;
}

void PresenceDecision::setState(PersonState state, uint32_t now) {
  if (!haveState_ || state != state_) {
    state_ = state;
    stateChangedAt_ = now;
    haveState_ = true;
  }
}

void PresenceDecision::sensorConfigured(uint32_t now) {
  reset();
  sensorReady_ = true;
  settleUntil_ = now + config_.settleMs;
  setState(PersonState::Away, now);
}

void PresenceDecision::sensorMissing(uint32_t now) {
  // No event: a vanished sensor is not a person leaving.
  reset();
  sensorReady_ = false;
  setState(PersonState::Away, now);
}

uint16_t PresenceDecision::computeActivity(uint32_t now) const {
  uint32_t total = 0;
  uint32_t moving = 0;
  for (size_t i = 0; i < windowCount_; ++i) {
    const size_t at = (windowHead_ + kWindowSamples - 1 - i) % kWindowSamples;
    if (now - windowAt_[at] >= config_.activityWindowMs) break;
    ++total;
    if (windowMotion_[at]) ++moving;
  }
  return total ? static_cast<uint16_t>(moving * 1000U / total) : 0;
}

PresenceEvent PresenceDecision::makeEvent(PresenceEventKind kind, uint32_t now) {
  PresenceEvent event;
  event.kind = kind;
  event.atMs = now;
  event.sequence = sequences_[static_cast<uint8_t>(kind)]++;
  return event;
}

PresenceEvent PresenceDecision::feed(const PresenceSampleInput& sample) {
  const uint32_t now = sample.atMs;
  const bool settling = static_cast<int32_t>(now - settleUntil_) < 0;
  if (!sensorReady_ || settling || !sample.valid) {
    // Unusable sample: nobody is present, silently. A read glitch or the
    // settling phase must not produce "left" or "arrived" texts.
    if (present_) reset();
    setState(PersonState::Away, now);
    return {};
  }

  const bool wasPresent = present_;
  if (!present_ && sample.presence >= config_.enterThreshold) {
    present_ = true;
    presentSince_ = now;
    arrived_ = false;
    movesArmed_ = true;
    windowHead_ = 0;
    windowCount_ = 0;
  } else if (present_ && sample.presence < config_.leaveThreshold) {
    const bool hadArrived = arrived_;
    reset();
    setState(PersonState::Away, now);
    return makeEvent(hadArrived ? PresenceEventKind::Left : PresenceEventKind::WalkedPast, now);
  }

  if (!present_) {
    setState(PersonState::Away, now);
    return {};
  }

  // Only motion while present counts; the swing after leaving never gets here.
  // The ringing motion output right after stepping in is ignored as well.
  const bool movementCounts = now - presentSince_ >= config_.movementIgnoreMs;
  if (movementCounts) {
    windowAt_[windowHead_] = now;
    windowMotion_[windowHead_] = sample.motionFlag;
    windowHead_ = (windowHead_ + 1) % kWindowSamples;
    if (windowCount_ < kWindowSamples) ++windowCount_;
  }
  activity_ = movementCounts ? computeActivity(now) : 0;
  setState(activity_ > 0 ? PersonState::Moving : PersonState::Still, now);

  if (!arrived_ && wasPresent && now - presentSince_ >= config_.dwellMs) {
    arrived_ = true;
    return makeEvent(PresenceEventKind::ArrivedInFront, now);
  }
  // Movement is judged only over a full window after the ignore time, so the
  // motion of stepping in is not mistaken for dancing.
  if (now - presentSince_ >= config_.movementIgnoreMs + config_.activityWindowMs) {
    if (movesArmed_ && activity_ >= config_.movesALotPermille) {
      movesArmed_ = false;
      return makeEvent(PresenceEventKind::MovesALot, now);
    }
    if (!movesArmed_ && activity_ < config_.movesALotRearmPermille) movesArmed_ = true;
  }
  return {};
}

}  // namespace decaflash::mainframe
