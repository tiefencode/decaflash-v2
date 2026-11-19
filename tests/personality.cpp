#include "personality.h"
#include <cassert>
#include <cstdio>
using namespace decaflash::mainframe;
MoodAudio music(uint32_t now, uint16_t bpm, uint16_t bass = 300) {
  MoodAudio a;
  a.fresh = true; a.bpm = bpm; a.confidence = 80;
  a.onsetAtMs = now - now % 500; a.bassValid = true; a.bassPermille = bass;
  return a;
}
int main() {
  for (unsigned bpm = 80; bpm <= 160; bpm += 20) {
    Personality p;
    for (unsigned t = 0; t <= 40000; t += 100) p.update(t, music(t, bpm));
    const int base = bpm <= 120 ? (bpm - 60) * 2000 / 3 :
      bpm <= 140 ? 40000 + (bpm - 120) * 1000 : 60000 + (bpm - 140) * 1500;
    const int expected = (base + (80 - 68) * 10000 / 32) / 1000;
    assert(p.snapshot().energy == expected);
  }
  Personality unknown;
  MoodAudio missing;
  for (unsigned t = 0; t <= 600000; t += 100) unknown.update(t, missing);
  assert(unknown.snapshot().energy == 30 && unknown.snapshot().loneliness == 100);
  MoodAudio creatureMode;
  creatureMode.creatureMode = true;
  Personality creature;
  creature.update(0, creatureMode);
  assert(creature.snapshot().energy == 100 && !creature.sleeping());
  for (unsigned t = 100; t <= 480000; t += 100) creature.update(t, creatureMode);
  assert(creature.snapshot().energy == 20 && !creature.sleeping());
  for (unsigned t = 480100; t <= 490000; t += 100) creature.update(t, creatureMode);
  assert(creature.sleeping());
  MotionEvent wakeEvent;
  wakeEvent.kind = MotionKind::Tap;
  wakeEvent.atMs = 490000;
  creature.onMotion(wakeEvent);
  assert(creature.sleeping()); // protected until the recharge reaches 25
  for (unsigned t = 490100; t <= 510000; t += 100) creature.update(t, creatureMode);
  creature.onMotion(wakeEvent);
  assert(!creature.sleeping() && creature.snapshot().energy >= 25);
  Personality rechargingCreature;
  rechargingCreature.update(0, creatureMode);
  for (unsigned t = 100; t <= 490000; t += 100) rechargingCreature.update(t, creatureMode);
  assert(rechargingCreature.sleeping());
  for (unsigned t = 490100; t <= 735000; t += 100) {
    rechargingCreature.update(t, creatureMode);
  }
  assert(rechargingCreature.snapshot().energy == 100 && !rechargingCreature.sleeping());
  Personality uncertain;
  for (unsigned t = 0; t <= 30000; t += 100) {
    auto a = music(t, 160); a.confidence = 20; uncertain.update(t, a);
  }
  assert(uncertain.snapshot().energy == 90); // BPM drives the base without a beat bonus
  Personality sameOnset;
  auto held = music(0, 160);
  for (unsigned t = 0; t <= 1000; t += 100) sameOnset.update(t, held);
  assert(sameOnset.snapshot().energy == 34); // BPM base ramps without onset history
  Personality tempoDrop;
  for (unsigned t = 0; t <= 40000; t += 100) tempoDrop.update(t, music(t, 160));
  for (unsigned t = 40100; t <= 45000; t += 100) tempoDrop.update(t, music(t, 115));
  assert(tempoDrop.snapshot().energy <= 45); // accepted slower tempo releases energy quickly
  Personality p;
  for (unsigned t = 0; t <= 40000; t += 100) p.update(t, music(t, 160));
  MoodAudio silence; silence.fresh = true; silence.silent = true;
  for (unsigned t = 40100; t <= 41100; t += 100) p.update(t, silence);
  assert(p.snapshot().energy == 93); // one second before decline
  for (unsigned t = 41200; t <= 45800; t += 100) p.update(t, silence);
  assert(p.snapshot().energy == 0); // confirmed silence reaches zero
  for (unsigned t = 45900; t <= 48900; t += 100) p.update(t, music(t, 160));
  assert(p.snapshot().energy >= 0);
  const auto before = p.snapshot().energy;
  for (unsigned t = 49000; t <= 51000; t += 100) p.update(t, missing);
  assert(p.snapshot().energy == before); // device failure never means silence
  Personality slow, fast, noBass;
  for (unsigned t = 0; t <= 400000; t += 100) {
    slow.update(t, music(t, 120)); fast.update(t, music(t, 160));
    noBass.update(t, music(t, 160, 0));
  }
  assert(slow.snapshot().depression > fast.snapshot().depression);
  assert(noBass.snapshot().depression > fast.snapshot().depression);
  assert(slow.snapshot().loneliness == fast.snapshot().loneliness);
  Personality socialDepression;
  socialDepression.update(0, creatureMode);
  for (unsigned t = 100; t <= 1250000; t += 100) {
    socialDepression.update(t, creatureMode);
  }
  // Above 80 loneliness, the common target climbs even without music.
  assert(socialDepression.snapshot().loneliness == 100);
  assert(socialDepression.snapshot().depression >= 80);
  MotionEvent sustainedAttention;
  sustainedAttention.kind = MotionKind::Shake;
  for (uint8_t repeat = 0; repeat < 4; ++repeat) socialDepression.onMotion(sustainedAttention);
  const auto beforeAttentionRelief = socialDepression.snapshot().depression;
  socialDepression.update(1250100, creatureMode);
  // Attention above 80 pulls by the same social amount in the other direction.
  assert(socialDepression.snapshot().attention > 80);
  assert(socialDepression.snapshot().depression < beforeAttentionRelief);
  Personality noPassiveDepressionDrop;
  for (unsigned t = 0; t <= 100000; t += 100) {
    noPassiveDepressionDrop.update(t, music(t, 120));
  }
  const auto beforeMusicRecovery = noPassiveDepressionDrop.snapshot().depression;
  for (unsigned t = 100100; t <= 130000; t += 100) {
    noPassiveDepressionDrop.update(t, music(t, 160));
  }
  assert(noPassiveDepressionDrop.snapshot().depression < beforeMusicRecovery);
  const auto beforeNeutralPeriod = noPassiveDepressionDrop.snapshot().depression;
  for (unsigned t = 130100; t <= 140000; t += 100) {
    noPassiveDepressionDrop.update(t, missing);
  }
  // Below both social thresholds, absent music leaves depression untouched.
  assert(noPassiveDepressionDrop.snapshot().depression == beforeNeutralPeriod);
  Personality beatlessLowBass;
  MoodAudio beatless;
  beatless.fresh = true;
  beatless.bassValid = true;
  beatless.bassPermille = 0;
  for (unsigned t = 0; t <= 120000; t += 100) beatlessLowBass.update(t, beatless);
  // The bass contribution works without BPM or onset confidence.
  assert(beatlessLowBass.snapshot().depression >= 8);
  Personality bpm80, bpm100, bpm120;
  for (unsigned t = 0; t <= 120000; t += 100) {
    bpm80.update(t, music(t, 80));
    bpm100.update(t, music(t, 100));
    bpm120.update(t, music(t, 120));
  }
  // The ambiguous 80--100 BPM range supplies no tempo melancholy term.
  assert(bpm80.snapshot().depression == 0 && bpm100.snapshot().depression == 0);
  assert(bpm120.snapshot().depression > 0);
  MotionEvent event; event.kind = MotionKind::Tap;
  const auto energyBeforeInteraction = unknown.snapshot().energy;
  unknown.onMotion(event);
  assert(unknown.snapshot().energy == energyBeforeInteraction);
  assert(unknown.snapshot().loneliness == 92);
  event.kind = MotionKind::MultiTap;
  event.atMs = 100;
  unknown.onMotion(event);
  assert(unknown.snapshot().annoyance == 0);
  event.count = 3;
  event.atMs = 500;
  unknown.onMotion(event);
  assert(unknown.snapshot().annoyance == 6);
  event.kind = MotionKind::Impact;
  event.atMs = 1000;
  unknown.onMotion(event);
  assert(unknown.snapshot().annoyance == 21);
  event.atMs = 1500;
  unknown.onMotion(event);
  assert(unknown.snapshot().annoyance == 36);
  for (unsigned i = 1; i <= 50; ++i) {
    event.atMs = 1500 + i * 1000;
    unknown.onMotion(event);
  }
  assert(unknown.snapshot().loneliness == 0 && unknown.snapshot().annoyance == 100);
  Personality shaken;
  event.kind = MotionKind::Shake;
  event.atMs = 100;
  shaken.onMotion(event);
  assert(shaken.snapshot().annoyance == 10);
  assert(shaken.snapshot().attention == 25 && shaken.snapshot().loneliness == 45);
  Personality repositioned;
  event.kind = MotionKind::Reposition;
  event.atMs = 200;
  repositioned.onMotion(event);
  assert(repositioned.snapshot().attention == 0 && repositioned.snapshot().loneliness == 50);
  Personality attentionDecay;
  attentionDecay.update(0, missing);
  event.kind = MotionKind::Tap;
  event.atMs = 0;
  attentionDecay.onMotion(event);
  attentionDecay.update(1000, missing);
  assert(attentionDecay.snapshot().attention == 9); // returns to 0 at 3 points/s
  Personality annoyed;
  annoyed.update(0, missing);
  event.kind = MotionKind::Impact;
  event.atMs = 0;
  annoyed.onMotion(event); // impact: +15
  for (unsigned t = 100; t <= 10000; t += 100) annoyed.update(t, missing);
  assert(annoyed.snapshot().annoyance == 14);
  for (unsigned t = 10100; t <= 150000; t += 100) annoyed.update(t, missing);
  assert(annoyed.snapshot().annoyance == 0);
  Personality wrap;
  const uint32_t base = 0xffffff00U;
  wrap.update(base, silence);
  for (unsigned t = 100; t <= 2000; t += 100) wrap.update(base + t, silence);
  assert(wrap.snapshot().energy == 10);
  Personality fine, coarse;
  for (unsigned t = 0; t <= 30000; ++t) fine.update(t, silence);
  for (unsigned t = 0; t <= 30000; t += 100) coarse.update(t, silence);
  assert(fine.snapshot().energy == coarse.snapshot().energy);
  assert(fine.snapshot().depression == coarse.snapshot().depression);
  assert(fine.snapshot().loneliness == coarse.snapshot().loneliness);
  puts("PASS: BPM mapping/confidence, silence/recovery, unknown audio, bass/depression, independent loneliness");

  // Presence: attention decays at 3 points/s as always but not below a floor
  // of 20 farther away and 60 directly in front; movement only adds through
  // the Moves event (+5). Loneliness rises at 0.2 points/s when away, at
  // 0.1 points/s farther away and holds directly in front.
  MoodPresence still; // farther away, standing still
  still.present = true;
  MoodPresence dancing; // farther away, moving
  dancing.present = true;
  dancing.moving = true;
  MoodPresence close; // directly in front
  close.present = true;
  close.near = true;
  MoodPresence away;

  Personality visitor;
  visitor.update(0, missing, still);
  for (unsigned t = 100; t <= 5000; t += 100) visitor.update(t, missing, still);
  assert(visitor.snapshot().attention == 15); // rising at 3 points/s
  for (unsigned t = 5100; t <= 20000; t += 100) visitor.update(t, missing, still);
  assert(visitor.snapshot().attention == 20);
  for (unsigned t = 20100; t <= 40000; t += 100) visitor.update(t, missing, dancing);
  assert(visitor.snapshot().attention == 20); // the moving flag alone changes nothing
  for (unsigned t = 40100; t <= 60000; t += 100) visitor.update(t, missing, close);
  assert(visitor.snapshot().attention == 60); // floor 60 directly in front
  for (unsigned t = 60100; t <= 80000; t += 100) visitor.update(t, missing, still);
  assert(visitor.snapshot().attention == 20); // decays to the far floor when stepping back
  event.kind = MotionKind::Tap;
  event.atMs = 80000;
  visitor.onMotion(event); // +12 above the floor
  visitor.update(80100, missing, still);
  assert(visitor.snapshot().attention == 31); // 32 minus 0.3
  for (unsigned t = 80200; t <= 90000; t += 100) visitor.update(t, missing, still);
  assert(visitor.snapshot().attention == 20); // back to the floor, not to 0
  PresenceEvent moves;
  moves.kind = PresenceEventKind::Moves;
  visitor.onPresence(moves);
  assert(visitor.snapshot().attention == 25); // movement: +5, loneliness unchanged
  PresenceEvent standsFar;
  standsFar.kind = PresenceEventKind::StandsFar;
  const Mood beforeFar = visitor.snapshot();
  visitor.onPresence(standsFar);
  assert(visitor.snapshot().attention == beforeFar.attention); // text only
  assert(visitor.snapshot().loneliness == beforeFar.loneliness);
  for (unsigned t = 90100; t <= 120000; t += 100) visitor.update(t, missing, away);
  assert(visitor.snapshot().attention == 0);

  Personality lonely;
  lonely.update(0, missing);
  for (unsigned t = 100; t <= 600000; t += 100) lonely.update(t, missing);
  assert(lonely.snapshot().loneliness == 100);
  const Mood lonelyBefore = lonely.snapshot();
  for (unsigned t = 600100; t <= 1200000; t += 100) lonely.update(t, missing, close);
  assert(lonely.snapshot().loneliness == 100); // presence alone does not lower it
  assert(lonely.snapshot().annoyance == lonelyBefore.annoyance);
  assert(lonely.snapshot().energy == lonelyBefore.energy);
  Personality heldLoneliness; // starts at 50
  heldLoneliness.update(0, missing, close);
  for (unsigned t = 100; t <= 600000; t += 100) heldLoneliness.update(t, missing, close);
  assert(heldLoneliness.snapshot().loneliness == 50); // directly in front: holds
  for (unsigned t = 600100; t <= 610000; t += 100) heldLoneliness.update(t, missing, still);
  assert(heldLoneliness.snapshot().loneliness == 51); // farther away: 0.1 points/s
  for (unsigned t = 610100; t <= 620000; t += 100) heldLoneliness.update(t, missing, away);
  assert(heldLoneliness.snapshot().loneliness == 53); // nobody: 0.2 points/s
  for (unsigned t = 620100; t <= 1220000; t += 100) heldLoneliness.update(t, missing, still);
  assert(heldLoneliness.snapshot().loneliness == 100); // ten minutes farther away
  Personality fineLoneliness, coarseLoneliness;
  fineLoneliness.update(0, missing, away);
  coarseLoneliness.update(0, missing, away);
  for (unsigned t = 7; t <= 30000; t += 7) fineLoneliness.update(t, missing, t < 15000 ? away : still);
  for (unsigned t = 100; t <= 30000; t += 100) coarseLoneliness.update(t, missing, t < 15000 ? away : still);
  fineLoneliness.update(30000, missing, still);
  assert(fineLoneliness.snapshot().loneliness == coarseLoneliness.snapshot().loneliness);

  Personality events;
  events.update(0, missing);
  PresenceEvent presenceEvent;
  presenceEvent.kind = PresenceEventKind::ArrivedInFront;
  events.onPresence(presenceEvent);
  assert(events.snapshot().attention == 25 && events.snapshot().loneliness == 45);
  assert(events.snapshot().annoyance == 0);
  presenceEvent.kind = PresenceEventKind::WalkedPast;
  events.onPresence(presenceEvent);
  assert(events.snapshot().attention == 33 && events.snapshot().loneliness == 40);
  presenceEvent.kind = PresenceEventKind::Left;
  events.onPresence(presenceEvent);
  assert(events.snapshot().attention == 33 && events.snapshot().loneliness == 40);

  // Only "arrived in front" wakes the sleeping creature.
  Personality sleeper;
  sleeper.update(0, creatureMode);
  for (unsigned t = 100; t <= 490000; t += 100) sleeper.update(t, creatureMode);
  assert(sleeper.sleeping());
  for (unsigned t = 490100; t <= 510000; t += 100) sleeper.update(t, creatureMode);
  presenceEvent.kind = PresenceEventKind::WalkedPast;
  sleeper.onPresence(presenceEvent);
  assert(sleeper.sleeping());
  presenceEvent.kind = PresenceEventKind::ArrivedInFront;
  sleeper.onPresence(presenceEvent);
  assert(!sleeper.sleeping());
  puts("PASS: presence attention base, loneliness, presence events, waking");
}
