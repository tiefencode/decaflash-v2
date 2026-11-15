#pragma once

#include <cstddef>
#include <cstdint>

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
  PresenceGraphRange range(PresenceSeries series, int32_t threshold) const;

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
};

// Serial lines for live reading; each ends with '\n'. They return the length
// written (without the terminating zero), or 0 if the buffer is too small.
size_t formatPresenceSampleLine(char* out, size_t size, const PresenceSample& sample,
                                uint32_t errors);
size_t formatPresenceConfigLine(char* out, size_t size, const PresenceConfig& config);
size_t formatPresenceStateLine(char* out, size_t size, bool sensorOk, uint32_t errors);
// "-12.3" style with one decimal, rounded half away from zero.
size_t formatCentiCelsius(char* out, size_t size, int16_t centiC);
// "42s" below 100 s, then whole minutes ("7m"), capped at "999m"; "--" if unknown.
size_t formatPresenceAge(char* out, size_t size, uint32_t ageMs);

}  // namespace decaflash::mainframe
