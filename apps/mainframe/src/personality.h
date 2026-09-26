#pragma once
#include <cstdint>
#include "motion_events.h"

namespace decaflash::mainframe {
struct Mood {
  uint8_t energy = 30, annoyance = 0, attention = 10, loneliness = 0, depression = 10;
};

struct MoodAudio {
  bool fresh = false, silent = false, bassValid = false;
  uint16_t bpm = 0, bassPermille = 0;
  uint8_t confidence = 0;
  uint32_t onsetAtMs = 0;
};

// Local, deterministic state. Values use milli-points internally.
class Personality {
 public:
  void update(uint32_t now, const MoodAudio& audio);
  void onMotion(const MotionEvent& event);
  Mood snapshot() const;
 private:
  int32_t energy_ = 30000, annoyance_ = 0, attention_ = 10000, depression_ = 10000, loneliness_ = 100000;
  uint32_t lastUpdate_ = 0, quietSince_ = 0, lastOnset_ = 0, tempoAt_ = 0;
  uint16_t candidateBpm_ = 0, trustedBpm_ = 0;
  uint8_t candidateCount_ = 0;
  uint8_t depressionRemainder_ = 0, lonelinessRemainder_ = 0, annoyanceRemainder_ = 0;
  bool started_ = false, quietPending_ = false, haveTempo_ = false, haveOnset_ = false;
};
}  // namespace decaflash::mainframe
