#pragma once

#include <cstdint>
#include "presence_debug.h"

namespace decaflash::mainframe {

// Unit TMOS PIR (U185, STHS34PF80) on the AtomS3R Grove port, read through
// M5Unified's Ex_I2C so I2C controller 0 has a single driver. Polled from
// the main loop; a missing or failing sensor never blocks anything else.
class PresenceSensor {
 public:
  void begin(uint32_t now);
  void service(uint32_t now);

  PresenceSensorState state() const { return state_; }
  uint32_t readErrors() const { return readErrors_; }

  // Each take* returns true once per new event and clears it.
  bool takeSample(PresenceSample& sample);
  bool takeStateChange(bool& sensorOk);
  bool takeConfig(PresenceConfig& config);

 private:
  bool configure();
  bool readSample(uint32_t now, PresenceSample& sample);
  void enterMissing(uint32_t now);
  void readFailed(uint32_t now);

  PresenceSample sample_;
  PresenceConfig config_;
  uint32_t nextPollAtMs_ = 0;
  uint32_t nextProbeAtMs_ = 0;
  uint32_t readErrors_ = 0;
  uint8_t consecutiveFailures_ = 0;
  PresenceSensorState state_ = PresenceSensorState::Missing;
  bool busReady_ = false;
  bool sampleReady_ = false;
  bool stateChanged_ = false;
  bool configReady_ = false;
};

}  // namespace decaflash::mainframe
