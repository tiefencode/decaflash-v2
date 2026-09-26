#pragma once

#include <stddef.h>
#include <stdint.h>

namespace decaflash {

static constexpr size_t kCommandNameLength = 24;

enum class DeviceType : uint8_t {
  Mainframe = 0,
  Node = 1,
};

enum class NodeKind : uint8_t {
  Flashlight = 0,
  RgbStrip = 1,
  UvLed = 2,
};

enum class NodeEffect : uint8_t {
  None = 0,

  Wash = 1,
  Pulse = 2,
  Accent = 3,
  Flicker = 4,
};

// Neutral output instructions sent by the Mainframe. Nodes apply these to
// their hardware without receiving or interpreting any mood values.
enum class RgbRenderMode : uint8_t {
  Scene = 0,
  Solid = 1,
};

enum class FlashOverride : uint8_t {
  Scene = 0,
  Off = 1,
  Full = 2,
};

struct NodeVisualState {
  RgbRenderMode rgbMode = RgbRenderMode::Scene;
  uint8_t brightnessPercent = 100;
  uint8_t colorRed = 0;
  uint8_t colorGreen = 0;
  uint8_t colorBlue = 0;
  uint8_t overlayRed = 0;
  uint8_t overlayGreen = 0;
  uint8_t overlayBlue = 0;
  uint8_t overlayOpacityPercent = 0;
  FlashOverride flashOverride = FlashOverride::Scene;
  uint8_t reserved0 = 0;
};

constexpr bool isValidNodeVisualState(const NodeVisualState& state) {
  return (state.rgbMode == RgbRenderMode::Scene || state.rgbMode == RgbRenderMode::Solid) &&
         state.brightnessPercent <= 100 && state.overlayOpacityPercent <= 100 &&
         (state.flashOverride == FlashOverride::Scene ||
          state.flashOverride == FlashOverride::Off ||
          state.flashOverride == FlashOverride::Full);
}

enum class FlashPattern : uint8_t {
  Off = 0,
  Pulse = 1,
  PulseRow = 2,
};

enum class FlashCommandMode : uint8_t {
  Off = 0,
  VariationProfile = 1,
};

enum class RgbPattern : uint8_t {
  Off = 0,
  Wave = 1,
  Pulse = 2,
  PulseRow = 3,
  Runner = 4,
  Heartbeat = 5,
  RiserPulse = 6,
};

enum class RunnerMotion : uint8_t {
  Bounce = 0,
  Loop = 1,
};

enum class RunnerPresentation : uint8_t {
  Parallel = 0,
  Sequence = 1,
};

static constexpr uint8_t kMaxRunnerBands = 4;

// A runner is made from a few independently positioned colour bands. Width,
// position and edge are relative to the whole strip, so scene code does not
// need to know how many LEDs a particular node has.
struct RgbRunnerBand {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t widthPercent;
  uint8_t phasePercent;
  uint8_t edgePercent;
};

struct FlashCommand {
  char name[kCommandNameLength];
  FlashCommandMode mode;
  uint8_t variationWindowBars;
  uint16_t profileSeed;
  uint8_t pulseWeight;
  uint8_t slowPulseWeight;
  uint8_t doublePulseWeight;
  uint8_t quadPulseWeight;
  uint8_t riserWeight;
};

struct FlashRenderCommand {
  char name[kCommandNameLength];
  FlashPattern pattern;
  uint8_t triggerEveryBars;
  uint8_t triggerBeat;
  uint8_t burstCount;
  uint16_t burstIntervalMs;
  int16_t burstIntervalStepMs;
  uint16_t flashDurationMs;
};

struct RgbCommand {
  char name[kCommandNameLength];
  RgbPattern pattern;
  uint8_t primaryR;
  uint8_t primaryG;
  uint8_t primaryB;
  uint8_t secondaryR;
  uint8_t secondaryG;
  uint8_t secondaryB;
  uint8_t floorLevel;
  uint8_t baseLevel;
  uint8_t peakLevel;
  uint8_t triggerEveryBars;
  uint8_t triggerBeat;
  uint16_t startOffsetMs;
  uint16_t durationMs;
  uint16_t peakHoldMs;
  uint16_t fadeOutMs;
  uint8_t waveCycleBeats;
  uint8_t pulseCount;
  uint16_t pulseGapMs;
  uint8_t subsequentPulseLevel;
  RunnerMotion runnerMotion;
  RunnerPresentation runnerPresentation;
  uint8_t runnerBandCount;
  RgbRunnerBand runnerBands[kMaxRunnerBands];
};

struct NodeIdentity {
  DeviceType deviceType;
  NodeKind nodeKind;
  NodeEffect nodeEffect;
};

}  // namespace decaflash
