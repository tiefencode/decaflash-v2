#include "eye_renderer.h"
#include "iris_vu.h"

#include <M5Unified.h>
#include <math.h>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kFrameIntervalMs = 33;
constexpr int16_t kDisplaySize = 128;
constexpr uint8_t kScleraSegments = 16;
constexpr uint8_t kIrisSegments = IrisVu::kFacets;
constexpr uint8_t kIrisBaseValue = 52;
constexpr uint8_t kScleraColumns = 5;
constexpr uint8_t kScleraRows = 3;
constexpr uint8_t kScleraVariants = kScleraColumns * kScleraRows;

struct Point {
  int16_t x;
  int16_t y;
};

struct Rgb {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

M5Canvas canvas(&M5.Display);
M5Canvas scleraSprites[kScleraVariants];
IrisFacets irisFacets;
Point irisBoundary[kIrisSegments];
Point pupilBoundary[12];
bool scleraReady = false;

uint16_t color(uint8_t red, uint8_t green, uint8_t blue) {
  return canvas.color565(red, green, blue);
}

Point pointAt(int16_t centerX, int16_t centerY, float radius, float angle) {
  return {
    static_cast<int16_t>(lroundf(centerX + cosf(angle) * radius)),
    static_cast<int16_t>(lroundf(centerY + sinf(angle) * radius)),
  };
}

Rgb scleraColor(float normalX, float normalY, float normalZ) {
  const float directional = fmaxf(
    0.0f, normalX * 0.50f - normalY * 0.60f + normalZ * 0.6245f);
  const uint8_t shade = static_cast<uint8_t>(fminf(5.0f, 1.0f + directional * 5.0f));
  static constexpr Rgb kPalette[] = {
    {28, 43, 71}, {63, 82, 113}, {105, 124, 152},
    {154, 171, 191}, {208, 218, 226}, {255, 247, 227},
  };
  return kPalette[shade];
}

void drawScleraFacet(M5Canvas& sprite, float innerRadius, float outerRadius,
                     uint8_t segment, float rotation) {
  const float start = segment * TWO_PI / kScleraSegments + rotation;
  const float end = (segment + 1) * TWO_PI / kScleraSegments + rotation;
  const float middleAngle = (start + end) * 0.5f;
  const float middleRadius = (innerRadius + outerRadius) * 0.5f;
  const float normalX = cosf(middleAngle) * middleRadius / 61.0f;
  const float normalY = sinf(middleAngle) * middleRadius / 61.0f;
  const float normalZ = sqrtf(fmaxf(0.0f, 1.0f - normalX * normalX - normalY * normalY));
  const Rgb shade = scleraColor(normalX, normalY, normalZ);
  const uint16_t fill = sprite.color565(shade.red, shade.green, shade.blue);
  const Point insideStart = pointAt(64, 64, innerRadius, start);
  const Point insideEnd = pointAt(64, 64, innerRadius, end);
  const Point outsideStart = pointAt(64, 64, outerRadius, start);
  const Point outsideEnd = pointAt(64, 64, outerRadius, end);
  sprite.fillTriangle(insideStart.x, insideStart.y, outsideStart.x, outsideStart.y,
                      outsideEnd.x, outsideEnd.y, fill);
  sprite.fillTriangle(insideStart.x, insideStart.y, outsideEnd.x, outsideEnd.y,
                      insideEnd.x, insideEnd.y, fill);
}

void initialiseGeometry() {
  for (uint8_t index = 0; index < kIrisSegments; ++index) {
    const float angle = index * TWO_PI / kIrisSegments;
    irisBoundary[index] = {
      static_cast<int16_t>(lroundf(cosf(angle) * 36.0f)),
      static_cast<int16_t>(lroundf(sinf(angle) * 36.0f)),
    };
  }
  for (uint8_t index = 0; index < 12; ++index) {
    const float angle = index * TWO_PI / 12.0f;
    pupilBoundary[index] = {
      static_cast<int16_t>(lroundf(cosf(angle) * 15.0f)),
      static_cast<int16_t>(lroundf(sinf(angle) * 15.0f)),
    };
  }
}

void initialiseSclera() {
  constexpr float kRings[] = {0.0f, 46.0f, 61.0f};
  for (uint8_t row = 0; row < kScleraRows; ++row) {
    for (uint8_t column = 0; column < kScleraColumns; ++column) {
      const uint8_t index = row * kScleraColumns + column;
      auto& sprite = scleraSprites[index];
      sprite.setPsram(true);
      sprite.setColorDepth(16);
      if (sprite.createSprite(kDisplaySize, kDisplaySize) == nullptr) {
        for (uint8_t previous = 0; previous < index; ++previous) {
          scleraSprites[previous].deleteSprite();
        }
        return;
      }
      sprite.fillScreen(TFT_BLACK);
      const int8_t gazeX = static_cast<int8_t>(column) - 2;
      const int8_t gazeY = static_cast<int8_t>(row) - 1;
      const float rotation = 0.18f + gazeX * 0.22f + gazeY * 0.08f;
      for (uint8_t ring = 0; ring < 2; ++ring) {
        for (uint8_t segment = 0; segment < kScleraSegments; ++segment) {
          drawScleraFacet(sprite, kRings[ring], kRings[ring + 1], segment, rotation);
        }
      }
    }
  }
  scleraReady = true;
}

uint8_t scleraVariantFor(int16_t gazeX, int16_t gazeY) {
  int16_t column = (gazeX + 8) / 4;
  int16_t row = (gazeY + 4) / 4;
  if (column < 0) column = 0;
  if (column >= kScleraColumns) column = kScleraColumns - 1;
  if (row < 0) row = 0;
  if (row >= kScleraRows) row = kScleraRows - 1;
  return static_cast<uint8_t>(row * kScleraColumns + column);
}

void drawPupil(int16_t centerX, int16_t centerY, uint8_t beatPulse) {
  const int16_t radius = 15 + static_cast<int16_t>(beatPulse * 5U / 255U);
  for (uint8_t facet = 0; facet < 12; ++facet) {
    const Point first = {
      static_cast<int16_t>(centerX + pupilBoundary[facet].x * radius / 15),
      static_cast<int16_t>(centerY + pupilBoundary[facet].y * radius / 15),
    };
    const Point second = {
      static_cast<int16_t>(centerX + pupilBoundary[(facet + 1) % 12].x * radius / 15),
      static_cast<int16_t>(centerY + pupilBoundary[(facet + 1) % 12].y * radius / 15),
    };
    canvas.fillTriangle(centerX, centerY, first.x, first.y, second.x, second.y, TFT_BLACK);
  }
}

}  // namespace

void EyeRenderer::service(uint32_t now, uint8_t beatInBar, bool beatDotVisible,
                          bool beatDotIsSync, uint8_t vuLevel, uint8_t beatPulse, const Mood* debug, const MotionEvent* event) {
  if (now - lastFrameAtMs_ < kFrameIntervalMs) return;
  if (!canvasReady_ && !initialiseCanvas()) return;
  lastFrameAtMs_ = now;
  draw(now, beatInBar, beatDotVisible, beatDotIsSync, vuLevel, beatPulse);
  if (debug) {
    const char* labels[] = {"energy", "annoyance", "attention", "loneliness", "depression"};
    const uint8_t values[] = {debug->energy, debug->annoyance, debug->attention,
                            debug->loneliness, debug->depression};
    canvas.fillRect(3, 20, 122, 106, TFT_BLACK);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.fillRect(3, 3, 109, 14, TFT_BLACK);
    canvas.setCursor(7, 5);
    if (event) canvas.printf("%s %u", motionName(event->kind),
                             static_cast<unsigned>(event->count));
    for (uint8_t i = 0; i < 5; ++i) {
      const int y = 24 + i * 20;
      canvas.setCursor(7, y);
      canvas.printf("%-10s %3u", labels[i], static_cast<unsigned>(values[i]));
      canvas.fillRect(7, y + 10, 110, 3, color(30, 35, 45));
      canvas.fillRect(7, y + 10, values[i] * 110 / 100, 3, color(40, 212, 255));
    }
  }
  canvas.pushSprite(0, 0);
}

bool EyeRenderer::initialiseCanvas() {
  initialiseGeometry();
  canvas.setColorDepth(16);
  canvasReady_ = canvas.createSprite(kDisplaySize, kDisplaySize) != nullptr;
  if (canvasReady_) initialiseSclera();
  return canvasReady_;
}

void EyeRenderer::draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible,
                       bool beatDotIsSync, uint8_t vuLevel, uint8_t beatPulse) {
  const float gazePhase = static_cast<float>(now) * 0.00043f;
  const float driftPhase = static_cast<float>(now) * 0.00031f;
  const float gazeX = sinf(gazePhase + sinf(gazePhase * 0.37f) * 0.70f) * 7.0f +
                      sinf(gazePhase * 2.31f) * 1.3f;
  const float gazeY = sinf(driftPhase + sinf(driftPhase * 0.53f) * 0.55f + 0.8f) * 4.0f +
                      sinf(driftPhase * 2.11f) * 0.8f;
  const int16_t irisX = 64 + static_cast<int16_t>(gazeX);
  const int16_t irisY = 64 + static_cast<int16_t>(gazeY);

  if (scleraReady) {
    scleraSprites[scleraVariantFor(irisX - 64, irisY - 64)].pushSprite(&canvas, 0, 0);
  } else {
    canvas.fillScreen(TFT_BLACK);
    canvas.fillCircle(64, 64, 61, color(154, 171, 191));
  }

  irisFacets.update(now, vuLevel);
  const int16_t irisRadius = 36 + static_cast<int16_t>(beatPulse * 2U / 255U);
  for (uint8_t ray = 0; ray < kIrisSegments; ++ray) {
    const Point first = {
      static_cast<int16_t>(irisBoundary[ray].x * irisRadius / 36),
      static_cast<int16_t>(irisBoundary[ray].y * irisRadius / 36),
    };
    const Point second = {
      static_cast<int16_t>(irisBoundary[(ray + 1) % kIrisSegments].x * irisRadius / 36),
      static_cast<int16_t>(irisBoundary[(ray + 1) % kIrisSegments].y * irisRadius / 36),
    };
    const uint8_t value = irisFacets.displayLevel(ray, kIrisBaseValue);
    const auto sectorColor = IrisFacets::shadedColor(ray, value);
    canvas.fillTriangle(irisX, irisY, irisX + first.x, irisY + first.y,
                        irisX + second.x, irisY + second.y,
                        color(sectorColor.r, sectorColor.g, sectorColor.b));
  }
  drawPupil(irisX, irisY, beatPulse);

  if (beatDotVisible) {
    const uint16_t indicatorColor = beatDotIsSync
      ? color(255, 0, 0)
      : (beatInBar == 1 ? color(255, 210, 0) : color(255, 255, 255));
    canvas.fillCircle(116, 11, 5, TFT_BLACK);
    canvas.fillCircle(116, 11, 3, indicatorColor);
  }
}

}  // namespace decaflash::mainframe
