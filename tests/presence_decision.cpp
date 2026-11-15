#include "presence_decision.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace decaflash::mainframe;

namespace {

constexpr uint32_t kStep = 125;  // 8 Hz

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

  // Alternating motion flag, `onOutOf` samples on per `period` samples.
  void dance(uint32_t durationMs, int16_t presence, int onOutOf, int period) {
    int i = 0;
    for (uint32_t t = 0; t < durationMs; t += kStep, ++i) {
      PresenceSampleInput sample;
      sample.valid = true;
      sample.presence = presence;
      sample.motionFlag = (i % period) < onOutOf;
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

void testEnterAndHysteresis() {
  Feeder f;
  f.run(1000, 899);
  assert(!f.decision.present());
  f.run(250, 900);
  assert(f.decision.present());
  assert(f.decision.state() == PersonState::Still);
  f.run(1000, 700);  // between leave and enter
  assert(f.decision.present());
  f.run(250, 599);
  assert(!f.decision.present());
  assert(f.decision.state() == PersonState::Away);
}

void testStaleBaselineAndFarAway() {
  Feeder f;
  f.run(60000, 340, false);  // stale baseline, sensor flag would be on
  assert(!f.decision.present());
  f.run(60000, 550, false);  // still person at 2 m
  assert(!f.decision.present());
  assert(f.events.empty());
}

void testSettlingAndMissing() {
  PresenceDecision decision;
  PresenceSampleInput sample;
  sample.valid = true;
  sample.presence = 2000;
  sample.atMs = 100;
  assert(decision.feed(sample).kind == PresenceEventKind::None);  // not configured
  assert(!decision.present());
  decision.sensorConfigured(1000);
  sample.atMs = 1000 + 14999;
  decision.feed(sample);
  assert(!decision.present());
  sample.atMs = 1000 + 15000;
  decision.feed(sample);
  assert(decision.present());
  decision.sensorMissing(20000);
  assert(!decision.present());
  sample.atMs = 20125;
  assert(decision.feed(sample).kind == PresenceEventKind::None);
  assert(!decision.present());
}

void testInvalidSampleIsSilent() {
  Feeder f;
  f.run(5000, 2000);
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

void testArrivalWalkPastAndLeave() {
  Feeder walk;
  walk.run(2500, 2100, true);  // shorter than the dwell time
  walk.run(500, 0);
  assert(walk.count(PresenceEventKind::WalkedPast) == 1);
  assert(walk.count(PresenceEventKind::ArrivedInFront) == 0);
  assert(walk.count(PresenceEventKind::Left) == 0);

  Feeder stay;
  stay.run(10000, 1940);
  assert(stay.count(PresenceEventKind::ArrivedInFront) == 1);
  stay.run(500, 0);
  assert(stay.count(PresenceEventKind::Left) == 1);
  assert(stay.count(PresenceEventKind::WalkedPast) == 0);
  // Arrival is reported once per visit, with increasing sequence per kind.
  stay.run(5000, 1940);
  assert(stay.count(PresenceEventKind::ArrivedInFront) == 2);
  assert(stay.events.back().sequence == 1);
}

void testMovementActivity() {
  Feeder f;
  f.run(4000, 1940, false);
  assert(f.decision.activityPermille() == 0);
  assert(f.decision.state() == PersonState::Still);
  f.dance(4000, 1940, 3, 4);  // flag on 75 % of samples
  assert(f.decision.activityPermille() >= 700 && f.decision.activityPermille() <= 800);
  assert(f.decision.state() == PersonState::Moving);
  assert(f.decision.mood().present);
  // After leaving the motion swing counts nothing.
  f.run(500, 0, true);
  assert(f.decision.activityPermille() == 0);
  assert(f.decision.mood().activityPermille == 0);
}

void testMovesALot() {
  Feeder f;
  f.run(4000, 1940);
  f.dance(4000, 1940, 3, 4);
  assert(f.count(PresenceEventKind::MovesALot) == 1);
  f.dance(4000, 1940, 3, 4);  // still dancing: no second event
  assert(f.count(PresenceEventKind::MovesALot) == 1);
  f.run(4000, 1940, false);  // calm down below the re-arm level
  f.dance(4000, 1940, 3, 4);
  assert(f.count(PresenceEventKind::MovesALot) == 2);

  // Stepping in with motion is not dancing.
  Feeder arrive;
  arrive.run(2000, 1940, true);
  assert(arrive.count(PresenceEventKind::MovesALot) == 0);
}

void testStateChangeTime() {
  Feeder f;
  f.run(1000, 0);
  const uint32_t before = f.now;
  f.run(250, 1940);
  assert(f.decision.state() == PersonState::Still);
  assert(f.decision.stateChangedAtMs() == before);
}

void testOverflow() {
  Feeder f(0xFFFFFFFFU - 20000U);  // settling ends just before the wrap
  f.run(10000, 1940);
  assert(f.decision.present());
  assert(f.count(PresenceEventKind::ArrivedInFront) == 1);
  f.run(500, 0);
  assert(f.count(PresenceEventKind::Left) == 1);
}

}  // namespace

int main() {
  testEnterAndHysteresis();
  testStaleBaselineAndFarAway();
  testSettlingAndMissing();
  testInvalidSampleIsSilent();
  testArrivalWalkPastAndLeave();
  testMovementActivity();
  testMovesALot();
  testStateChangeTime();
  testOverflow();
  std::puts("presence_decision: all tests passed");
  return 0;
}
