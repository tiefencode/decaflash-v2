#include "text_bank.h"

// The firmware holds one bank, chosen at build time (add-verse-text-bank): the
// verse bank by default, the classic bank with DECAFLASH_TEXT_BANK_CLASSIC=1.
#if defined(DECAFLASH_TEXT_BANK_CLASSIC) && DECAFLASH_TEXT_BANK_CLASSIC
#include "text_bank_data.h"
namespace decaflash::mainframe {
namespace built_in_bank = text_bank_data;
}  // namespace decaflash::mainframe
#else
#include "text_bank_verse_data.h"
namespace decaflash::mainframe {
namespace built_in_bank = text_bank_verse_data;
}  // namespace decaflash::mainframe
#endif

namespace decaflash::mainframe {
namespace {

TextBank::Table builtInTable() {
  return {built_in_bank::kGroupStart, built_in_bank::kLines, built_in_bank::kLineCount,
          built_in_bank::kEvents};
}

}  // namespace

TextBank::TextBank(Random random, void* context) : TextBank(builtInTable(), random, context) {}

TextBank::TextBank(const Table& table, Random random, void* context)
    : table_(table), random_(random), context_(context) {
  groupOrder_ = new uint16_t[table_.lineCount ? table_.lineCount : 1];
  eventOrder_ = new uint16_t[table_.lineCount ? table_.lineCount : 1];
  for (uint16_t i = 0; i < table_.lineCount; ++i) groupOrder_[i] = eventOrder_[i] = i;
  groupCursor_ = new uint16_t[table_.events * kLeads]();
  eventCursor_ = new uint16_t[table_.events]();
}

TextBank::~TextBank() {
  delete[] groupOrder_;
  delete[] eventOrder_;
  delete[] groupCursor_;
  delete[] eventCursor_;
}

uint8_t TextBank::leadFor(const Mood& mood) {
  const uint8_t values[5] = {mood.energy, mood.annoyance, mood.attention, mood.loneliness,
                             mood.depression};
  uint8_t best[5];
  uint8_t count = 0;
  int bestDistance = -1;
  for (uint8_t v = 0; v < 5; ++v) {
    const int distance = values[v] < 50 ? 50 - values[v] : values[v] - 50;
    if (distance > bestDistance) {
      bestDistance = distance;
      count = 0;
    }
    if (distance == bestDistance) best[count++] = v;
  }
  const uint8_t value = count > 1 ? best[random_(context_) % count] : best[0];
  return static_cast<uint8_t>(value * 2 + (values[value] < 50 ? 0 : 1));
}

uint16_t TextBank::groupSize(uint8_t event, uint8_t lead) const {
  if (event >= table_.events || lead >= kLeads) return 0;
  const uint16_t g = static_cast<uint16_t>(event * kLeads + lead);
  return static_cast<uint16_t>(table_.groupStart[g + 1] - table_.groupStart[g]);
}

uint16_t TextBank::eventSize(uint8_t event) const {
  if (event >= table_.events) return 0;
  return static_cast<uint16_t>(table_.groupStart[(event + 1) * kLeads] -
                               table_.groupStart[event * kLeads]);
}

uint16_t TextBank::nextInRange(uint16_t* order, uint16_t& cursor, uint16_t begin,
                               uint16_t end) {
  const uint16_t size = static_cast<uint16_t>(end - begin);
  if (cursor == 0) {  // start of a round: shuffle the range (Fisher-Yates)
    for (uint16_t i = static_cast<uint16_t>(size - 1); i > 0; --i) {
      const uint16_t j = static_cast<uint16_t>(random_(context_) % (i + 1u));
      const uint16_t swap = order[begin + i];
      order[begin + i] = order[begin + j];
      order[begin + j] = swap;
    }
  }
  const uint16_t line = order[begin + cursor];
  cursor = static_cast<uint16_t>((cursor + 1) % size);
  return line;
}

const char* TextBank::pick(uint8_t event, uint8_t lead) {
  if (event >= table_.events || lead >= kLeads) return nullptr;
  const uint16_t g = static_cast<uint16_t>(event * kLeads + lead);
  if (groupSize(event, lead)) {
    return table_.lines[nextInRange(groupOrder_, groupCursor_[g], table_.groupStart[g],
                                    table_.groupStart[g + 1])];
  }
  if (!eventSize(event)) return nullptr;
  return table_.lines[nextInRange(eventOrder_, eventCursor_[event],
                                  table_.groupStart[event * kLeads],
                                  table_.groupStart[(event + 1) * kLeads])];
}

const char* TextBank::leadName(uint8_t lead) {
  static constexpr const char* kNames[kLeads] = {
    "ENERGY_LOW", "ENERGY_HIGH", "ANNOYANCE_LOW", "ANNOYANCE_HIGH", "ATTENTION_LOW",
    "ATTENTION_HIGH", "LONELINESS_LOW", "LONELINESS_HIGH", "DEPRESSION_LOW",
    "DEPRESSION_HIGH"};
  return lead < kLeads ? kNames[lead] : "?";
}

}  // namespace decaflash::mainframe
