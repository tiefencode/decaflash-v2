#pragma once

#include <cstdint>

#include "personality.h"

namespace decaflash::mainframe {

// Text bank (add-text-bank): picks a reaction text for an event and the lead
// mood value from the lines built into the firmware. No hardware dependencies;
// the random source is passed in.
class TextBank {
 public:
  using Random = uint32_t (*)(void* context);

  // A bank over a table: lines of group g (= event * kLeads + lead) are
  // lines[groupStart[g] .. groupStart[g + 1]).
  struct Table {
    const uint16_t* groupStart;
    const char* const* lines;
    uint16_t lineCount;
    uint8_t events;
  };
  static constexpr uint8_t kLeads = 10;  // value * 2 + (high ? 1 : 0), Mood order

  TextBank(Random random, void* context);           // the built-in table
  TextBank(const Table& table, Random random, void* context);
  ~TextBank();
  TextBank(const TextBank&) = delete;
  TextBank& operator=(const TextBank&) = delete;

  // The mood value farthest from 50, at its low side below 50 and its high
  // side otherwise; equal distances are broken at random.
  uint8_t leadFor(const Mood& mood);
  // The next line of the round for this event and lead value; a group without
  // lines falls back to the event's whole range. nullptr if the event has none.
  const char* pick(uint8_t event, uint8_t lead);
  uint16_t groupSize(uint8_t event, uint8_t lead) const;
  uint16_t eventSize(uint8_t event) const;
  static const char* leadName(uint8_t lead);  // e.g. "ANNOYANCE_HIGH"

 private:
  uint16_t nextInRange(uint16_t* order, uint16_t& cursor, uint16_t begin, uint16_t end);
  Table table_;
  Random random_;
  void* context_;
  uint16_t* groupOrder_ = nullptr;   // one permutation per group, at its range
  uint16_t* eventOrder_ = nullptr;   // one permutation per event, for the fallback
  uint16_t* groupCursor_ = nullptr;
  uint16_t* eventCursor_ = nullptr;
};

}  // namespace decaflash::mainframe
