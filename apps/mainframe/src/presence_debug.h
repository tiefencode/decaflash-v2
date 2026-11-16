#pragma once

#include <cstddef>
#include <cstdint>

#include "presence_decision.h"

namespace decaflash::mainframe {

// One reading of the Unit TMOS PIR (STHS34PF80). All values are the sensor's
// raw register contents; ambientCentiC is the ambient register, whose
// sensitivity of 100 LSB/degree makes it hundredths of a degree Celsius.
struct PresenceSample {
  bool valid = false;
  bool presenceFlag = false;
  bool motionFlag = false;
  bool shockFlag = false;
  int16_t presence = 0;
  int16_t motion = 0;
  int16_t shock = 0;
  int16_t objectRaw = 0;
  int16_t objectComp = 0;
  int16_t ambientCentiC = 0;
  uint32_t atMs = 0;
};

// Configuration as read back from the sensor after it was set up.
struct PresenceConfig {
  uint8_t odrHz = 0;
  uint16_t avgTmos = 0;
  uint8_t avgT = 0;
  uint16_t presenceThreshold = 0;
  uint8_t presenceHysteresis = 0;
  uint16_t motionThreshold = 0;
  uint8_t motionHysteresis = 0;
  uint16_t shockThreshold = 0;
  uint8_t shockHysteresis = 0;
  // Object signal counts per degree Celsius, read from the sensor (SENS_DATA).
  uint16_t objectSensitivity = 0;
};

enum class PresenceSensorState : uint8_t { Missing, Ok, Error };
enum class PresenceFlag : uint8_t { Presence, Motion, Shock };
enum class PresenceSeries : uint8_t { Presence, Motion };

struct PresenceGraphRange {
  int32_t low = 0;
  int32_t high = 0;
};

// Hardware-independent state behind the presence debug screen: the latest
// sample, a short history for the graphs and when each flag last changed.
class PresenceDebug {
 public:
  // 15 s at the configured 8 Hz; one entry per graph pixel column.
  static constexpr size_t kHistorySize = 120;
  static constexpr uint32_t kUnknownAge = UINT32_MAX;

  void setState(PresenceSensorState state, uint32_t errors);
  void setConfig(const PresenceConfig& config) { config_ = config; haveConfig_ = true; }
  // The Mainframe's own presence decision, for the person row and graph line.
  void setPerson(PersonState state, uint32_t changedAtMs, bool known, int16_t enterThreshold,
                 int16_t nearThreshold) {
    person_ = state;
    personChangedAt_ = changedAtMs;
    personKnown_ = known;
    enterThreshold_ = enterThreshold;
    nearThreshold_ = nearThreshold;
  }
  PersonState person() const { return person_; }
  uint32_t personAgeMs(uint32_t now) const { return personKnown_ ? now - personChangedAt_ : kUnknownAge; }
  int16_t enterThreshold() const { return enterThreshold_; }
  int16_t nearThreshold() const { return nearThreshold_; }
  // Only valid samples enter the history and the flag timing.
  void add(const PresenceSample& sample);

  PresenceSensorState state() const { return state_; }
  uint32_t errors() const { return errors_; }
  bool haveConfig() const { return haveConfig_; }
  const PresenceConfig& config() const { return config_; }
  bool haveSample() const { return count_ > 0; }
  const PresenceSample& latest() const { return latest_; }

  size_t count() const { return count_; }
  // index 0 is the oldest stored entry, count() - 1 the newest.
  int16_t valueAt(PresenceSeries series, size_t index) const;
  bool flag(PresenceFlag which) const;
  // Milliseconds since the flag last changed, or kUnknownAge before the
  // first valid sample. Works across millis() overflow.
  uint32_t flagAgeMs(PresenceFlag which, uint32_t now) const;
  // Covers 0, the threshold and every stored value, so the threshold line
  // and the whole recent curve are always inside the graph.
  PresenceGraphRange range(PresenceSeries series, int32_t threshold, int32_t extraThreshold = 0,
                           int32_t secondExtraThreshold = 0) const;

 private:
  void reset();

  PresenceSample latest_;
  PresenceConfig config_;
  int16_t presence_[kHistorySize] = {};
  int16_t motion_[kHistorySize] = {};
  uint32_t flagChangedAt_[3] = {};
  bool flagState_[3] = {};
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t errors_ = 0;
  PresenceSensorState state_ = PresenceSensorState::Missing;
  bool haveConfig_ = false;
  PersonState person_ = PersonState::Away;
  uint32_t personChangedAt_ = 0;
  bool personKnown_ = false;
  int16_t enterThreshold_ = 0;
  int16_t nearThreshold_ = 0;
};

// Serial lines for live reading; each ends with '\n'. They return the length
// written (without the terminating zero), or 0 if the buffer is too small.
size_t formatPresenceSampleLine(char* out, size_t size, const PresenceSample& sample,
                                uint32_t errors, PersonState person);
// "away", "far", "far_moving" or "near".
const char* personName(PersonState person);
// "stands_in_front", "walked_past", "left" or "none".
const char* presenceEventName(PresenceEventKind kind);
// "TMOS_EVENT t=<ms> kind=<name>\n" for one presence event.
size_t formatPresenceEventLine(char* out, size_t size, const PresenceEvent& event);
size_t formatPresenceConfigLine(char* out, size_t size, const PresenceConfig& config);
size_t formatPresenceStateLine(char* out, size_t size, bool sensorOk, uint32_t errors);
// "-12.3" style with one decimal, rounded half away from zero.
size_t formatCentiCelsius(char* out, size_t size, int16_t centiC);
// Object signal as a temperature difference to the sensor itself, "+0.49"
// with two decimals, rounded half away from zero; "--" without a sensitivity.
size_t formatHeatCelsius(char* out, size_t size, int16_t objectRaw, uint16_t sensitivity);
// "42s" below 100 s, then whole minutes ("7m"), capped at "999m"; "--" if unknown.
size_t formatPresenceAge(char* out, size_t size, uint32_t ageMs);

}  // namespace decaflash::mainframe
