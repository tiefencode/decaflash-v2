#pragma once

#include <Arduino.h>
#include "personality.h"
#include "message_panel.h"

#ifndef DECAFLASH_EYE_RENDERER_MODE
#define DECAFLASH_EYE_RENDERER_MODE 48
#endif

#ifndef DECAFLASH_EYE_BENCHMARK
#define DECAFLASH_EYE_BENCHMARK 0
#endif

namespace decaflash::mainframe {

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
  uint32_t panelEmojiCacheBlits = 0;
  size_t panelEmojiCacheBytes = 0;
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

// Owns only the display animation cadence. Mainframe control remains in main.cpp.
class EyeRenderer {
 public:
  bool begin() { return canvasReady_ || initialiseCanvas(); }
  void service(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
               uint8_t vuLevel, uint8_t beatPulse, uint8_t attention, uint8_t annoyance,
               uint8_t loneliness, uint8_t bootProgress,
               const Mood* debug = nullptr, const MotionEvent* event = nullptr,
               const MessagePanel* panel = nullptr);
  const EyeRendererBenchmark& benchmark() const { return benchmark_; }

 private:
  bool initialiseCanvas();
  void draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible, bool beatDotIsSync,
            uint8_t vuLevel, uint8_t beatPulse, uint8_t attention, uint8_t annoyance,
            uint8_t loneliness, uint8_t bootProgress);
  void drawBootSequence(uint32_t now, uint8_t bootProgress);
  void updateGaze(uint32_t now, uint8_t attention, float& gazeX, float& gazeY);
  void drawEmotionLids(uint8_t annoyance, uint8_t loneliness);
  void drawMessagePanel(uint32_t now, const MessagePanel& panel);
  bool refreshPanelEmojiCache(PanelGlyph glyph);
  void drawCachedPanelEmoji(int16_t x, int16_t y, PanelGlyph glyph);
  void captureBenchmarkMemory(size_t psramBefore, size_t internalBefore);
  void sampleBenchmarkMemory();
  void recordOutputFrame(uint32_t durationUs);
  void recordLayerFrame(uint32_t durationUs);
  void recordPanelPngDecode(uint32_t durationUs);
  uint32_t nextGazeRandom();

  uint32_t lastFrameAtMs_ = 0;
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
  bool gazeSaccading_ = false;
  bool canvasReady_ = false;
  EyeRendererBenchmark benchmark_;
};

}  // namespace decaflash::mainframe
