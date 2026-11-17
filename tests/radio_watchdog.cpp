#include <cassert>
#include <vector>

#include "radio_watchdog.h"

using decaflash::node::RadioAction;
using decaflash::node::RadioPollResult;
using decaflash::node::RadioWatchdog;

namespace {

constexpr uint32_t kSilenceMs = 30000;
constexpr uint32_t kRebootMs = 300000;
constexpr uint32_t kStepMs = 100;

struct Event {
  uint32_t atMs;  // relative to the start of the run
  RadioAction action;
  uint16_t attempt;
};

// Polls every 100 ms for durationMs and collects every action.
std::vector<Event> run(RadioWatchdog& watchdog, uint32_t& now, uint32_t durationMs,
                       bool underControl) {
  std::vector<Event> events;
  for (uint32_t t = kStepMs; t <= durationMs; t += kStepMs) {
    now += kStepMs;
    const RadioPollResult result = watchdog.poll(now, underControl);
    if (result.action != RadioAction::None) {
      events.push_back({t, result.action, result.attempt});
    }
  }
  return events;
}

void testStagesAndRepeat(uint32_t start) {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = start;
  watchdog.begin(now);

  // Mainframe never heard, Node in Demo: repairs all the same, never a reboot.
  const auto events = run(watchdog, now, 12 * 60000, false);
  assert(events.size() == 24);
  assert(events[0].atMs == 30000);
  assert(events[0].action == RadioAction::ReinitEspNow);
  assert(events[0].attempt == 1);
  for (size_t i = 1; i < events.size(); ++i) {
    assert(events[i].atMs == (i + 1) * 30000);
    assert(events[i].action == RadioAction::RestartWifi);
    assert(events[i].attempt == i + 1);
  }
  assert(!watchdog.heard());
  assert(watchdog.repairs() == 24);
}

void testPacketsKeepItQuiet() {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = 1000;
  watchdog.begin(now);

  // A packet every 3 s, as the Mainframe sends them: no action for an hour.
  for (uint32_t t = 0; t < 3600000; t += kStepMs) {
    now += kStepMs;
    if (t % 3000 == 0) watchdog.onPacket(now);
    const RadioPollResult result = watchdog.poll(now, true);
    assert(result.action == RadioAction::None);
    assert(!result.packetsReturned);
  }
  assert(watchdog.heard());
  assert(watchdog.repairs() == 0);
}

void testPacketsReturnAndSequenceRestarts() {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = 5000;
  watchdog.begin(now);
  watchdog.onPacket(now);
  assert(watchdog.poll(now, true).action == RadioAction::None);

  // 100 s of silence: one ESP-NOW re-initialisation, two Wi-Fi restarts.
  auto events = run(watchdog, now, 100000, true);
  assert(events.size() == 3);
  assert(events[0].action == RadioAction::ReinitEspNow);
  assert(events[1].action == RadioAction::RestartWifi);
  assert(events[2].action == RadioAction::RestartWifi);

  // The packet is reported once, with the silence and the repairs it ended.
  now += kStepMs;
  watchdog.onPacket(now);
  RadioPollResult result = watchdog.poll(now, true);
  assert(result.action == RadioAction::None);
  assert(result.packetsReturned);
  assert(result.attempt == 3);
  assert(result.silentMs == 100100);
  now += kStepMs;
  assert(!watchdog.poll(now, true).packetsReturned);

  // The next silence starts again with the ESP-NOW re-initialisation.
  events = run(watchdog, now, 61000, true);
  assert(events.size() == 2);
  assert(events[0].atMs == 29900);
  assert(events[0].action == RadioAction::ReinitEspNow);
  assert(events[0].attempt == 1);
  assert(events[1].action == RadioAction::RestartWifi);
}

void testPacketWithoutRepairIsNotAReturn() {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = 0;
  watchdog.begin(now);
  run(watchdog, now, 20000, true);
  watchdog.onPacket(now);
  assert(!watchdog.poll(now, true).packetsReturned);
}

void testRebootOnlyUnderMainframeControl(uint32_t start) {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = start;
  watchdog.begin(now);
  watchdog.onPacket(now);
  watchdog.poll(now, true);

  // Under Mainframe control: nine repairs, then reboot at five minutes.
  const auto events = run(watchdog, now, kRebootMs, true);
  assert(events.size() == 10);
  for (size_t i = 0; i < 9; ++i) assert(events[i].action != RadioAction::Reboot);
  assert(events[9].atMs == kRebootMs);
  assert(events[9].action == RadioAction::Reboot);

  // The caller did not reboot (cancelled): the request stays until a packet.
  now += kStepMs;
  assert(watchdog.poll(now, true).action == RadioAction::Reboot);

  // A Node in Demo is never told to reboot, however long the silence.
  now += kStepMs;
  const auto demo = run(watchdog, now, 30 * 60000, false);
  for (const auto& event : demo) assert(event.action == RadioAction::RestartWifi);
  assert(!demo.empty());
}

void testSinglePacketRestartsRebootPeriod() {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = 0;
  watchdog.begin(now);
  watchdog.onPacket(now);
  watchdog.poll(now, true);

  // 4 min 50 s of silence, then one packet.
  auto events = run(watchdog, now, 290000, true);
  for (const auto& event : events) assert(event.action != RadioAction::Reboot);
  watchdog.onPacket(now);
  assert(watchdog.poll(now, true).packetsReturned);

  // No reboot for the next 4 min 59 s; reboot five minutes after the packet.
  events = run(watchdog, now, kRebootMs - kStepMs, true);
  for (const auto& event : events) assert(event.action != RadioAction::Reboot);
  now += kStepMs;
  assert(watchdog.poll(now, true).action == RadioAction::Reboot);
}

void testPacketAtTheLastMomentCancelsReboot() {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = 0;
  watchdog.begin(now);
  watchdog.onPacket(now);
  watchdog.poll(now, true);
  run(watchdog, now, kRebootMs - kStepMs, true);

  now += kStepMs;
  assert(watchdog.poll(now, true).action == RadioAction::Reboot);
  // The caller finds a fresh packet in its last check and feeds it.
  watchdog.onPacket(now);
  now += kStepMs;
  const RadioPollResult result = watchdog.poll(now, true);
  assert(result.action == RadioAction::None);
  assert(result.packetsReturned);
  assert(watchdog.silentMs() == kStepMs);
}

void testSilenceSaturates() {
  RadioWatchdog watchdog(kSilenceMs, kRebootMs);
  uint32_t now = 0;
  watchdog.begin(now);
  // More than 49.7 days in Demo without a packet: polled in steps of a day.
  uint32_t repairs = 0;
  for (int day = 0; day < 120; ++day) {
    now += 86400000UL;
    if (watchdog.poll(now, false).action != RadioAction::None) ++repairs;
  }
  assert(repairs == 120);
  assert(watchdog.silentMs() == UINT32_MAX);
  // A long silence that wrapped millis() still leads to the reboot.
  assert(watchdog.poll(now, true).action == RadioAction::Reboot);
}

}  // namespace

int main() {
  testStagesAndRepeat(0);
  // millis() overflows during the silence.
  testStagesAndRepeat(UINT32_MAX - 45000);
  testPacketsKeepItQuiet();
  testPacketsReturnAndSequenceRestarts();
  testPacketWithoutRepairIsNotAReturn();
  testRebootOnlyUnderMainframeControl(0);
  testRebootOnlyUnderMainframeControl(UINT32_MAX - 120000);
  testSinglePacketRestartsRebootPeriod();
  testPacketAtTheLastMomentCancelsReboot();
  testSilenceSaturates();
  return 0;
}
