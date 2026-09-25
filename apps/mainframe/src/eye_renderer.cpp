#include "eye_renderer.h"

#include <M5Unified.h>
#include <math.h>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kFrameIntervalMs = 33;
constexpr int16_t kDisplaySize = 128;

M5Canvas canvas(&M5.Display);

uint16_t color(uint8_t red, uint8_t green, uint8_t blue) {
  return canvas.color565(red, green, blue);
}

void fillFacetFan(int16_t centerX, int16_t centerY, int16_t radius,
                  uint8_t facets, float rotation, uint16_t baseColor,
                  uint16_t alternateColor) {
  for (uint8_t facet = 0; facet < facets; ++facet) {
    const float start = rotation + (static_cast<float>(facet) / facets) * TWO_PI;
    const float end = rotation + (static_cast<float>(facet + 1) / facets) * TWO_PI;
    const int16_t startX = centerX + static_cast<int16_t>(cosf(start) * radius);
    const int16_t startY = centerY + static_cast<int16_t>(sinf(start) * radius);
    const int16_t endX = centerX + static_cast<int16_t>(cosf(end) * radius);
    const int16_t endY = centerY + static_cast<int16_t>(sinf(end) * radius);
    canvas.fillTriangle(centerX, centerY, startX, startY, endX, endY,
                        facet % 2 == 0 ? baseColor : alternateColor);
  }
}

}  // namespace

void EyeRenderer::service(uint32_t now, uint8_t beatInBar, bool beatDotVisible,
                          bool beatDotIsSync) {
  if (now - lastFrameAtMs_ < kFrameIntervalMs) return;
  if (!canvasReady_ && !initialiseCanvas()) return;
  lastFrameAtMs_ = now;
  draw(now, beatInBar, beatDotVisible, beatDotIsSync);
  canvas.pushSprite(0, 0);
}

bool EyeRenderer::initialiseCanvas() {
  canvas.setColorDepth(16);
  canvasReady_ = canvas.createSprite(kDisplaySize, kDisplaySize) != nullptr;
  return canvasReady_;
}

void EyeRenderer::draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible,
                       bool beatDotIsSync) {
  const float gazePhase = static_cast<float>(now) * 0.00068f;
  const float driftPhase = static_cast<float>(now) * 0.00039f;
  const int16_t irisX = 64 + static_cast<int16_t>(sinf(gazePhase) * 6.0f);
  const int16_t irisY = 64 + static_cast<int16_t>(sinf(driftPhase + 0.8f) * 3.0f);

  canvas.fillScreen(TFT_BLACK);

  // A nearly full-screen, spherical eye built from deliberately visible facets.
  fillFacetFan(64, 64, 61, 12, 0.18f,
                color(164, 194, 218), color(224, 239, 246));
  fillFacetFan(64, 64, 55, 12, 0.45f,
                color(190, 217, 233), color(239, 248, 250));

  fillFacetFan(irisX, irisY, 34, 10, gazePhase * 0.25f,
                color(8, 81, 152), color(24, 173, 227));
  fillFacetFan(irisX, irisY, 23, 8, -gazePhase * 0.18f,
                color(12, 140, 218), color(77, 222, 255));
  fillFacetFan(irisX, irisY, 15, 8, 0.2f,
                color(3, 6, 22), color(0, 0, 5));

  canvas.fillTriangle(irisX - 13, irisY - 15, irisX - 3, irisY - 15,
                      irisX - 13, irisY - 5, color(255, 255, 255));
  canvas.fillTriangle(irisX - 3, irisY - 15, irisX + 3, irisY - 9,
                      irisX - 3, irisY - 5, color(176, 234, 255));
  canvas.fillRect(irisX + 10, irisY + 10, 3, 3, color(120, 215, 255));

  if (beatDotVisible) {
    const uint16_t indicatorColor = beatDotIsSync
      ? color(255, 0, 0)
      : (beatInBar == 1 ? color(255, 210, 0) : color(255, 255, 255));
    canvas.fillCircle(116, 11, 5, TFT_BLACK);
    canvas.fillCircle(116, 11, 3, indicatorColor);
  }
}

}  // namespace decaflash::mainframe
