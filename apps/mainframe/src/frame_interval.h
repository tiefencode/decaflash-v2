#pragma once

#include <cstdint>

// Frame interval while the message panel is drawn over the eye. Chosen by the
// user on 2026-11-17 after a look at the device and a measurement of 25, 40,
// 50 and 67 ms (openspec change reduce-panel-frame-rate, design.md).
#ifndef DECAFLASH_PANEL_FRAME_INTERVAL_MS
#define DECAFLASH_PANEL_FRAME_INTERVAL_MS 67
#endif

// Frame interval without the panel. Measurement builds may override it, e.g.
// 40 ms as the reserve for CPU bottlenecks kept in ideas.md.
#ifndef DECAFLASH_FRAME_INTERVAL_MS
#define DECAFLASH_FRAME_INTERVAL_MS 25
#endif

namespace decaflash::mainframe {

constexpr uint16_t kFrameIntervalMs = DECAFLASH_FRAME_INTERVAL_MS;  // 25 ms: about 35 frames/s on the device
constexpr uint16_t kPanelFrameIntervalMs = DECAFLASH_PANEL_FRAME_INTERVAL_MS;  // about 15 frames/s

// The panel is drawn only while the creature is awake and no debug screen is
// shown; only those frames use the panel interval.
inline bool drawsMessagePanel(bool debugScreen, bool sleeping, bool panelVisible) {
  return !debugScreen && !sleeping && panelVisible;
}

inline uint16_t selectFrameIntervalMs(bool panelFrame, uint16_t normalMs = kFrameIntervalMs,
                                      uint16_t panelMs = kPanelFrameIntervalMs) {
  return panelFrame ? panelMs : normalMs;
}

}  // namespace decaflash::mainframe
