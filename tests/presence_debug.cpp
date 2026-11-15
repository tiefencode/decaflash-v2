#include "presence_debug.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace decaflash::mainframe;

namespace {

PresenceSample sample(uint32_t atMs, int16_t presence, int16_t motion, bool presenceFlag = false,
                      bool motionFlag = false) {
  PresenceSample result;
  result.valid = true;
  result.atMs = atMs;
  result.presence = presence;
  result.motion = motion;
  result.presenceFlag = presenceFlag;
  result.motionFlag = motionFlag;
  return result;
}

void testHistory() {
  PresenceDebug debug;
  assert(!debug.haveSample());
  assert(debug.flagAgeMs(PresenceFlag::Presence, 1000) == PresenceDebug::kUnknownAge);

  PresenceSample invalid = sample(10, 999, 999);
  invalid.valid = false;
  debug.add(invalid);
  assert(debug.count() == 0);

  for (int i = 0; i < 130; ++i) debug.add(sample(i * 125U, static_cast<int16_t>(i), static_cast<int16_t>(-i)));
  assert(debug.count() == PresenceDebug::kHistorySize);
  // Oldest kept entry is sample 10, newest is sample 129.
  assert(debug.valueAt(PresenceSeries::Presence, 0) == 10);
  assert(debug.valueAt(PresenceSeries::Presence, PresenceDebug::kHistorySize - 1) == 129);
  assert(debug.valueAt(PresenceSeries::Motion, PresenceDebug::kHistorySize - 1) == -129);
  assert(debug.latest().presence == 129);

  // Invalid samples never enter the history.
  debug.add(invalid);
  assert(debug.latest().presence == 129);
  assert(debug.valueAt(PresenceSeries::Presence, PresenceDebug::kHistorySize - 1) == 129);
}

void testFlagTiming() {
  PresenceDebug debug;
  debug.add(sample(1000, 0, 0));
  assert(!debug.flag(PresenceFlag::Presence));
  assert(debug.flagAgeMs(PresenceFlag::Presence, 4000) == 3000);
  debug.add(sample(5000, 300, 0, true));
  assert(debug.flag(PresenceFlag::Presence));
  assert(debug.flagAgeMs(PresenceFlag::Presence, 5000) == 0);
  // Motion did not change, so its age keeps counting from the first sample.
  assert(debug.flagAgeMs(PresenceFlag::Motion, 6000) == 5000);
  debug.add(sample(6000, 310, 0, true));
  assert(debug.flagAgeMs(PresenceFlag::Presence, 7000) == 2000);

  // millis() overflow.
  PresenceDebug wrap;
  wrap.add(sample(0xFFFFFF00U, 0, 0, false, true));
  assert(wrap.flagAgeMs(PresenceFlag::Motion, 0x00000100U) == 0x200U);
}

void testStateReset() {
  PresenceDebug debug;
  debug.setState(PresenceSensorState::Ok, 0);
  PresenceConfig config;
  config.presenceThreshold = 200;
  debug.setConfig(config);
  debug.add(sample(100, 50, 5, true));
  debug.setState(PresenceSensorState::Error, 1);
  assert(debug.haveSample());
  assert(debug.errors() == 1);
  debug.setState(PresenceSensorState::Missing, 8);
  assert(!debug.haveSample());
  assert(!debug.haveConfig());
  assert(debug.flagAgeMs(PresenceFlag::Presence, 200) == PresenceDebug::kUnknownAge);
  assert(debug.errors() == 8);
}

void testRange() {
  PresenceDebug debug;
  PresenceGraphRange empty = debug.range(PresenceSeries::Presence, 200);
  assert(empty.low == 0 && empty.high == 200);
  debug.add(sample(0, 50, -40));
  debug.add(sample(125, 420, 10));
  PresenceGraphRange presence = debug.range(PresenceSeries::Presence, 200);
  assert(presence.low == 0 && presence.high == 420);
  PresenceGraphRange motion = debug.range(PresenceSeries::Motion, 200);
  assert(motion.low == -40 && motion.high == 200);
  PresenceDebug flat;
  PresenceGraphRange both = debug.range(PresenceSeries::Presence, 200, 900);
  assert(both.low == 0 && both.high == 900);
  PresenceGraphRange zero = flat.range(PresenceSeries::Motion, 0);
  assert(zero.high > zero.low);
}

void testLines() {
  char line[200];
  PresenceSample s = sample(81234, 412, 37, true, false);
  s.shock = 2;
  s.objectRaw = 1840;
  s.objectComp = 1835;
  s.ambientCentiC = 2341;
  size_t length = formatPresenceSampleLine(line, sizeof(line), s, 0, PersonState::Still);
  const char* expected =
    "TMOS t=81234 presence_flag=1 motion_flag=0 shock_flag=0 presence=412 motion=37 shock=2 "
    "object=1840 object_comp=1835 ambient=23.4 errors=0 person=still\n";
  assert(std::strcmp(line, expected) == 0);
  assert(length == std::strlen(expected));

  s = sample(5, -120, -7, false, true);
  s.shockFlag = true;
  s.shock = -3;
  s.objectRaw = -15;
  s.objectComp = -16;
  s.ambientCentiC = -5;
  formatPresenceSampleLine(line, sizeof(line), s, 12, PersonState::Moving);
  assert(std::strcmp(line,
                     "TMOS t=5 presence_flag=0 motion_flag=1 shock_flag=1 presence=-120 "
                     "motion=-7 shock=-3 object=-15 object_comp=-16 ambient=-0.1 errors=12 person=moving\n") == 0);

  assert(formatPresenceSampleLine(line, 20, s, 0, PersonState::Away) == 0);
  formatPresenceSampleLine(line, sizeof(line), s, 0, PersonState::Away);
  assert(std::strstr(line, " person=away\n") != nullptr);

  PresenceConfig config;
  config.odrHz = 8;
  config.avgTmos = 32;
  config.avgT = 8;
  config.presenceThreshold = 200;
  config.presenceHysteresis = 32;
  config.motionThreshold = 200;
  config.motionHysteresis = 32;
  config.shockThreshold = 10;
  config.shockHysteresis = 2;
  config.objectSensitivity = 2000;
  formatPresenceConfigLine(line, sizeof(line), config);
  assert(std::strcmp(line,
                     "TMOS_CONFIG odr_hz=8 avg_tmos=32 avg_t=8 presence_ths=200 presence_hyst=32 "
                     "motion_ths=200 motion_hyst=32 shock_ths=10 shock_hyst=2 sensitivity=2000\n") == 0);

  PresenceEvent event;
  event.kind = PresenceEventKind::WalkedPast;
  event.atMs = 81234;
  formatPresenceEventLine(line, sizeof(line), event);
  assert(std::strcmp(line, "TMOS_EVENT t=81234 kind=walked_past\n") == 0);
  event.kind = PresenceEventKind::ArrivedInFront;
  formatPresenceEventLine(line, sizeof(line), event);
  assert(std::strcmp(line, "TMOS_EVENT t=81234 kind=arrived_in_front\n") == 0);
  assert(std::strcmp(presenceEventName(PresenceEventKind::Left), "left") == 0);

  formatPresenceStateLine(line, sizeof(line), false, 8);
  assert(std::strcmp(line, "TMOS_STATE sensor=missing errors=8\n") == 0);
  formatPresenceStateLine(line, sizeof(line), true, 8);
  assert(std::strcmp(line, "TMOS_STATE sensor=ok errors=8\n") == 0);
}

void testSmallFormats() {
  char text[16];
  formatCentiCelsius(text, sizeof(text), 0);
  assert(std::strcmp(text, "0.0") == 0);
  formatCentiCelsius(text, sizeof(text), 2345);
  assert(std::strcmp(text, "23.5") == 0);
  formatCentiCelsius(text, sizeof(text), -1234);
  assert(std::strcmp(text, "-12.3") == 0);
  formatCentiCelsius(text, sizeof(text), -4);
  assert(std::strcmp(text, "0.0") == 0);

  formatHeatCelsius(text, sizeof(text), 988, 2000);
  assert(std::strcmp(text, "+0.49") == 0);
  formatHeatCelsius(text, sizeof(text), 990, 2000);
  assert(std::strcmp(text, "+0.50") == 0);
  formatHeatCelsius(text, sizeof(text), -281, 2000);
  assert(std::strcmp(text, "-0.14") == 0);
  formatHeatCelsius(text, sizeof(text), -5, 2000);
  assert(std::strcmp(text, "+0.00") == 0);
  formatHeatCelsius(text, sizeof(text), 32767, 2048);
  assert(std::strcmp(text, "+16.00") == 0);
  formatHeatCelsius(text, sizeof(text), 1234, 0);
  assert(std::strcmp(text, "--") == 0);

  formatPresenceAge(text, sizeof(text), PresenceDebug::kUnknownAge);
  assert(std::strcmp(text, "--") == 0);
  formatPresenceAge(text, sizeof(text), 42999);
  assert(std::strcmp(text, "42s") == 0);
  formatPresenceAge(text, sizeof(text), 99999);
  assert(std::strcmp(text, "99s") == 0);
  formatPresenceAge(text, sizeof(text), 100000);
  assert(std::strcmp(text, "1m") == 0);
  formatPresenceAge(text, sizeof(text), 3600000UL * 30UL);
  assert(std::strcmp(text, "999m") == 0);
}

}  // namespace

int main() {
  testHistory();
  testFlagTiming();
  testStateReset();
  testRange();
  testLines();
  testSmallFormats();
  std::puts("presence_debug: all tests passed");
  return 0;
}
