#pragma once

#include <Arduino.h>
#include "frame_interval.h"
#include "personality.h"
#include "message_panel.h"
#include "presence_debug.h"

#ifndef DECAFLASH_EYE_RENDERER_MODE
#define DECAFLASH_EYE_RENDERER_MODE 48
#endif

#ifndef DECAFLASH_EYE_BENCHMARK
#define DECAFLASH_EYE_BENCHMARK 0
#endif

// Measurement only: lets main.cpp switch both frame intervals at run time.
#ifndef DECAFLASH_PANEL_RATE_SWEEP
#define DECAFLASH_PANEL_RATE_SWEEP 0
#endif

// Measurement only: per-section timing of every frame with the message panel.
#ifndef DECAFLASH_PANEL_FRAME_TIMING
#define DECAFLASH_PANEL_FRAME_TIMING 0
#endif

namespace decaflash::mainframe {

#if DECAFLASH_PANEL_FRAME_TIMING
// One frame with the message panel, split into its sections (microseconds).
struct PanelFrameTiming {
  uint32_t eyeUs = 0;          // canvas check, memory sample and the eye itself
  uint32_t shadeUs = 0;        // scanline shade and panel frame lines
  uint32_t wrapUs = 0;         // line wrapping, glyph row and scroll queries
  uint32_t textUs = 0;         // text blocks and both text passes, inline glyphs included
  uint32_t inlineDecodeUs = 0; // PNG decodes of inline glyphs (part of textUs)
  uint32_t emojiUs = 0;        // emoji block and cached emoji, decode included
  uint32_t emojiDecodeUs = 0;  // PNG decode into the emoji cache (part of emojiUs)
  uint32_t pushUs = 0;         // canvas to display
  uint32_t totalUs = 0;
  uint8_t emojiDecodes = 0;
  uint8_t inlineDecodes = 0;
  bool firstOfText = false;    // first panel frame since the text was shown
  uint8_t glyph = 0;           // leading glyph of the text
  uint32_t textShownAtMs = 0;
};
#endif

// The low-poly modes are deliberately compile-time selections so each device
// benchmark has the real memory layout of the renderer it measures. The
// production default is the visually approved 48-triangle variant; 20, 40,
// 72 and 80 select the corresponding comparison budgets.
struct EyeRendererBenchmark {
  uint8_t mode = DECAFLASH_EYE_RENDERER_MODE;
  uint8_t triangles = 0;
  uint16_t layerCadenceMs = 0;
  uint32_t outputFrames = 0;
  uint32_t outputAverageUs = 0;
  uint32_t outputWorstUs = 0;
  uint32_t layerFrames = 0;
  uint32_t layerAverageUs = 0;
  uint32_t layerWorstUs = 0;
  uint32_t panelPngDecodes = 0;
  uint32_t panelPngDecodeAverageUs = 0;
  uint32_t panelPngDecodeWorstUs = 0;
  uint32_t panelEmojiCacheBlits = 0;  // emoji tiles copied from the atlas
  size_t panelEmojiCacheBytes = 0;    // both emoji atlas sprites
  uint32_t panelAtlasDecodeUs = 0;    // decoding both atlas sprites at start
  size_t eyeAllocationBytes = 0;
  size_t psramAllocationBytes = 0;
  size_t internalAllocationBytes = 0;
  size_t psramFreeBytes = 0;
  size_t psramMinimumFreeBytes = 0;
  size_t psramLargestBlockBytes = 0;
  size_t internalFreeBytes = 0;
  size_t internalMinimumFreeBytes = 0;
  size_t internalLargestBlockBytes = 0;
};

// Compact on-device tempo diagnostics. showBpm is the clock that drives the
// adjacent beat dot and the Nodes; candidateBpm/confidence explain V2's next
// possible update without masquerading as the active clock.
struct TempoDebugInfo {
  uint16_t showBpm = 0;
  uint16_t candidateBpm = 0;
  uint8_t confidence = 0;
};

// Owns only the display animation cadence. Mainframe control remains in main.cpp.
class EyeRenderer {
 public:
  bool begin() { return canvasReady_ || initialiseCanvas(); }
  void service(uint32_t now, uint16_t bpm, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
               uint8_t vuLevel, uint8_t beatPulse, uint8_t attention, uint8_t annoyance,
               uint8_t loneliness, uint8_t bootProgress,
               bool sleeping,
               const Mood* debug = nullptr, const MotionEvent* event = nullptr,
               const TempoDebugInfo* tempoDebug = nullptr,
               const MessagePanel* panel = nullptr,
               const PresenceDebug* presence = nullptr);
  const EyeRendererBenchmark& benchmark() const { return benchmark_; }
  uint16_t frameIntervalMs() const { return frameIntervalMs_; }
  uint16_t panelFrameIntervalMs() const { return panelFrameIntervalMs_; }
#if DECAFLASH_PANEL_FRAME_TIMING
  const PanelFrameTiming& lastPanelFrameTiming() const { return panelTiming_; }
  uint32_t panelTimingFrames() const { return panelTimingFrames_; }
  // Measurement only: false draws emoji through the earlier PNG paths.
  void setPanelAtlasEnabled(bool enabled);
  bool panelAtlasEnabled() const;
#endif
#if DECAFLASH_PANEL_RATE_SWEEP
  void setFrameIntervals(uint16_t frameMs, uint16_t panelFrameMs) {
    frameIntervalMs_ = frameMs;
    panelFrameIntervalMs_ = panelFrameMs;
  }
#endif

 private:
  bool initialiseCanvas();
  void draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
            uint8_t vuLevel, uint8_t beatPulse, uint8_t attention, uint8_t annoyance,
            uint8_t loneliness, uint8_t bootProgress, bool sleeping);
  void drawBootSequence(uint32_t now, uint8_t bootProgress);
  void updateGaze(uint32_t now, uint8_t attention, float& gazeX, float& gazeY);
  void drawEmotionLids(uint8_t annoyance, uint8_t loneliness);
  void drawSleepLids(uint32_t now, bool sleeping);
  void drawMessagePanel(uint32_t now, const MessagePanel& panel);
  void drawPanelEmoji(int16_t x, int16_t y, PanelGlyph glyph);
#if DECAFLASH_PANEL_FRAME_TIMING
  bool refreshPanelEmojiCache(PanelGlyph glyph);
#endif
  void captureBenchmarkMemory(size_t psramBefore, size_t internalBefore);
  void sampleBenchmarkMemory();
  void recordOutputFrame(uint32_t durationUs);
  void recordLayerFrame(uint32_t durationUs);
  void recordPanelPngDecode(uint32_t durationUs);
  uint32_t nextGazeRandom();

  uint32_t lastFrameAtMs_ = 0;
  uint16_t frameIntervalMs_ = kFrameIntervalMs;
  uint16_t panelFrameIntervalMs_ = kPanelFrameIntervalMs;
  uint32_t idleBreathStartedAtMs_ = 0;
  uint32_t lastGazeAtMs_ = 0;
  float gazeAlertness_ = 0.0f;
  float gazeX_ = 0.0f;
  float gazeY_ = 0.0f;
  float gazeStartX_ = 0.0f;
  float gazeStartY_ = 0.0f;
  float gazeTargetX_ = 0.0f;
  float gazeTargetY_ = 0.0f;
  uint32_t gazeNextAtMs_ = 0;
  uint32_t gazeSaccadeAtMs_ = 0;
  uint16_t gazeSaccadeDurationMs_ = 0;
  uint32_t gazeRandom_ = 0xC0FFEE21;
  uint32_t lastSleepLidAtMs_ = 0;
  float sleepLidProgress_ = 0.0f;
  bool gazeSaccading_ = false;
  bool canvasReady_ = false;
  EyeRendererBenchmark benchmark_;
#if DECAFLASH_PANEL_FRAME_TIMING
  PanelFrameTiming panelTiming_;
  uint32_t panelTimingFrames_ = 0;
  uint32_t lastPanelTextShownAtMs_ = 0;
#endif
};

}  // namespace decaflash::mainframe
