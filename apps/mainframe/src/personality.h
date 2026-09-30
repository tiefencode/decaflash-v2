#pragma once
#include <cstdint>
#include "motion_events.h"

namespace decaflash::mainframe {
struct Mood {
  uint8_t energy = 30, annoyance = 0, attention = 0, loneliness = 50, depression = 0;
};

struct MoodAudio {
  bool fresh = false, silent = false, bassValid = false;
  // Before the first analysis-button press, the energy meter represents the
  // creature's own stamina instead of an inferred music tempo.
  bool creatureMode = false;
  uint16_t bpm = 0, bassPermille = 0;
  uint8_t confidence = 0;
  uint32_t onsetAtMs = 0;
};

// Local, deterministic state. Values use milli-points internally.
class Personality {
 public:
  void update(uint32_t now, const MoodAudio& audio);
  void onMotion(const MotionEvent& event);
  bool sleeping() const { return sleeping_; }
  // Advances the deterministic, occasional sleep-noise schedule only when a
  // snore can actually be played. There is deliberately no queued snore.
  bool consumeSnore(uint32_t now);
  Mood snapshot() const;
  // The debug readout rounds internal fixed-point values for human-readable
  // threshold inspection; control logic keeps using snapshot().
  Mood debugSnapshot() const;
 private:
  int32_t energy_ = 30000, annoyance_ = 0, attention_ = 0, depression_ = 0, loneliness_ = 50000;
  uint32_t lastUpdate_ = 0, quietSince_ = 0, lastOnset_ = 0, tempoAt_ = 0;
  uint32_t lowEnergySince_ = 0, nextSnoreAt_ = 0, snoreRandom_ = 0x51EE9U;
  uint16_t candidateBpm_ = 0, trustedBpm_ = 0;
  uint8_t candidateCount_ = 0;
  int16_t depressionRateRemainder_ = 0;
  uint8_t lonelinessRemainder_ = 0, annoyanceRemainder_ = 0;
  uint8_t creatureDrainRemainder_ = 0, creatureRechargeRemainder_ = 0;
  bool started_ = false, quietPending_ = false, haveTempo_ = false, haveOnset_ = false;
  bool creatureMode_ = false, sleeping_ = false;
};
}  // namespace decaflash::mainframe
