#include "presence_sensor.h"

#include <M5Unified.h>
#include <SparkFun_STHS34PF80_Arduino_Library.h>

namespace decaflash::mainframe {
namespace {

constexpr uint8_t kAddress = 0x5A;
constexpr uint32_t kBusHz = 100000;
// Calibration values from the change design; see design.md before tuning.
constexpr uint32_t kPollIntervalMs = 25;
constexpr uint32_t kProbeIntervalMs = 5000;
constexpr uint8_t kFailuresUntilMissing = 8;
constexpr sths34pf80_tmos_odr_t kOdr = STHS34PF80_TMOS_ODR_AT_8Hz;

// SparkFun's base class talks to the chip through these callbacks; routing
// them through Ex_I2C avoids a second (Arduino Wire) driver on controller 0.
class GroveSths34pf80 : public STHS34PF80 {
 public:
  void attach() {
    sensor.read_reg = read;
    sensor.write_reg = write;
    sensor.mdelay = pause;
    sensor.handle = this;
  }
  int32_t objectSensitivity(uint16_t* value) {
    return sths34pf80_tmos_sensitivity_get(&sensor, value);
  }
  int32_t readBlock(uint8_t reg, uint8_t* data, uint16_t length) {
    return sths34pf80_read_reg(&sensor, reg, data, length);
  }

 private:
  static int32_t read(void*, uint8_t reg, uint8_t* data, uint16_t length) {
    return M5.Ex_I2C.readRegister(kAddress, reg, data, length, kBusHz) ? 0 : -1;
  }
  static int32_t write(void*, uint8_t reg, const uint8_t* data, uint16_t length) {
    return M5.Ex_I2C.writeRegister(kAddress, reg, data, length, kBusHz) ? 0 : -1;
  }
  static void pause(uint32_t ms) { delay(ms); }
};

GroveSths34pf80 chip;

int16_t littleEndian(const uint8_t* bytes) {
  return static_cast<int16_t>(static_cast<uint16_t>(bytes[0]) |
                              (static_cast<uint16_t>(bytes[1]) << 8));
}

uint8_t odrHz(sths34pf80_tmos_odr_t odr) {
  switch (odr) {
    case STHS34PF80_TMOS_ODR_AT_1Hz: return 1;
    case STHS34PF80_TMOS_ODR_AT_2Hz: return 2;
    case STHS34PF80_TMOS_ODR_AT_4Hz: return 4;
    case STHS34PF80_TMOS_ODR_AT_8Hz: return 8;
    case STHS34PF80_TMOS_ODR_AT_15Hz: return 15;
    case STHS34PF80_TMOS_ODR_AT_30Hz: return 30;
    default: return 0;  // off or below 1 Hz
  }
}

uint16_t averageCount(sths34pf80_avg_tobject_num_t value) {
  static constexpr uint16_t kCounts[] = {2, 8, 32, 128, 256, 512, 1024, 2048};
  const auto index = static_cast<uint8_t>(value);
  return index < 8 ? kCounts[index] : 0;
}

uint8_t averageCount(sths34pf80_avg_tambient_num_t value) {
  static constexpr uint8_t kCounts[] = {8, 4, 2, 1};
  const auto index = static_cast<uint8_t>(value);
  return index < 4 ? kCounts[index] : 0;
}

}  // namespace

void PresenceSensor::begin(uint32_t now) {
  chip.attach();
  // M5Unified assigns Ex_I2C to the Grove pins but does not start it.
  busReady_ = M5.Ex_I2C.isEnabled() && M5.Ex_I2C.begin();
  nextProbeAtMs_ = now;
  stateChanged_ = true;  // report the first probe result either way
  service(now);
}

bool PresenceSensor::configure() {
  // Own sequence instead of SparkFun's begin(): that one starts the sensor at
  // 1 Hz, and every later embedded-register access (threshold read-back)
  // would then power the sensor down and reset its algorithm again. Here the
  // sensor stays powered down (ODR 0 after reboot) until the very end.
  if (chip.isConnected() != 0) return false;
  if (chip.reset() != 0) return false;  // reboot memory, reset algorithm
  if (chip.setAverageTObjectNumber(STHS34PF80_AVG_TMOS_32) != 0) return false;
  if (chip.setAverageTAmbientNumber(STHS34PF80_AVG_T_8) != 0) return false;
  if (chip.setBlockDataUpdate(true) != 0) return false;

  uint16_t presenceThs = 0, motionThs = 0, shockThs = 0;
  uint8_t presenceHyst = 0, motionHyst = 0, shockHyst = 0;
  if (chip.getPresenceThreshold(&presenceThs) != 0 ||
      chip.getPresenceHysteresis(&presenceHyst) != 0 ||
      chip.getMotionThreshold(&motionThs) != 0 ||
      chip.getMotionHysteresis(&motionHyst) != 0 ||
      chip.getTAmbientShockThreshold(&shockThs) != 0 ||
      chip.getTAmbientShockHysteresis(&shockHyst) != 0) {
    return false;
  }

  // Starting from power-down this writes ODR 0, resets the algorithm once
  // and sets the rate; it has no busy wait.
  if (chip.setTmosODR(kOdr) != 0) return false;

  sths34pf80_tmos_odr_t odr;
  sths34pf80_avg_tobject_num_t avgTmos;
  sths34pf80_avg_tambient_num_t avgT;
  uint16_t sensitivity = 0;
  if (chip.getTmosODR(&odr) != 0 || chip.getAverageTObjectNumber(&avgTmos) != 0 ||
      chip.getAverageTAmbientNumber(&avgT) != 0 || chip.objectSensitivity(&sensitivity) != 0) {
    return false;
  }
  PresenceConfig config;
  config.odrHz = odrHz(odr);
  config.avgTmos = averageCount(avgTmos);
  config.avgT = averageCount(avgT);
  config.presenceThreshold = presenceThs;
  config.presenceHysteresis = presenceHyst;
  config.motionThreshold = motionThs;
  config.motionHysteresis = motionHyst;
  config.shockThreshold = shockThs;
  config.shockHysteresis = shockHyst;
  config.objectSensitivity = sensitivity;
  config_ = config;
  configReady_ = true;
  return true;
}

void PresenceSensor::enterMissing(uint32_t now) {
  if (state_ != PresenceSensorState::Missing) stateChanged_ = true;
  state_ = PresenceSensorState::Missing;
  consecutiveFailures_ = 0;
  nextProbeAtMs_ = now + kProbeIntervalMs;
}

void PresenceSensor::readFailed(uint32_t now) {
  ++readErrors_;
  sample_ = PresenceSample{};
  sample_.atMs = now;
  sampleReady_ = true;  // an invalid sample; the debug view keeps it out
  state_ = PresenceSensorState::Error;
  if (++consecutiveFailures_ >= kFailuresUntilMissing) enterMissing(now);
}

bool PresenceSensor::readSample(uint32_t now, PresenceSample& sample) {
  sths34pf80_tmos_func_status_t status;
  uint8_t objectAmbient[4];
  uint8_t algorithms[8];
  // FUNC_STATUS first: reading it clears data-ready. Block data update keeps
  // the following outputs from the same conversion.
  if (chip.getTmosFunctionStatus(&status) != 0) return false;
  if (chip.readBlock(STHS34PF80_TOBJECT_L, objectAmbient, sizeof(objectAmbient)) != 0) return false;
  if (chip.readBlock(STHS34PF80_TOBJ_COMP_L, algorithms, sizeof(algorithms)) != 0) return false;
  sample = PresenceSample{};
  sample.valid = true;
  sample.atMs = now;
  sample.presenceFlag = status.pres_flag != 0;
  sample.motionFlag = status.mot_flag != 0;
  sample.shockFlag = status.tamb_shock_flag != 0;
  sample.objectRaw = littleEndian(&objectAmbient[0]);
  sample.ambientCentiC = littleEndian(&objectAmbient[2]);
  sample.objectComp = littleEndian(&algorithms[0]);
  sample.presence = littleEndian(&algorithms[2]);
  sample.motion = littleEndian(&algorithms[4]);
  sample.shock = littleEndian(&algorithms[6]);
  return true;
}

void PresenceSensor::service(uint32_t now) {
  if (!busReady_) {
    if (state_ != PresenceSensorState::Missing) enterMissing(now);
    return;
  }
  if (state_ == PresenceSensorState::Missing) {
    if (static_cast<int32_t>(now - nextProbeAtMs_) < 0) return;
    nextProbeAtMs_ = now + kProbeIntervalMs;
    if (!configure()) return;  // still missing; stateChanged_ may hold the start report
    state_ = PresenceSensorState::Ok;
    consecutiveFailures_ = 0;
    stateChanged_ = true;
    nextPollAtMs_ = now;
    return;
  }

  if (static_cast<int32_t>(now - nextPollAtMs_) < 0) return;
  nextPollAtMs_ = now + kPollIntervalMs;
  sths34pf80_tmos_drdy_status_t ready;
  if (chip.getDataReady(&ready) != 0) {
    readFailed(now);
    return;
  }
  if (!ready.drdy) return;
  PresenceSample sample;
  if (!readSample(now, sample)) {
    readFailed(now);
    return;
  }
  consecutiveFailures_ = 0;
  state_ = PresenceSensorState::Ok;
  sample_ = sample;
  sampleReady_ = true;
}

bool PresenceSensor::takeSample(PresenceSample& sample) {
  if (!sampleReady_) return false;
  sampleReady_ = false;
  sample = sample_;
  return true;
}

bool PresenceSensor::takeStateChange(bool& sensorOk) {
  if (!stateChanged_) return false;
  stateChanged_ = false;
  sensorOk = state_ != PresenceSensorState::Missing;
  return true;
}

bool PresenceSensor::takeConfig(PresenceConfig& config) {
  if (!configReady_) return false;
  configReady_ = false;
  config = config_;
  return true;
}

}  // namespace decaflash::mainframe
