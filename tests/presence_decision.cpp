#include "presence_decision.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace decaflash::mainframe;

namespace {

constexpr uint32_t kStep = 125;  // 8 Hz
constexpr int16_t kFar = 1500;   // in range, below the near zone
constexpr int16_t kNear = 6000;  // directly in front

struct Feeder {
  PresenceDecision decision;
  uint32_t now;
  std::vector<PresenceEvent> events;

  explicit Feeder(uint32_t start = 0) : now(start) {
    decision.sensorConfigured(now);
    now += 15000;  // past settling
  }

  void run(uint32_t durationMs, int16_t presence, bool motion = false) {
    for (uint32_t t = 0; t < durationMs; t += kStep) {
      PresenceSampleInput sample;
      sample.valid = true;
      sample.presence = presence;
      sample.motionFlag = motion;
      sample.atMs = now;
      const auto event = decision.feed(sample);
      if (event.kind != PresenceEventKind::None) events.push_back(event);
      now += kStep;
    }
  }

  int count(PresenceEventKind kind) const {
    int n = 0;
    for (const auto& e : events) n += e.kind == kind;
    return n;
  }
};

void testPresenceHysteresis() {
  Feeder f;
  f.run(1000, 899);
  assert(!f.decision.present());
  f.run(250, 900);
  assert(f.decision.present() && !f.decision.near());
  assert(f.decision.state() == PersonState::Far);
  f.run(1000, 700);  // between leave and enter
  assert(f.decision.present());
  f.run(250, 599);
  assert(!f.decision.present());
  assert(f.decision.state() == PersonState::Away);
}

void testNearZone() {
  Feeder f;
  f.run(2000, kFar);
  assert(f.decision.present() && !f.decision.near());
  f.run(2000, 2500);  // about 1 m: far since tune-loneliness-presence
  assert(f.decision.present() && !f.decision.near());
  f.run(250, 2800);
  assert(f.decision.near());
  assert(f.decision.state() == PersonState::Near);
  f.run(1000, 2500);  // between near leave and near enter
  assert(f.decision.near());
  f.run(1500, 2199);  // a short dip below the leave threshold does not count
  assert(f.decision.near());
  f.run(1000, 3500);  // back up: the hold time starts again
  f.run(1875, 2199);
  assert(f.decision.near());
  f.run(250, 2199);  // below for the full 2 s
  assert(f.decision.present() && !f.decision.near());
  assert(f.decision.mood().present && !f.decision.mood().near);
}

void testStaleBaselineStaysAway() {
  Feeder f;
  f.run(60000, 340);  // stale baseline, sensor flag would be on
  assert(!f.decision.present());
  assert(f.events.empty());
}

void testSettlingAndMissing() {
  PresenceDecision decision;
  PresenceSampleInput sample;
  sample.valid = true;
  sample.presence = kNear;
  sample.atMs = 100;
  assert(decision.feed(sample).kind == PresenceEventKind::None);  // not configured
  assert(!decision.present());
  decision.sensorConfigured(1000);
  sample.atMs = 1000 + 14999;
  decision.feed(sample);
  assert(!decision.present());
  sample.atMs = 1000 + 15000;
  decision.feed(sample);
  assert(decision.present() && decision.near());
  decision.sensorMissing(20000);
  assert(!decision.present());
  sample.atMs = 20125;
  assert(decision.feed(sample).kind == PresenceEventKind::None);
  assert(!decision.present());
}

void testInvalidSampleIsSilent() {
  Feeder f;
  f.run(5000, kNear);
  assert(f.count(PresenceEventKind::ArrivedInFront) == 1);
  PresenceSampleInput bad;
  bad.valid = false;
  bad.atMs = f.now;
  assert(f.decision.feed(bad).kind == PresenceEventKind::None);
  assert(!f.decision.present());
  f.now += kStep;
  f.run(500, 0);
  assert(f.count(PresenceEventKind::Left) == 0);
}

void testArrivedOnlyNear() {
  Feeder far;
  far.run(20000, kFar);  // standing farther away for a long time
  assert(far.count(PresenceEventKind::ArrivedInFront) == 0);
  assert(far.count(PresenceEventKind::StandsFar) == 1);  // once, after 2 s
  far.run(500, 0);
  assert(far.events.size() == 1);  // leaving from far adds no event

  Feeder near;
  near.run(1750, kNear);
  assert(near.count(PresenceEventKind::ArrivedInFront) == 0);  // dwell not reached
  near.run(500, kNear);
  assert(near.count(PresenceEventKind::ArrivedInFront) == 1);
  near.run(5000, kNear);
  assert(near.count(PresenceEventKind::ArrivedInFront) == 1);
  near.run(20000, kNear);
  assert(near.count(PresenceEventKind::ArrivedInFront) == 1);  // once per near stay
}

void testLeftWhenLeavingNearZone() {
  Feeder f;
  f.run(5000, kNear);
  assert(f.count(PresenceEventKind::ArrivedInFront) == 1);
  f.run(3000, kFar);  // steps back but stays in range (2 s hold time)
  assert(f.count(PresenceEventKind::Left) == 1);
  assert(f.decision.present());
  f.run(500, 0);  // leaves completely: no second "left"
  assert(f.count(PresenceEventKind::Left) == 1);
  assert(f.count(PresenceEventKind::WalkedPast) == 0);

  Feeder direct;
  direct.run(5000, kNear);
  direct.run(500, 0);  // from directly in front straight out of range
  assert(direct.count(PresenceEventKind::Left) == 1);

  // A short step into the near zone without the dwell time: no event.
  Feeder brief;
  brief.run(2000, kFar);
  brief.run(1500, kNear);
  brief.run(3000, kFar);
  assert(brief.events.empty());
}

void testWalkPast() {
  Feeder walk;
  walk.run(1500, kFar, true);  // shorter than the far dwell time
  walk.run(500, 0);
  assert(walk.count(PresenceEventKind::WalkedPast) == 1);
  assert(walk.count(PresenceEventKind::ArrivedInFront) == 0);
  assert(walk.count(PresenceEventKind::Left) == 0);

  Feeder fast;
  fast.run(1000, kNear, true);  // through the near zone, about 1 s
  fast.run(1000, kFar, true);
  fast.run(3000, 0);
  assert(fast.count(PresenceEventKind::WalkedPast) == 1);
  assert(fast.count(PresenceEventKind::ArrivedInFront) == 0);
  assert(fast.events.back().sequence == 0);
}

void testStandsFar() {
  Feeder f;
  f.run(1750, kFar);
  assert(f.count(PresenceEventKind::StandsFar) == 0);
  f.run(500, kFar);
  assert(f.count(PresenceEventKind::StandsFar) == 1);
  f.run(5000, kNear);  // steps in: greeted again in front
  assert(f.count(PresenceEventKind::ArrivedInFront) == 1);
  f.run(5000, kFar);  // steps back: left, but no second far greeting
  assert(f.count(PresenceEventKind::Left) == 1);
  assert(f.count(PresenceEventKind::StandsFar) == 1);

  // Greeted in front first: stepping back gives no far greeting either.
  Feeder front;
  front.run(5000, kNear);
  front.run(5000, kFar);
  assert(front.count(PresenceEventKind::StandsFar) == 0);

  // A visit of 2-3 s with the far greeting is no walk-past.
  Feeder linger;
  linger.run(2500, kFar);
  linger.run(500, 0);
  assert(linger.count(PresenceEventKind::StandsFar) == 1);
  assert(linger.count(PresenceEventKind::WalkedPast) == 0);
}

void testMovesEvent() {
  Feeder f;
  f.run(15000, kFar, true);  // ringing time: no movement events yet
  assert(f.count(PresenceEventKind::Moves) == 0);
  f.run(1000, kFar, true);
  assert(f.count(PresenceEventKind::Moves) == 1);  // at once when moving
  f.run(7000, kFar, true);
  assert(f.count(PresenceEventKind::Moves) == 1);  // cooldown 9 s
  f.run(2000, kFar, true);
  assert(f.count(PresenceEventKind::Moves) == 2);
  f.run(20000, kFar, false);  // standing still: no more
  assert(f.count(PresenceEventKind::Moves) == 2);

  Feeder near;  // movement counts directly in front as well
  near.run(17000, kNear, true);
  assert(near.count(PresenceEventKind::Moves) == 1);
}

void testMovement() {
  Feeder ring;
  ring.run(14000, kFar, true);  // ringing motion after stepping in
  assert(!ring.decision.moving());
  assert(ring.decision.state() == PersonState::Far);
  ring.run(3000, kFar, true);
  assert(ring.decision.moving());
  assert(ring.decision.state() == PersonState::FarMoving);
  assert(ring.decision.mood().moving);
  ring.run(3500, kFar, false);  // calm for longer than the window
  assert(!ring.decision.moving());
  assert(ring.decision.state() == PersonState::Far);

  Feeder near;
  near.run(20000, kNear, true);
  assert(near.decision.state() == PersonState::Near);  // movement does not change near
  near.run(500, 0, true);  // swing after leaving counts nothing
  assert(!near.decision.moving());
}

void testStateChangeTime() {
  Feeder f;
  f.run(1000, 0);
  const uint32_t before = f.now;
  f.run(250, kFar);
  assert(f.decision.state() == PersonState::Far);
  assert(f.decision.stateChangedAtMs() == before);
}

void testOverflow() {
  Feeder f(0xFFFFFFFFU - 20000U);  // settling ends just before the wrap
  f.run(10000, kNear);
  assert(f.decision.near());
  assert(f.count(PresenceEventKind::ArrivedInFront) == 1);
  f.run(500, 0);
  assert(f.count(PresenceEventKind::Left) == 1);
}

}  // namespace

int main() {
  testPresenceHysteresis();
  testNearZone();
  testStaleBaselineStaysAway();
  testSettlingAndMissing();
  testInvalidSampleIsSilent();
  testArrivedOnlyNear();
  testLeftWhenLeavingNearZone();
  testWalkPast();
  testStandsFar();
  testMovesEvent();
  testMovement();
  testStateChangeTime();
  testOverflow();
  std::puts("presence_decision: all tests passed");
  return 0;
}
