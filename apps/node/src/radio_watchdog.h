#pragma once

#include <stdint.h>

namespace decaflash::node {

enum class RadioAction : uint8_t {
  None = 0,
  ReinitEspNow = 1,
  RestartWifi = 2,
  Reboot = 3,
};

struct RadioPollResult {
  RadioAction action = RadioAction::None;
  // True on the first poll after a packet ended a silence that needed repairs.
  bool packetsReturned = false;
  // Length of the silence the action or the return refers to.
  uint32_t silentMs = 0;
  // Repairs of this silence so far, including the one asked for now.
  uint16_t attempt = 0;
};

// Decides when a Node repairs its radio because no valid V2 packet arrived.
// Hardware independent: the caller passes millis() and runs the action.
//
// Silence counts from begin() or from the last packet. After one silence
// period the first repair re-initialises ESP-NOW; after every further period
// the Wi-Fi driver is restarted. Once the silence reaches the reboot period a
// Node under Mainframe control is told to reboot; a Node in Demo never is, so
// a rebooted Node cannot reboot again before a packet took it out of Demo.
//
// Durations are accumulated from poll to poll and saturate, so a millis()
// overflow can neither shorten a silence nor stop the repairs.
class RadioWatchdog {
 public:
  constexpr RadioWatchdog(uint32_t silencePeriodMs, uint32_t rebootPeriodMs)
    : silencePeriodMs_(silencePeriodMs), rebootPeriodMs_(rebootPeriodMs) {}

  void begin(uint32_t now) {
    lastPollAtMs_ = now;
    silentMs_ = 0;
    sinceAttemptMs_ = 0;
    attempts_ = 0;
    packetPending_ = false;
  }

  // A valid V2 packet was received at packetAtMs. Call before poll().
  void onPacket(uint32_t packetAtMs) {
    packetPending_ = true;
    packetAtMs_ = packetAtMs;
    heard_ = true;
  }

  RadioPollResult poll(uint32_t now, bool underMainframeControl) {
    RadioPollResult result;
    const uint32_t elapsedMs = now - lastPollAtMs_;
    lastPollAtMs_ = now;
    silentMs_ = saturatingAdd(silentMs_, elapsedMs);
    sinceAttemptMs_ = saturatingAdd(sinceAttemptMs_, elapsedMs);

    if (packetPending_) {
      packetPending_ = false;
      result.packetsReturned = attempts_ > 0;
      result.silentMs = silentMs_;
      result.attempt = attempts_;
      silentMs_ = now - packetAtMs_;
      sinceAttemptMs_ = silentMs_;
      attempts_ = 0;
      return result;
    }

    result.silentMs = silentMs_;
    result.attempt = attempts_;

    if (underMainframeControl && silentMs_ >= rebootPeriodMs_) {
      result.action = RadioAction::Reboot;
      return result;
    }

    if (sinceAttemptMs_ >= silencePeriodMs_) {
      sinceAttemptMs_ = 0;
      if (attempts_ < UINT16_MAX) ++attempts_;
      if (repairs_ < UINT32_MAX) ++repairs_;
      result.attempt = attempts_;
      result.action = (attempts_ == 1) ? RadioAction::ReinitEspNow : RadioAction::RestartWifi;
    }
    return result;
  }

  // Whether any valid packet arrived since boot.
  bool heard() const { return heard_; }
  // Time since the last packet, or since begin() if none arrived yet.
  uint32_t silentMs() const { return silentMs_; }
  // Repairs since boot.
  uint32_t repairs() const { return repairs_; }

 private:
  static uint32_t saturatingAdd(uint32_t value, uint32_t delta) {
    return (UINT32_MAX - value < delta) ? UINT32_MAX : value + delta;
  }

  uint32_t silencePeriodMs_;
  uint32_t rebootPeriodMs_;
  uint32_t lastPollAtMs_ = 0;
  uint32_t silentMs_ = 0;
  uint32_t sinceAttemptMs_ = 0;
  uint32_t packetAtMs_ = 0;
  uint32_t repairs_ = 0;
  uint16_t attempts_ = 0;
  bool packetPending_ = false;
  bool heard_ = false;
};

}  // namespace decaflash::node
