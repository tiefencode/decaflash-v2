#include "eye_renderer.h"
#include "iris_vu.h"
#include "panel_glyphs.h"
#include "panel_text_sprites.h"

#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <cstring>
#include <math.h>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kFrameIntervalMs = 25;
constexpr uint32_t kLowPolyLayerIntervalMs = 125;
constexpr int16_t kDisplaySize = 128;
constexpr uint8_t kScleraSegments = 16;
constexpr uint8_t kIrisSegments = IrisVu::kFacets;
constexpr uint8_t kIrisBaseValue = 52;
constexpr int16_t kIrisRadius = 38;

#if DECAFLASH_EYE_RENDERER_MODE != 20 && \
    DECAFLASH_EYE_RENDERER_MODE != 40 && \
    DECAFLASH_EYE_RENDERER_MODE != 48 && \
    DECAFLASH_EYE_RENDERER_MODE != 72 && \
    DECAFLASH_EYE_RENDERER_MODE != 80
#error "DECAFLASH_EYE_RENDERER_MODE must be 20, 40, 48, 72 or 80"
#endif

#if DECAFLASH_EYE_RENDERER_MODE == 20
constexpr uint8_t kLowPolySegments = 7;
constexpr uint8_t kLowPolyRings = 2;
constexpr uint8_t kLowPolyTriangles = 21;
#elif DECAFLASH_EYE_RENDERER_MODE == 40
constexpr uint8_t kLowPolySegments = 8;
constexpr uint8_t kLowPolyRings = 3;
constexpr uint8_t kLowPolyTriangles = 40;
#elif DECAFLASH_EYE_RENDERER_MODE == 48
constexpr uint8_t kLowPolySegments = 16;
constexpr uint8_t kLowPolyRings = 2;
constexpr uint8_t kLowPolyTriangles = 48;
#elif DECAFLASH_EYE_RENDERER_MODE == 72
constexpr uint8_t kLowPolySegments = 8;
constexpr uint8_t kLowPolyRings = 5;
constexpr uint8_t kLowPolyTriangles = 72;
#elif DECAFLASH_EYE_RENDERER_MODE == 80
constexpr uint8_t kLowPolySegments = 16;
constexpr uint8_t kLowPolyRings = 3;
constexpr uint8_t kLowPolyTriangles = 80;
#endif

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
M5Canvas lowPolySclera(&M5.Display);
IrisFacets irisFacets;
Point irisBoundary[kIrisSegments];
Point pupilBoundary[12];
bool lowPolyScleraReady = false;
uint32_t lastLowPolyLayerAtMs = 0;

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

// The mesh intentionally has no texture, interpolation or depth buffer. Its
// triangles are concentric strips on the visible sphere; drawing outside-in
// gives a stable painter's order for this convex, front-facing shape.
void drawLowPolyTriangle(M5Canvas& layer, const Point& first, const Point& second,
                         const Point& third, float gazeX, float gazeY) {
  const float middleX = (first.x + second.x + third.x) / 3.0f;
  const float middleY = (first.y + second.y + third.y) / 3.0f;
  // Gaze changes the sampled sphere normal, rather than moving the iris
  // layer. That retains the existing iris/pupil positioning contract.
  const float normalX = fminf(0.98f, fmaxf(-0.98f,
    (middleX - 64.0f) / 61.0f + gazeX / 88.0f));
  const float normalY = fminf(0.98f, fmaxf(-0.98f,
    (middleY - 64.0f) / 61.0f + gazeY / 88.0f));
  const float normalZ = sqrtf(fmaxf(0.0f, 1.0f - normalX * normalX - normalY * normalY));
  const Rgb shade = scleraColor(normalX, normalY, normalZ);
  layer.fillTriangle(first.x, first.y, second.x, second.y, third.x, third.y,
                     layer.color565(shade.red, shade.green, shade.blue));
}

void renderLowPolySclera(float gazeX, float gazeY, float scale = 1.0f) {
  lowPolySclera.fillScreen(TFT_BLACK);
  const float rotation = 0.18f + gazeX * 0.11f + gazeY * 0.04f;
#if DECAFLASH_EYE_RENDERER_MODE == 48
  // This is the same 16-segment/two-ring layout as the cached sprite.  A
  // facet quad is still submitted as two triangles, but both receive one
  // flat shade, so its diagonal cannot become a visible seam.
  constexpr float kRings[] = {0.0f, 46.0f, 61.0f};
  for (uint8_t ring = 0; ring < 2; ++ring) {
    for (uint8_t segment = 0; segment < kScleraSegments; ++segment) {
      drawScleraFacet(lowPolySclera, kRings[ring] * scale,
                      kRings[ring + 1U] * scale, segment, rotation);
    }
  }
#else
  for (uint8_t ring = 0; ring < kLowPolyRings; ++ring) {
    const float innerRadius = 61.0f * scale * ring / kLowPolyRings;
    const float outerRadius = 61.0f * scale * (ring + 1U) / kLowPolyRings;
    for (uint8_t segment = 0; segment < kLowPolySegments; ++segment) {
      const float start = segment * TWO_PI / kLowPolySegments + rotation;
      const float end = (segment + 1U) * TWO_PI / kLowPolySegments + rotation;
      const Point innerStart = pointAt(64, 64, innerRadius, start);
      const Point innerEnd = pointAt(64, 64, innerRadius, end);
      const Point outerStart = pointAt(64, 64, outerRadius, start);
      const Point outerEnd = pointAt(64, 64, outerRadius, end);
      if (ring == 0) {
        drawLowPolyTriangle(lowPolySclera, innerStart, outerStart, outerEnd, gazeX, gazeY);
      } else {
        drawLowPolyTriangle(lowPolySclera, innerStart, outerStart, outerEnd, gazeX, gazeY);
        drawLowPolyTriangle(lowPolySclera, innerStart, outerEnd, innerEnd, gazeX, gazeY);
      }
    }
  }
#endif
}

void initialiseLowPolySclera() {
  lowPolySclera.setPsram(true);
  lowPolySclera.setColorDepth(16);
  if (lowPolySclera.createSprite(kDisplaySize, kDisplaySize) == nullptr) return;
  renderLowPolySclera(0.0f, 0.0f);
  lowPolyScleraReady = true;
}

void initialiseGeometry() {
  for (uint8_t index = 0; index < kIrisSegments; ++index) {
    const float angle = index * TWO_PI / kIrisSegments;
    irisBoundary[index] = {
      static_cast<int16_t>(lroundf(cosf(angle) * kIrisRadius)),
      static_cast<int16_t>(lroundf(sinf(angle) * kIrisRadius)),
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

void drawPupil(int16_t centerX, int16_t centerY, uint8_t beatPulse, float scale = 1.0f) {
  const int16_t radius = static_cast<int16_t>(lroundf(
    (15.0f + static_cast<float>(beatPulse * 5U / 255U)) * scale));
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

void blendWhiteOverlay(uint8_t opacity) {
  if (opacity == 0) return;
  for (int16_t y = 0; y < kDisplaySize; ++y) {
    for (int16_t x = 0; x < kDisplaySize; ++x) {
      const uint16_t source = canvas.readPixel(x, y);
      const uint8_t red = static_cast<uint8_t>(((source >> 11U) & 0x1fU) * 255U / 31U);
      const uint8_t green = static_cast<uint8_t>(((source >> 5U) & 0x3fU) * 255U / 63U);
      const uint8_t blue = static_cast<uint8_t>((source & 0x1fU) * 255U / 31U);
      canvas.drawPixel(x, y, color(
        static_cast<uint8_t>(red + (255U - red) * opacity / 255U),
        static_cast<uint8_t>(green + (255U - green) * opacity / 255U),
        static_cast<uint8_t>(blue + (255U - blue) * opacity / 255U)));
    }
  }
}

void drawBootRevealEye(uint32_t now) {
  if (lowPolyScleraReady) {
    lowPolySclera.pushSprite(&canvas, 0, 0);
  } else {
    canvas.fillCircle(64, 64, 61, color(154, 171, 191));
  }

  irisFacets.update(now, 0);
  for (uint8_t ray = 0; ray < kIrisSegments; ++ray) {
    const uint8_t value = irisFacets.displayLevel(ray, kIrisBaseValue);
    const auto sectorColor = IrisFacets::shadedColor(ray, value);
    canvas.fillTriangle(64, 64, 64 + irisBoundary[ray].x, 64 + irisBoundary[ray].y,
                        64 + irisBoundary[(ray + 1) % kIrisSegments].x,
                        64 + irisBoundary[(ray + 1) % kIrisSegments].y,
                        color(sectorColor.r, sectorColor.g, sectorColor.b));
  }
  drawPupil(64, 64, 0);
}

bool isFaceGlyph(PanelGlyph glyph) {
  switch (glyph) {
    case PanelGlyph::Smile: case PanelGlyph::Dizzy: case PanelGlyph::Nauseous:
    case PanelGlyph::Love: case PanelGlyph::Laugh: case PanelGlyph::Melt:
    case PanelGlyph::HeartEyes: case PanelGlyph::Kiss: case PanelGlyph::TearSmile:
    case PanelGlyph::Hug: case PanelGlyph::Shy: case PanelGlyph::HeadTurn:
    case PanelGlyph::Plead: case PanelGlyph::Cry: case PanelGlyph::Scream:
    case PanelGlyph::Angry: case PanelGlyph::SeeNoEvil: case PanelGlyph::Blush:
    case PanelGlyph::EyeRoll: case PanelGlyph::Exhausted: case PanelGlyph::Relaxed:
    case PanelGlyph::Sad: case PanelGlyph::DizzySpiral: case PanelGlyph::Huffy:
    case PanelGlyph::Swearing: case PanelGlyph::Moon:
      return true;
    default: return false;
  }
}

void drawFaceBase(int16_t x, int16_t y, uint16_t tint) {
  const uint16_t outline = color(22, 25, 42);
  const uint16_t coolShadow = color(150, 169, 199);
  const uint16_t skin = tint ? tint : color(246, 243, 247);
  const uint16_t highlight = color(255, 255, 255);
  canvas.fillCircle(x + 8, y + 8, 8, outline);
  canvas.fillCircle(x + 9, y + 9, 7, coolShadow);
  canvas.fillCircle(x + 7, y + 7, 6, skin);
  canvas.fillCircle(x + 5, y + 4, 2, highlight);
}

void drawKohlEye(int16_t x, int16_t y, bool closed = false) {
  const uint16_t kohl = TFT_BLACK;
  const uint16_t shadow = color(102, 79, 132);
  if (closed) {
    canvas.drawFastHLine(x, y + 2, 4, kohl);
    canvas.drawPixel(x - 1, y + 1, kohl);
    canvas.drawPixel(x + 4, y + 1, kohl);
  } else {
    canvas.fillRect(x, y, 4, 3, kohl);
    canvas.drawPixel(x - 1, y + 1, kohl);
    canvas.drawPixel(x + 4, y + 1, kohl);
    canvas.drawPixel(x + 1, y, TFT_WHITE);
  }
  canvas.drawFastHLine(x, y + 4, 4, shadow);
}

void drawFaceGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  if (silhouette) {
    canvas.fillCircle(x + 10, y + 10, 8, color(0, 18, 55));
    return;
  }
  if (glyph == PanelGlyph::Moon) {
    canvas.fillCircle(x + 8, y + 8, 8, color(22, 25, 42));
    canvas.fillCircle(x + 6, y + 6, 5, color(43, 38, 61));
    canvas.fillRect(x + 4, y + 6, 3, 2, color(198, 184, 229));
    canvas.fillRect(x + 10, y + 9, 3, 2, color(198, 184, 229));
    canvas.drawFastHLine(x + 6, y + 13, 5, color(198, 184, 229));
    return;
  }
  uint16_t tint = 0;
  if (glyph == PanelGlyph::Angry || glyph == PanelGlyph::Huffy || glyph == PanelGlyph::Swearing) {
    tint = color(255, 230, 240);
  }
  drawFaceBase(x, y, tint);
  const uint16_t mouth = color(86, 47, 70);
  const uint16_t blush = color(186, 113, 153);
  const int16_t left = x + 4;
  const int16_t right = x + 10;

  switch (glyph) {
    case PanelGlyph::Dizzy:
    case PanelGlyph::DizzySpiral:
      canvas.drawLine(left, y + 5, left + 3, y + 8, color(60, 45, 96));
      canvas.drawLine(left + 3, y + 5, left, y + 8, color(60, 45, 96));
      canvas.drawLine(right, y + 5, right + 3, y + 8, color(60, 45, 96));
      canvas.drawLine(right + 3, y + 5, right, y + 8, color(60, 45, 96));
      canvas.drawRect(x + 7, y + 11, 3, 2, mouth);
      break;
    case PanelGlyph::Nauseous:
      drawKohlEye(left, y + 5, true); drawKohlEye(right, y + 5, true);
      canvas.fillRect(x + 6, y + 11, 5, 3, color(83, 184, 70));
      break;
    case PanelGlyph::Angry:
    case PanelGlyph::Huffy:
    case PanelGlyph::Swearing:
      drawKohlEye(left, y + 6); drawKohlEye(right, y + 6);
      canvas.drawLine(left - 1, y + 4, left + 3, y + 5, TFT_BLACK);
      canvas.drawLine(right + 4, y + 4, right, y + 5, TFT_BLACK);
      canvas.drawFastHLine(x + 6, y + 12, 5, mouth);
      if (glyph == PanelGlyph::Swearing) canvas.fillRect(x + 7, y + 10, 3, 3, color(255, 64, 88));
      break;
    case PanelGlyph::Cry:
    case PanelGlyph::Scream:
    case PanelGlyph::TearSmile:
      drawKohlEye(left, y + 5, glyph == PanelGlyph::TearSmile);
      drawKohlEye(right, y + 5, glyph == PanelGlyph::TearSmile);
      canvas.fillRect(left + 1, y + 9, 2, 4, color(45, 171, 255));
      if (glyph != PanelGlyph::TearSmile) canvas.fillRect(right + 1, y + 9, 2, 4, color(45, 171, 255));
      canvas.fillRect(x + 7, y + 12, 3, 2, mouth);
      break;
    case PanelGlyph::HeartEyes:
    case PanelGlyph::Love:
      canvas.fillRect(left, y + 5, 3, 3, color(231, 44, 118));
      canvas.fillRect(right, y + 5, 3, 3, color(231, 44, 118));
      canvas.drawFastHLine(x + 6, y + 12, 5, mouth);
      break;
    case PanelGlyph::EyeRoll:
      drawKohlEye(left, y + 5); drawKohlEye(right, y + 5);
      canvas.drawPixel(left + 1, y + 5, color(189, 175, 220));
      canvas.drawPixel(right + 1, y + 5, color(189, 175, 220));
      canvas.drawFastHLine(x + 6, y + 12, 5, mouth);
      break;
    case PanelGlyph::SeeNoEvil:
      canvas.fillRect(left - 1, y + 5, 5, 4, color(28, 26, 35));
      canvas.fillRect(right - 1, y + 5, 5, 4, color(28, 26, 35));
      canvas.drawFastHLine(x + 6, y + 12, 5, mouth);
      break;
    case PanelGlyph::Plead:
    case PanelGlyph::Shy:
    case PanelGlyph::Blush:
      drawKohlEye(left, y + 5); drawKohlEye(right, y + 5);
      canvas.fillRect(x + 3, y + 10, 2, 2, blush);
      canvas.fillRect(x + 12, y + 10, 2, 2, blush);
      canvas.drawPixel(x + 8, y + 12, mouth);
      break;
    case PanelGlyph::Relaxed:
    case PanelGlyph::Sad:
    case PanelGlyph::Exhausted:
      drawKohlEye(left, y + 5, true); drawKohlEye(right, y + 5, true);
      canvas.drawFastHLine(x + 6, y + 12, 5, mouth);
      break;
    default:
      drawKohlEye(left, y + 5, glyph == PanelGlyph::Laugh || glyph == PanelGlyph::Kiss);
      drawKohlEye(right, y + 5, glyph == PanelGlyph::Laugh || glyph == PanelGlyph::Kiss);
      canvas.fillRect(x + 7, y + 11, 3, 2, mouth);
      break;
  }
}

void drawObjectGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  const uint16_t ink = silhouette ? color(0, 18, 55) : color(40, 212, 255);
  const uint16_t violet = silhouette ? ink : color(152, 110, 255);
  const uint16_t magenta = silhouette ? ink : color(231, 44, 118);
  const uint16_t hand = silhouette ? ink : color(246, 243, 247);
  const uint16_t outline = silhouette ? ink : color(22, 25, 42);
  switch (glyph) {
    case PanelGlyph::Wave:
      canvas.fillRect(x + 5, y + 7, 7, 7, outline);
      canvas.fillRect(x + 6, y + 6, 5, 7, hand);
      canvas.fillRect(x + 3, y + 2, 2, 7, hand); canvas.fillRect(x + 6, y + 1, 2, 6, hand);
      canvas.fillRect(x + 9, y + 2, 2, 6, hand); canvas.fillRect(x + 12, y + 4, 2, 6, hand);
      break;
    case PanelGlyph::OkHand:
      canvas.drawCircle(x + 7, y + 8, 4, outline); canvas.drawCircle(x + 7, y + 8, 3, hand);
      canvas.drawFastVLine(x + 11, y + 2, 8, hand); canvas.drawFastVLine(x + 13, y + 4, 7, hand);
      canvas.drawFastHLine(x + 7, y + 13, 7, hand); break;
    case PanelGlyph::PointTogether:
      canvas.fillRect(x, y + 7, 6, 4, hand); canvas.fillRect(x + 4, y + 5, 4, 2, hand);
      canvas.fillRect(x + 10, y + 7, 6, 4, hand); canvas.fillRect(x + 8, y + 5, 4, 2, hand); break;
    case PanelGlyph::Praise:
      canvas.fillRect(x + 1, y + 2, 3, 11, hand); canvas.fillRect(x + 5, y, 2, 9, hand);
      canvas.fillRect(x + 12, y + 2, 3, 11, hand); canvas.fillRect(x + 9, y, 2, 9, hand); break;
    case PanelGlyph::Eye:
      canvas.fillTriangle(x, y + 8, x + 8, y + 2, x + 16, y + 8, ink);
      canvas.fillTriangle(x, y + 8, x + 8, y + 14, x + 16, y + 8, ink);
      canvas.fillCircle(x + 8, y + 8, 4, TFT_WHITE); canvas.fillCircle(x + 8, y + 8, 2, violet); break;
    case PanelGlyph::Warning:
      canvas.fillTriangle(x + 8, y + 1, x + 15, y + 15, x + 1, y + 15, magenta);
      canvas.drawFastVLine(x + 8, y + 5, 5, TFT_BLACK); canvas.fillCircle(x + 8, y + 12, 1, TFT_BLACK); break;
    case PanelGlyph::AngerBurst:
      canvas.fillTriangle(x + 8, y, x + 11, y + 6, x + 16, y + 5, magenta);
      canvas.fillTriangle(x + 16, y + 8, x + 10, y + 10, x + 11, y + 16, magenta);
      canvas.fillTriangle(x + 8, y + 16, x + 5, y + 10, x, y + 11, magenta);
      canvas.fillTriangle(x, y + 8, x + 5, y + 6, x + 4, y, magenta); break;
    case PanelGlyph::Music:
      canvas.drawFastVLine(x + 10, y + 2, 10, violet); canvas.drawFastHLine(x + 10, y + 2, 6, violet);
      canvas.fillCircle(x + 7, y + 13, 3, violet); break;
    case PanelGlyph::Speech:
      canvas.fillRoundRect(x + 1, y + 2, 14, 10, 2, violet); canvas.fillTriangle(x + 5, y + 11, x + 4, y + 15, x + 8, y + 11, violet); break;
    case PanelGlyph::Loop:
      canvas.drawCircle(x + 8, y + 8, 6, violet); canvas.fillTriangle(x + 13, y + 3, x + 16, y + 5, x + 12, y + 7, violet); break;
    case PanelGlyph::Skull:
      canvas.fillCircle(x + 8, y + 7, 7, outline); canvas.fillCircle(x + 8, y + 7, 6, hand);
      canvas.fillCircle(x + 5, y + 6, 2, TFT_BLACK); canvas.fillCircle(x + 11, y + 6, 2, TFT_BLACK);
      canvas.fillRect(x + 5, y + 11, 7, 3, hand); canvas.drawFastVLine(x + 8, y + 11, 3, outline); break;
    case PanelGlyph::SkullCrossbones:
      canvas.drawLine(x + 2, y + 14, x + 14, y + 2, hand); canvas.drawLine(x + 2, y + 2, x + 14, y + 14, hand);
      canvas.fillCircle(x + 8, y + 7, 5, hand); canvas.fillCircle(x + 6, y + 7, 1, TFT_BLACK); canvas.fillCircle(x + 10, y + 7, 1, TFT_BLACK); break;
    case PanelGlyph::Poop:
      canvas.fillCircle(x + 8, y + 11, 5, color(104, 64, 45)); canvas.fillCircle(x + 8, y + 7, 4, color(125, 76, 51));
      canvas.fillTriangle(x + 5, y + 5, x + 11, y + 5, x + 8, y + 1, color(125, 76, 51)); break;
    case PanelGlyph::BlackHeart:
      canvas.fillCircle(x + 5, y + 6, 4, TFT_BLACK); canvas.fillCircle(x + 11, y + 6, 4, TFT_BLACK);
      canvas.fillTriangle(x + 1, y + 7, x + 15, y + 7, x + 8, y + 15, TFT_BLACK); break;
    case PanelGlyph::Knife:
      canvas.fillTriangle(x + 3, y + 3, x + 13, y + 8, x + 3, y + 10, hand); canvas.fillRect(x + 1, y + 8, 5, 3, magenta); break;
    case PanelGlyph::Glass:
      canvas.drawRect(x + 4, y + 2, 8, 11, hand); canvas.fillRect(x + 5, y + 8, 6, 4, color(130, 54, 92));
      canvas.drawFastHLine(x + 2, y + 14, 12, hand); break;
    case PanelGlyph::Coffin:
      canvas.fillTriangle(x + 5, y + 1, x + 11, y + 1, x + 14, y + 5, outline);
      canvas.fillTriangle(x + 2, y + 11, x + 5, y + 15, x + 11, y + 15, outline);
      canvas.fillRect(x + 3, y + 5, 11, 6, outline); canvas.drawFastVLine(x + 8, y + 4, 8, hand); canvas.drawFastHLine(x + 6, y + 7, 5, hand); break;
    case PanelGlyph::Cigarette:
      canvas.fillRect(x + 2, y + 8, 11, 3, hand); canvas.fillRect(x + 12, y + 8, 3, 3, magenta);
      canvas.drawPixel(x + 5, y + 4, color(180, 180, 190)); canvas.drawPixel(x + 7, y + 2, color(180, 180, 190)); break;
    case PanelGlyph::Grave:
      canvas.fillRect(x + 4, y + 6, 8, 8, color(81, 70, 96)); canvas.fillCircle(x + 8, y + 6, 4, color(81, 70, 96));
      canvas.drawFastVLine(x + 8, y + 5, 6, hand); canvas.drawFastHLine(x + 6, y + 7, 5, hand); break;
    case PanelGlyph::Urn:
      canvas.fillRect(x + 5, y + 5, 6, 8, violet); canvas.fillCircle(x + 8, y + 6, 4, violet);
      canvas.drawFastHLine(x + 4, y + 14, 8, violet); canvas.drawFastHLine(x + 6, y + 3, 4, hand); break;
    default:
      canvas.drawRect(x + 2, y + 2, 12, 12, ink); canvas.drawLine(x + 4, y + 4, x + 12, y + 12, violet);
      canvas.drawLine(x + 12, y + 4, x + 4, y + 12, magenta); break;
  }
}

// Terminal emoji gets its own 40 px area. This keeps the familiar silhouette
// readable while leaving room for the pale, cold shading and heavy eye make-up.
void drawLargeKohlEye(int16_t x, int16_t y, bool closed = false) {
  const uint16_t kohl = TFT_BLACK;
  const uint16_t underEye = color(104, 82, 142);
  if (closed) {
    canvas.drawLine(x, y + 5, x + 11, y + 7, kohl);
    canvas.drawLine(x, y + 4, x + 2, y + 7, kohl);
    canvas.drawLine(x + 10, y + 6, x + 13, y + 3, kohl);
  } else {
    canvas.fillTriangle(x, y + 4, x + 12, y + 4, x + 9, y + 11, kohl);
    canvas.fillRect(x + 2, y + 5, 8, 5, kohl);
    canvas.fillRect(x + 4, y + 5, 3, 2, TFT_WHITE);
  }
  canvas.drawFastHLine(x + 2, y + 12, 10, underEye);
}

void drawLargeFaceGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  const uint16_t depth = color(0, 11, 28);
  if (silhouette) {
    canvas.fillCircle(x + 21, y + 22, 22, depth);
    return;
  }
  if (glyph == PanelGlyph::Moon) {
    canvas.fillCircle(x + 20, y + 20, 20, color(25, 23, 39));
    canvas.fillCircle(x + 14, y + 15, 14, color(47, 42, 70));
    canvas.fillRect(x + 11, y + 13, 6, 3, color(190, 177, 221));
    canvas.fillRect(x + 23, y + 22, 5, 3, color(190, 177, 221));
    canvas.drawFastHLine(x + 14, y + 30, 12, color(190, 177, 221));
    return;
  }

  const uint16_t outline = color(22, 25, 42);
  const uint16_t coolEdge = color(153, 177, 208);
  const uint16_t blueShade = color(200, 222, 240);
  const uint16_t porcelain = (glyph == PanelGlyph::Angry || glyph == PanelGlyph::Huffy ||
                              glyph == PanelGlyph::Swearing)
      ? color(255, 232, 242) : color(249, 248, 251);
  const uint16_t mouth = color(94, 42, 72);
  const uint16_t blush = color(198, 111, 150);
  canvas.fillCircle(x + 20, y + 20, 22, outline);
  canvas.fillCircle(x + 22, y + 22, 20, coolEdge);
  canvas.fillCircle(x + 18, y + 17, 19, porcelain);
  canvas.fillCircle(x + 10, y + 9, 8, TFT_WHITE);
  canvas.fillCircle(x + 29, y + 28, 8, blueShade);
  canvas.fillCircle(x + 17, y + 16, 17, porcelain);
  canvas.fillRect(x + 5, y + 25, 3, 6, blueShade);
  canvas.fillRect(x + 9, y + 32, 8, 3, blueShade);

  const int16_t left = x + 5;
  const int16_t right = x + 23;
  switch (glyph) {
    case PanelGlyph::Dizzy:
    case PanelGlyph::DizzySpiral:
      canvas.drawLine(left + 1, y + 16, left + 11, y + 24, color(61, 42, 99));
      canvas.drawLine(left + 11, y + 16, left + 1, y + 24, color(61, 42, 99));
      canvas.drawLine(right + 1, y + 16, right + 11, y + 24, color(61, 42, 99));
      canvas.drawLine(right + 11, y + 16, right + 1, y + 24, color(61, 42, 99));
      canvas.fillRect(x + 16, y + 30, 8, 4, mouth);
      break;
    case PanelGlyph::Nauseous:
      drawLargeKohlEye(left, y + 14, true); drawLargeKohlEye(right, y + 14, true);
      canvas.fillRoundRect(x + 13, y + 29, 14, 7, 2, color(76, 177, 67));
      break;
    case PanelGlyph::Angry:
    case PanelGlyph::Huffy:
    case PanelGlyph::Swearing:
      drawLargeKohlEye(left, y + 15); drawLargeKohlEye(right, y + 15);
      canvas.drawLine(left, y + 13, left + 10, y + 16, TFT_BLACK);
      canvas.drawLine(right + 12, y + 13, right + 3, y + 16, TFT_BLACK);
      canvas.drawFastHLine(x + 14, y + 32, 12, mouth);
      if (glyph == PanelGlyph::Swearing) canvas.fillRect(x + 16, y + 29, 8, 6, color(255, 64, 88));
      break;
    case PanelGlyph::Cry:
    case PanelGlyph::Scream:
    case PanelGlyph::TearSmile:
      drawLargeKohlEye(left, y + 14, glyph == PanelGlyph::TearSmile);
      drawLargeKohlEye(right, y + 14, glyph == PanelGlyph::TearSmile);
      canvas.fillRect(left + 4, y + 26, 5, 9, color(39, 174, 255));
      if (glyph != PanelGlyph::TearSmile) canvas.fillRect(right + 4, y + 26, 5, 9, color(39, 174, 255));
      canvas.fillRoundRect(x + 16, y + 31, 8, 4, 1, mouth);
      break;
    case PanelGlyph::HeartEyes:
    case PanelGlyph::Love:
      canvas.fillCircle(left + 5, y + 20, 5, color(231, 44, 118));
      canvas.fillCircle(right + 5, y + 20, 5, color(231, 44, 118));
      canvas.fillTriangle(left, y + 21, left + 10, y + 21, left + 5, y + 27, color(231, 44, 118));
      canvas.fillTriangle(right, y + 21, right + 10, y + 21, right + 5, y + 27, color(231, 44, 118));
      canvas.drawFastHLine(x + 13, y + 32, 14, mouth);
      break;
    case PanelGlyph::SeeNoEvil:
      canvas.fillRoundRect(left - 1, y + 16, 13, 11, 2, color(31, 28, 39));
      canvas.fillRoundRect(right - 1, y + 16, 13, 11, 2, color(31, 28, 39));
      canvas.drawFastHLine(x + 14, y + 32, 12, mouth);
      break;
    case PanelGlyph::Plead:
    case PanelGlyph::Shy:
    case PanelGlyph::Blush:
      drawLargeKohlEye(left, y + 14); drawLargeKohlEye(right, y + 14);
      canvas.fillCircle(x + 8, y + 29, 3, blush); canvas.fillCircle(x + 31, y + 29, 3, blush);
      canvas.fillCircle(x + 20, y + 33, 2, mouth);
      break;
    case PanelGlyph::Relaxed:
    case PanelGlyph::Sad:
    case PanelGlyph::Exhausted:
      drawLargeKohlEye(left, y + 14, true); drawLargeKohlEye(right, y + 14, true);
      canvas.drawFastHLine(x + 14, y + 32, 12, mouth);
      break;
    default:
      drawLargeKohlEye(left, y + 14, glyph == PanelGlyph::Laugh || glyph == PanelGlyph::Kiss);
      drawLargeKohlEye(right, y + 14, glyph == PanelGlyph::Laugh || glyph == PanelGlyph::Kiss);
      canvas.fillRoundRect(x + 15, y + 30, 10, 5, 2, mouth);
      if (glyph == PanelGlyph::Smile || glyph == PanelGlyph::Laugh) {
        canvas.fillRect(x + 17, y + 32, 6, 3, color(232, 125, 161));
      }
      break;
  }
}

void drawLargeObjectGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  const uint16_t depth = color(0, 11, 28);
  if (silhouette) {
    canvas.fillRoundRect(x, y, 40, 40, 8, depth);
    return;
  }
  const uint16_t ink = color(29, 33, 55);
  const uint16_t porcelain = color(249, 248, 251);
  const uint16_t coolEdge = color(153, 177, 208);
  const uint16_t cyan = color(40, 212, 255);
  const uint16_t violet = color(152, 110, 255);
  const uint16_t magenta = color(231, 44, 118);
  switch (glyph) {
    case PanelGlyph::Wave:
      canvas.fillCircle(x + 21, y + 26, 13, ink); canvas.fillCircle(x + 19, y + 23, 12, coolEdge);
      canvas.fillCircle(x + 17, y + 20, 11, porcelain);
      canvas.fillRoundRect(x + 5, y + 3, 5, 20, 2, porcelain);
      canvas.fillRoundRect(x + 12, y, 5, 19, 2, porcelain);
      canvas.fillRoundRect(x + 20, y + 1, 5, 18, 2, porcelain);
      canvas.fillRoundRect(x + 28, y + 6, 5, 17, 2, porcelain);
      break;
    case PanelGlyph::Praise:
      canvas.fillRoundRect(x + 3, y + 5, 12, 29, 4, coolEdge);
      canvas.fillRoundRect(x + 6, y, 6, 27, 3, porcelain);
      canvas.fillRoundRect(x + 25, y + 5, 12, 29, 4, coolEdge);
      canvas.fillRoundRect(x + 28, y, 6, 27, 3, porcelain);
      break;
    case PanelGlyph::Eye:
      canvas.fillTriangle(x + 1, y + 20, x + 20, y + 5, x + 39, y + 20, cyan);
      canvas.fillTriangle(x + 1, y + 20, x + 20, y + 35, x + 39, y + 20, cyan);
      canvas.fillCircle(x + 20, y + 20, 11, porcelain); canvas.fillCircle(x + 20, y + 20, 6, violet);
      canvas.fillCircle(x + 20, y + 20, 3, ink); break;
    case PanelGlyph::Warning:
      canvas.fillTriangle(x + 20, y + 2, x + 38, y + 37, x + 2, y + 37, magenta);
      canvas.fillRect(x + 18, y + 13, 4, 13, TFT_BLACK); canvas.fillCircle(x + 20, y + 31, 3, TFT_BLACK); break;
    case PanelGlyph::AngerBurst:
      canvas.fillTriangle(x + 20, y, x + 27, y + 14, x + 40, y + 11, magenta);
      canvas.fillTriangle(x + 40, y + 20, x + 26, y + 25, x + 28, y + 40, magenta);
      canvas.fillTriangle(x + 20, y + 40, x + 13, y + 26, x, y + 29, magenta);
      canvas.fillTriangle(x, y + 20, x + 13, y + 15, x + 11, y, magenta); break;
    case PanelGlyph::Loop:
      canvas.drawCircle(x + 20, y + 20, 15, violet); canvas.drawCircle(x + 20, y + 20, 14, violet);
      canvas.fillTriangle(x + 30, y + 6, x + 40, y + 12, x + 29, y + 18, violet); break;
    case PanelGlyph::Music:
      canvas.drawFastVLine(x + 24, y + 5, 24, violet); canvas.drawFastHLine(x + 24, y + 5, 14, violet);
      canvas.fillCircle(x + 15, y + 31, 7, violet); canvas.fillCircle(x + 31, y + 27, 7, violet); break;
    case PanelGlyph::Speech:
      canvas.fillRoundRect(x + 3, y + 7, 34, 24, 5, violet);
      canvas.fillTriangle(x + 14, y + 29, x + 10, y + 39, x + 22, y + 30, violet); break;
    default:
      canvas.fillCircle(x + 20, y + 20, 18, ink); canvas.fillCircle(x + 18, y + 17, 16, porcelain);
      canvas.drawLine(x + 10, y + 10, x + 30, y + 30, cyan); canvas.drawLine(x + 30, y + 10, x + 10, y + 30, magenta); break;
  }
}

void drawLargeGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  if (isFaceGlyph(glyph)) drawLargeFaceGlyph(x, y, glyph, silhouette);
  else drawLargeObjectGlyph(x, y, glyph, silhouette);
}

// A face occupies one text-flow emoji cell (two fixed columns). The stepped
// highlights and blue rim follow the reference sprite instead of becoming a
// tiny flat symbol.
void drawInlineKohlEye(int16_t x, int16_t y, bool closed = false) {
  const uint16_t kohl = TFT_BLACK;
  if (closed) {
    canvas.drawLine(x, y + 3, x + 6, y + 4, kohl);
    canvas.drawPixel(x - 1, y + 2, kohl);
    canvas.drawPixel(x + 7, y + 2, kohl);
  } else {
    canvas.fillTriangle(x - 1, y + 2, x + 7, y + 2, x + 5, y + 7, kohl);
    canvas.fillRect(x + 1, y + 3, 5, 4, kohl);
    canvas.drawPixel(x + 3, y + 3, TFT_WHITE);
  }
  canvas.drawFastHLine(x + 1, y + 8, 5, color(111, 86, 145));
}

void drawInlineFaceGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  const uint16_t depth = color(0, 11, 28);
  if (silhouette) {
    canvas.fillCircle(x + 11, y + 11, 11, depth);
    return;
  }
  const uint16_t outline = color(22, 25, 42);
  const uint16_t coolEdge = color(151, 177, 209);
  const uint16_t blueShade = color(205, 226, 242);
  const uint16_t porcelain = color(249, 248, 251);
  const uint16_t mouth = color(91, 40, 69);
  canvas.fillCircle(x + 10, y + 10, 11, outline);
  canvas.fillCircle(x + 11, y + 11, 10, coolEdge);
  canvas.fillCircle(x + 9, y + 8, 9, porcelain);
  canvas.fillRect(x + 4, y + 3, 5, 3, TFT_WHITE);
  canvas.fillRect(x + 2, y + 6, 3, 5, blueShade);
  canvas.fillRect(x + 6, y + 16, 8, 3, blueShade);
  canvas.fillRect(x + 15, y + 13, 3, 3, blueShade);

  const int16_t left = x + 3;
  const int16_t right = x + 12;
  switch (glyph) {
    case PanelGlyph::Dizzy:
    case PanelGlyph::DizzySpiral:
      canvas.drawLine(left, y + 8, left + 6, y + 13, color(61, 42, 99));
      canvas.drawLine(left + 6, y + 8, left, y + 13, color(61, 42, 99));
      canvas.drawLine(right, y + 8, right + 6, y + 13, color(61, 42, 99));
      canvas.drawLine(right + 6, y + 8, right, y + 13, color(61, 42, 99));
      canvas.fillRect(x + 8, y + 15, 5, 2, mouth);
      break;
    case PanelGlyph::Nauseous:
      drawInlineKohlEye(left, y + 7, true); drawInlineKohlEye(right, y + 7, true);
      canvas.fillRect(x + 7, y + 15, 6, 3, color(79, 179, 68));
      break;
    case PanelGlyph::Cry:
    case PanelGlyph::Scream:
    case PanelGlyph::TearSmile:
      drawInlineKohlEye(left, y + 7, glyph == PanelGlyph::TearSmile);
      drawInlineKohlEye(right, y + 7, glyph == PanelGlyph::TearSmile);
      canvas.fillRect(left + 2, y + 14, 3, 5, color(38, 175, 255));
      if (glyph != PanelGlyph::TearSmile) canvas.fillRect(right + 2, y + 14, 3, 5, color(38, 175, 255));
      canvas.fillRect(x + 8, y + 16, 4, 2, mouth);
      break;
    case PanelGlyph::SeeNoEvil:
      canvas.fillRect(left, y + 8, 7, 6, color(30, 28, 38));
      canvas.fillRect(right, y + 8, 7, 6, color(30, 28, 38));
      canvas.fillRect(x + 8, y + 16, 4, 2, mouth);
      break;
    case PanelGlyph::HeartEyes:
    case PanelGlyph::Love:
      canvas.fillCircle(left + 3, y + 10, 3, color(231, 44, 118));
      canvas.fillCircle(right + 3, y + 10, 3, color(231, 44, 118));
      canvas.drawFastHLine(x + 7, y + 16, 6, mouth);
      break;
    case PanelGlyph::Angry:
    case PanelGlyph::Huffy:
    case PanelGlyph::Swearing:
      drawInlineKohlEye(left, y + 8); drawInlineKohlEye(right, y + 8);
      canvas.drawLine(left, y + 7, left + 5, y + 8, TFT_BLACK);
      canvas.drawLine(right + 6, y + 7, right + 2, y + 8, TFT_BLACK);
      canvas.drawFastHLine(x + 7, y + 17, 6, mouth);
      break;
    default:
      drawInlineKohlEye(left, y + 7, glyph == PanelGlyph::Laugh || glyph == PanelGlyph::Kiss);
      drawInlineKohlEye(right, y + 7, glyph == PanelGlyph::Laugh || glyph == PanelGlyph::Kiss);
      canvas.fillRect(x + 8, y + 16, 5, 2, mouth);
      if (glyph == PanelGlyph::Smile || glyph == PanelGlyph::Laugh) {
        canvas.fillRect(x + 9, y + 18, 3, 2, color(232, 125, 161));
      }
      break;
  }
}

void drawCenteredFaceGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  const uint16_t depth = color(0, 11, 28);
  if (silhouette) {
    canvas.fillCircle(x + 16, y + 17, 17, depth);
    return;
  }
  const uint16_t outline = color(22, 25, 42);
  const uint16_t coolEdge = color(151, 177, 209);
  const uint16_t blueShade = color(205, 226, 242);
  const uint16_t porcelain = color(249, 248, 251);
  const uint16_t mouth = color(91, 40, 69);
  canvas.fillCircle(x + 16, y + 16, 17, outline);
  canvas.fillCircle(x + 18, y + 18, 16, coolEdge);
  canvas.fillCircle(x + 14, y + 13, 15, porcelain);
  canvas.fillRect(x + 8, y + 3, 9, 4, TFT_WHITE);
  canvas.fillRect(x + 5, y + 7, 4, 7, blueShade);
  canvas.fillRect(x + 10, y + 27, 12, 4, blueShade);
  canvas.fillRect(x + 24, y + 22, 4, 4, blueShade);

  const int16_t left = x + 5;
  const int16_t right = x + 19;
  const uint16_t kohl = TFT_BLACK;
  const uint16_t underEye = color(113, 84, 154);
  if (glyph == PanelGlyph::Dizzy || glyph == PanelGlyph::DizzySpiral) {
    canvas.drawLine(left, y + 12, left + 9, y + 20, color(61, 42, 99));
    canvas.drawLine(left + 9, y + 12, left, y + 20, color(61, 42, 99));
    canvas.drawLine(right, y + 12, right + 9, y + 20, color(61, 42, 99));
    canvas.drawLine(right + 9, y + 12, right, y + 20, color(61, 42, 99));
    canvas.fillRect(x + 12, y + 24, 8, 3, mouth);
    return;
  }
  if (glyph == PanelGlyph::Nauseous) {
    canvas.drawLine(left, y + 15, left + 8, y + 16, kohl);
    canvas.drawLine(right, y + 16, right + 8, y + 15, kohl);
    canvas.fillRoundRect(x + 11, y + 24, 10, 5, 2, color(79, 179, 68));
    return;
  }
  if (glyph == PanelGlyph::SeeNoEvil) {
    canvas.fillRoundRect(left, y + 12, 10, 8, 2, kohl);
    canvas.fillRoundRect(right, y + 12, 10, 8, 2, kohl);
  } else if (glyph == PanelGlyph::HeartEyes || glyph == PanelGlyph::Love) {
    canvas.fillCircle(left + 4, y + 15, 4, color(231, 44, 118));
    canvas.fillCircle(right + 4, y + 15, 4, color(231, 44, 118));
  } else {
    canvas.fillTriangle(left - 2, y + 11, left + 9, y + 11, left + 8, y + 20, kohl);
    canvas.fillRect(left + 2, y + 13, 7, 6, kohl);
    canvas.fillTriangle(right + 10, y + 11, right - 1, y + 11, right, y + 20, kohl);
    canvas.fillRect(right, y + 13, 7, 6, kohl);
    canvas.drawFastHLine(left + 2, y + 21, 7, underEye);
    canvas.drawFastHLine(right, y + 21, 7, underEye);
    canvas.fillRect(left + 4, y + 22, 4, 2, underEye);
    canvas.fillRect(right + 2, y + 22, 4, 2, underEye);
  }
  if (glyph == PanelGlyph::Cry || glyph == PanelGlyph::Scream || glyph == PanelGlyph::TearSmile) {
    canvas.fillRect(left + 4, y + 21, 4, 7, color(38, 175, 255));
    if (glyph != PanelGlyph::TearSmile) canvas.fillRect(right + 3, y + 21, 4, 7, color(38, 175, 255));
  }
  if (glyph == PanelGlyph::Angry || glyph == PanelGlyph::Huffy || glyph == PanelGlyph::Swearing) {
    canvas.drawLine(left, y + 9, left + 8, y + 11, kohl);
    canvas.drawLine(right + 8, y + 9, right + 1, y + 11, kohl);
  }
  canvas.fillRect(x + 12, y + 25, 9, 3, mouth);
  if (glyph == PanelGlyph::Smile || glyph == PanelGlyph::Laugh) {
    canvas.fillRect(x + 14, y + 28, 5, 3, color(232, 125, 161));
  }
}

void drawCenteredObjectGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  const uint16_t depth = color(0, 11, 28);
  if (silhouette) {
    canvas.fillRoundRect(x, y, 32, 32, 7, depth);
    return;
  }
  const uint16_t porcelain = color(249, 248, 251);
  const uint16_t coolEdge = color(151, 177, 209);
  const uint16_t cyan = color(40, 212, 255);
  const uint16_t violet = color(152, 110, 255);
  const uint16_t magenta = color(231, 44, 118);
  switch (glyph) {
    case PanelGlyph::Wave:
      canvas.fillCircle(x + 17, y + 21, 10, coolEdge); canvas.fillCircle(x + 15, y + 18, 9, porcelain);
      canvas.fillRoundRect(x + 4, y + 2, 4, 17, 2, porcelain); canvas.fillRoundRect(x + 10, y, 4, 16, 2, porcelain);
      canvas.fillRoundRect(x + 16, y + 1, 4, 15, 2, porcelain); canvas.fillRoundRect(x + 22, y + 5, 4, 14, 2, porcelain); break;
    case PanelGlyph::Praise:
      canvas.fillRoundRect(x + 3, y + 4, 10, 24, 4, coolEdge); canvas.fillRoundRect(x + 6, y, 5, 22, 3, porcelain);
      canvas.fillRoundRect(x + 19, y + 4, 10, 24, 4, coolEdge); canvas.fillRoundRect(x + 22, y, 5, 22, 3, porcelain); break;
    case PanelGlyph::Eye:
      canvas.fillTriangle(x + 1, y + 16, x + 16, y + 4, x + 31, y + 16, cyan);
      canvas.fillTriangle(x + 1, y + 16, x + 16, y + 28, x + 31, y + 16, cyan);
      canvas.fillCircle(x + 16, y + 16, 9, porcelain); canvas.fillCircle(x + 16, y + 16, 5, violet); break;
    case PanelGlyph::AngerBurst:
      canvas.fillTriangle(x + 16, y, x + 22, y + 11, x + 32, y + 9, magenta);
      canvas.fillTriangle(x + 32, y + 16, x + 21, y + 21, x + 23, y + 32, magenta);
      canvas.fillTriangle(x + 16, y + 32, x + 10, y + 21, x, y + 23, magenta);
      canvas.fillTriangle(x, y + 16, x + 10, y + 11, x + 9, y, magenta); break;
    case PanelGlyph::Loop:
      canvas.drawCircle(x + 16, y + 16, 12, violet); canvas.drawCircle(x + 16, y + 16, 11, violet);
      canvas.fillTriangle(x + 24, y + 5, x + 32, y + 10, x + 23, y + 15, violet); break;
    case PanelGlyph::Music:
      canvas.drawFastVLine(x + 19, y + 3, 20, violet); canvas.drawFastHLine(x + 19, y + 3, 11, violet);
      canvas.fillCircle(x + 11, y + 26, 6, violet); canvas.fillCircle(x + 25, y + 22, 6, violet); break;
    default:
      canvas.fillCircle(x + 16, y + 16, 14, coolEdge); canvas.fillCircle(x + 14, y + 13, 13, porcelain); break;
  }
}

bool panelTextSpriteIndex(PanelGlyph glyph, uint8_t& index) {
  switch (glyph) {
    case PanelGlyph::Smile: index = 0; return true;
    case PanelGlyph::Wave: index = 1; return true;
    case PanelGlyph::Eye: index = 2; return true;
    case PanelGlyph::Nauseous: index = 3; return true;
    case PanelGlyph::AngerBurst: index = 4; return true;
    case PanelGlyph::Speech: index = 5; return true;
    case PanelGlyph::Loop: index = 6; return true;
    case PanelGlyph::TearSmile: index = 7; return true;
    case PanelGlyph::Cry: index = 8; return true;
    case PanelGlyph::SeeNoEvil: index = 9; return true;
    case PanelGlyph::PointTogether: index = 10; return true;
    case PanelGlyph::Praise: index = 11; return true;
    case PanelGlyph::Blush: index = 12; return true;
    case PanelGlyph::DizzySpiral: index = 13; return true;
    case PanelGlyph::Music: index = 14; return true;
    case PanelGlyph::Warning: index = 15; return true;
    default: return false;
  }
}

bool drawApprovedTextSprite(int16_t x, int16_t y, PanelGlyph glyph) {
  uint8_t index = 0;
  if (!panelTextSpriteIndex(glyph, index)) return false;
  return canvas.drawPng(kPanelTextSpritesPng, kPanelTextSpritesPngLength, x, y,
                        32, 32, (index % 4) * 32, (index / 4) * 32);
}

void drawCenteredGlyph(int16_t x, int16_t y, PanelGlyph glyph, bool silhouette) {
  if (!silhouette && drawApprovedTextSprite(x, y, glyph)) return;
  if (isFaceGlyph(glyph)) drawCenteredFaceGlyph(x, y, glyph, silhouette);
  else drawCenteredObjectGlyph(x, y, glyph, silhouette);
}

void drawPanelLine(const char* text, int16_t x, int16_t y, uint16_t textColor, bool shadow) {
  for (size_t at = 0, length = std::strlen(text); at < length;) {
    const auto token = panelGlyphToken(text + at, length - at);
    if (!token.bytes) break;
    if (token.glyph != PanelGlyph::None && token.glyph != PanelGlyph::Aumlaut &&
        token.glyph != PanelGlyph::Oumlaut && token.glyph != PanelGlyph::Uumlaut &&
        token.glyph != PanelGlyph::SharpS) {
      if (isFaceGlyph(token.glyph)) drawInlineFaceGlyph(x + 2, y - 2, token.glyph, shadow);
      else drawObjectGlyph(x + 4, y, token.glyph, shadow);
    } else {
      char character = text[at];
      if (token.glyph == PanelGlyph::Aumlaut) character = (text[at] == '\xc3' && text[at + 1] == '\x84') ? 'A' : 'a';
      if (token.glyph == PanelGlyph::Oumlaut) character = (text[at] == '\xc3' && text[at + 1] == '\x96') ? 'O' : 'o';
      if (token.glyph == PanelGlyph::Uumlaut) character = (text[at] == '\xc3' && text[at + 1] == '\x9c') ? 'U' : 'u';
      if (token.glyph == PanelGlyph::SharpS) character = 'B';
      if (static_cast<uint8_t>(character) < 0x20 || static_cast<uint8_t>(character) >= 0x80) character = '?';
      canvas.setTextColor(textColor);
      canvas.setCursor(x, y);
      canvas.print(character);
      if (token.glyph == PanelGlyph::Aumlaut || token.glyph == PanelGlyph::Oumlaut ||
          token.glyph == PanelGlyph::Uumlaut) {
        canvas.fillRect(x + 2, y, 3, 3, textColor); canvas.fillRect(x + 8, y, 3, 3, textColor);
      }
    }
    x += token.columns * 12;
    at += token.bytes;
  }
}

}  // namespace

void EyeRenderer::service(uint32_t now, uint8_t beatInBar, bool beatDotVisible,
                          bool beatDotIsSync, uint8_t vuLevel, uint8_t beatPulse,
                          uint8_t attention, uint8_t annoyance, uint8_t loneliness,
                          uint8_t bootProgress,
                          const Mood* debug, const MotionEvent* event,
                          const MessagePanel* panel) {
  if (now - lastFrameAtMs_ < kFrameIntervalMs) return;
  const uint32_t frameStartedAtUs = micros();
  if (!canvasReady_ && !initialiseCanvas()) return;
  lastFrameAtMs_ = now;
  draw(now, beatInBar, beatDotVisible, beatDotIsSync, vuLevel, beatPulse, attention,
       annoyance, loneliness, bootProgress);
  if (debug) {
    const char* labels[] = {"energy", "annoyance", "attention", "loneliness", "depression"};
    const uint8_t values[] = {debug->energy, debug->annoyance, debug->attention,
                            debug->loneliness, debug->depression};
    canvas.fillRect(3, 20, 122, 106, TFT_BLACK);
    // The message panel selects Font 2. Restore the original compact debug
    // font explicitly so the values never inherit the panel's larger glyphs.
    canvas.setTextFont(1);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_WHITE, TFT_BLACK);
    canvas.fillRect(3, 3, 109, 14, TFT_BLACK);
    canvas.setCursor(7, 5);
    if (event) {
      canvas.printf("%s %u", motionName(event->kind),
                    static_cast<unsigned>(event->count));
    }
    for (uint8_t i = 0; i < 5; ++i) {
      const int y = 24 + i * 20;
      canvas.setCursor(7, y);
      canvas.printf("%-10s %3u", labels[i], static_cast<unsigned>(values[i]));
      canvas.fillRect(7, y + 10, 110, 3, color(30, 35, 45));
      canvas.fillRect(7, y + 10, values[i] * 110 / 100, 3, color(40, 212, 255));
    }
  } else if (panel && panel->visible(now)) {
    drawMessagePanel(now, *panel);
  }
  canvas.pushSprite(0, 0);
  recordOutputFrame(micros() - frameStartedAtUs);
}

bool EyeRenderer::initialiseCanvas() {
  const size_t psramBefore = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t internalBefore = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  initialiseGeometry();
  canvas.setColorDepth(16);
  canvasReady_ = canvas.createSprite(kDisplaySize, kDisplaySize) != nullptr;
  if (canvasReady_) {
    initialiseLowPolySclera();
    benchmark_.triangles = kLowPolyTriangles;
    benchmark_.layerCadenceMs = kLowPolyLayerIntervalMs;
    captureBenchmarkMemory(psramBefore, internalBefore);
  }
  return canvasReady_;
}

void EyeRenderer::captureBenchmarkMemory(size_t psramBefore, size_t internalBefore) {
  benchmark_.psramFreeBytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  benchmark_.psramLargestBlockBytes = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  benchmark_.internalFreeBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  benchmark_.internalLargestBlockBytes = heap_caps_get_largest_free_block(
    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  benchmark_.psramAllocationBytes = psramBefore - benchmark_.psramFreeBytes;
  benchmark_.internalAllocationBytes = internalBefore - benchmark_.internalFreeBytes;
  benchmark_.eyeAllocationBytes = benchmark_.psramAllocationBytes +
    benchmark_.internalAllocationBytes;
}

void EyeRenderer::recordOutputFrame(uint32_t durationUs) {
  ++benchmark_.outputFrames;
  benchmark_.outputAverageUs = static_cast<uint32_t>(
    (static_cast<uint64_t>(benchmark_.outputAverageUs) *
      (benchmark_.outputFrames - 1U) + durationUs) / benchmark_.outputFrames);
  if (durationUs > benchmark_.outputWorstUs) benchmark_.outputWorstUs = durationUs;
}

void EyeRenderer::recordLayerFrame(uint32_t durationUs) {
  ++benchmark_.layerFrames;
  benchmark_.layerAverageUs = static_cast<uint32_t>(
    (static_cast<uint64_t>(benchmark_.layerAverageUs) *
      (benchmark_.layerFrames - 1U) + durationUs) / benchmark_.layerFrames);
  if (durationUs > benchmark_.layerWorstUs) benchmark_.layerWorstUs = durationUs;
}

uint32_t EyeRenderer::nextGazeRandom() {
  gazeRandom_ = gazeRandom_ * 1664525UL + 1013904223UL;
  return gazeRandom_;
}

void EyeRenderer::updateGaze(uint32_t now, uint8_t attention, float& gazeX, float& gazeY) {
  uint32_t elapsedMs = lastGazeAtMs_ ? now - lastGazeAtMs_ : 0;
  lastGazeAtMs_ = now;
  if (elapsedMs > 100) elapsedMs = 100;

  const float targetAlertness = fmaxf(
    0.0f, (static_cast<float>(attention) - 10.0f) / 90.0f);
  const float smoothing = fminf(1.0f, static_cast<float>(elapsedMs) / 400.0f);
  gazeAlertness_ += (targetAlertness - gazeAlertness_) * smoothing;

  if (gazeNextAtMs_ == 0) gazeNextAtMs_ = now + 900;
  if (gazeSaccading_) {
    const uint32_t elapsedSaccadeMs = now - gazeSaccadeAtMs_;
    const float progress = fminf(
      1.0f, static_cast<float>(elapsedSaccadeMs) / gazeSaccadeDurationMs_);
    const float eased = progress * progress * (3.0f - 2.0f * progress);
    gazeX_ = gazeStartX_ + (gazeTargetX_ - gazeStartX_) * eased;
    gazeY_ = gazeStartY_ + (gazeTargetY_ - gazeStartY_) * eased;
    if (progress >= 1.0f) {
      gazeSaccading_ = false;
      const uint32_t minimumFixationMs = 1500UL - static_cast<uint32_t>(1150.0f * gazeAlertness_);
      const uint32_t fixationRangeMs = 1600UL - static_cast<uint32_t>(1100.0f * gazeAlertness_);
      gazeNextAtMs_ = now + minimumFixationMs + nextGazeRandom() % fixationRangeMs;
    }
  } else if (static_cast<int32_t>(now - gazeNextAtMs_) >= 0) {
    const float xRange = 9.0f + gazeAlertness_ * 13.0f;
    const float yRange = 5.0f + gazeAlertness_ * 7.0f;
    const float randomX = static_cast<float>(static_cast<int16_t>(nextGazeRandom() >> 16)) / 32768.0f;
    const float randomY = static_cast<float>(static_cast<int16_t>(nextGazeRandom() >> 16)) / 32768.0f;
    gazeStartX_ = gazeX_;
    gazeStartY_ = gazeY_;
    gazeTargetX_ = randomX * xRange;
    gazeTargetY_ = randomY * yRange;
    const float distance = hypotf(gazeTargetX_ - gazeStartX_, gazeTargetY_ - gazeStartY_);
    if (distance < 4.0f) gazeTargetX_ = randomX < 0.0f ? -xRange : xRange;
    const float adjustedDistance = hypotf(gazeTargetX_ - gazeStartX_, gazeTargetY_ - gazeStartY_);
    gazeSaccadeDurationMs_ = static_cast<uint16_t>(
      60.0f + fminf(120.0f, adjustedDistance * 5.0f) * (1.0f - gazeAlertness_ * 0.45f));
    gazeSaccadeAtMs_ = now;
    gazeSaccading_ = true;
  }
  gazeX = gazeX_;
  gazeY = gazeY_;
}

void EyeRenderer::drawEmotionLids(uint8_t annoyance, uint8_t loneliness) {
  constexpr uint8_t kEmotionThreshold = 90;
  const uint16_t lidColor = TFT_BLACK;
  if (annoyance >= kEmotionThreshold) {
    const uint8_t intensity = annoyance - kEmotionThreshold;
    const int16_t edgeY = 31 + intensity / 5;
    const int16_t centerY = 39 + intensity / 2;
    canvas.fillTriangle(0, 0, 127, 0, 127, edgeY, lidColor);
    canvas.fillTriangle(0, 0, 127, edgeY, 64, centerY, lidColor);
    canvas.fillTriangle(0, 0, 64, centerY, 0, edgeY, lidColor);
  } else if (loneliness >= kEmotionThreshold) {
    const uint8_t intensity = loneliness - kEmotionThreshold;
    const int16_t edgeY = 35 + intensity / 4;
    const int16_t centerY = 25 - intensity / 4;
    canvas.fillTriangle(0, 0, 127, 0, 127, edgeY, lidColor);
    canvas.fillTriangle(0, 0, 127, edgeY, 64, centerY, lidColor);
    canvas.fillTriangle(0, 0, 64, centerY, 0, edgeY, lidColor);
  }
}

void EyeRenderer::drawMessagePanel(uint32_t now, const MessagePanel& panel) {
  constexpr uint8_t kLineBytes = 64;
  constexpr uint8_t kLineCount = 4;
  const uint16_t frame = color(152, 110, 255);
  const uint16_t corner = color(40, 212, 255);
  const uint16_t shade = color(0, 8, 22);
  const uint16_t blockBackground = color(0, 0, 0);

  // Alternating dark scanlines approximate a translucent panel on a 16-bit
  // sprite without allocating a second alpha canvas.
  for (int16_t y = 6; y < 122; ++y) {
    if ((y & 3) != 0) canvas.drawFastHLine(5, y, 118, shade);
  }
  canvas.drawFastHLine(12, 4, 104, frame);
  canvas.drawFastHLine(12, 123, 104, frame);
  canvas.drawFastVLine(4, 12, 104, frame);
  canvas.drawFastVLine(123, 12, 104, frame);
  canvas.drawLine(4, 12, 12, 4, frame);
  canvas.drawLine(116, 4, 123, 12, frame);
  canvas.drawLine(4, 115, 12, 123, frame);
  canvas.drawLine(116, 123, 123, 115, frame);
  canvas.fillRect(4, 13, 2, 10, corner);
  canvas.fillRect(122, 105, 2, 10, corner);

  canvas.setTextFont(1);
  canvas.setTextSize(2);
  for (uint8_t line = 0; line < kLineCount; ++line) {
    char text[kLineBytes] = {};
    panel.wrappedLine(now, line, text, sizeof(text));
    if (!text[0]) continue;
    const int16_t y = 13 + line * 22;
    const uint8_t columns = panelTextColumns(text, std::strlen(text));
    canvas.fillRoundRect(9, y - 3, columns * 12 + 8, 21, 3, blockBackground);
    drawPanelLine(text, 15, y + 2, color(0, 18, 55), true);
    drawPanelLine(text, 13, y, TFT_WHITE, false);
  }
  for (uint8_t line = 0; line < kLineCount; ++line) {
    const PanelGlyph glyph = panel.glyphAtRow(now, line);
    if (glyph == PanelGlyph::None) continue;
    const int16_t y = 13 + line * 22;
    canvas.fillRoundRect(43, y - 6, 42, 38, 5, blockBackground);
    drawCenteredGlyph(48, y - 2, glyph, true);
    drawCenteredGlyph(46, y - 4, glyph, false);
  }
  if (panel.moreTextBelow(now) && (now / 450U) % 2U == 0U) {
    canvas.fillTriangle(107, 108, 113, 108, 110, 113, TFT_WHITE);
  }
}

void EyeRenderer::draw(uint32_t now, uint8_t beatInBar, bool beatDotVisible,
                       bool beatDotIsSync, uint8_t vuLevel, uint8_t beatPulse,
                       uint8_t attention, uint8_t annoyance, uint8_t loneliness,
                       uint8_t bootProgress) {
  if (bootProgress < 255) {
    idleBreathStartedAtMs_ = 0;
    drawBootSequence(now, bootProgress);
    return;
  }
  if (idleBreathStartedAtMs_ == 0) idleBreathStartedAtMs_ = now;
  float gazeX;
  float gazeY;
  updateGaze(now, attention, gazeX, gazeY);
  if (loneliness >= 90) gazeY += static_cast<float>(loneliness - 90) * 4.0f / 10.0f;
  // Move the complete eye by up to three pixels over five seconds. The
  // procedural Sclera layer, Iris and pupil use the same scale.
  const float breathElapsedMs = static_cast<float>(now - idleBreathStartedAtMs_);
  const float breathScale = 0.975f + cosf(breathElapsedMs * TWO_PI / 5000.0f) * 0.025f;
  const int16_t gazeIrisX = 64 + static_cast<int16_t>(gazeX);
  const int16_t gazeIrisY = 64 + static_cast<int16_t>(gazeY);
  const int16_t irisX = 64 + static_cast<int16_t>(lroundf((gazeIrisX - 64) * breathScale));
  const int16_t irisY = 64 + static_cast<int16_t>(lroundf((gazeIrisY - 64) * breathScale));

  canvas.fillScreen(TFT_BLACK);
  if (lowPolyScleraReady) {
    if (lastLowPolyLayerAtMs == 0 || now - lastLowPolyLayerAtMs >= kLowPolyLayerIntervalMs) {
      const uint32_t layerStartedAtUs = micros();
      renderLowPolySclera(gazeX, gazeY, breathScale);
      recordLayerFrame(micros() - layerStartedAtUs);
      lastLowPolyLayerAtMs = now;
    }
    lowPolySclera.pushSprite(&canvas, 0, 0);
  } else {
    canvas.fillCircle(64, 64, static_cast<int16_t>(lroundf(61.0f * breathScale)),
                      color(154, 171, 191));
  }

  irisFacets.update(now, vuLevel);
  const int16_t irisRadius = static_cast<int16_t>(lroundf(
    (kIrisRadius + static_cast<int16_t>(beatPulse * 2U / 255U)) * breathScale));
  for (uint8_t ray = 0; ray < kIrisSegments; ++ray) {
    const Point first = {
      static_cast<int16_t>(irisBoundary[ray].x * irisRadius / kIrisRadius),
      static_cast<int16_t>(irisBoundary[ray].y * irisRadius / kIrisRadius),
    };
    const Point second = {
      static_cast<int16_t>(irisBoundary[(ray + 1) % kIrisSegments].x * irisRadius / kIrisRadius),
      static_cast<int16_t>(irisBoundary[(ray + 1) % kIrisSegments].y * irisRadius / kIrisRadius),
    };
    const uint8_t value = irisFacets.displayLevel(ray, kIrisBaseValue);
    const auto profile = annoyance >= 90 ? IrisFacets::Profile::Annoyed
                                         : IrisFacets::Profile::Default;
    const auto sectorColor = IrisFacets::shadedColor(ray, value, profile);
    canvas.fillTriangle(irisX, irisY, irisX + first.x, irisY + first.y,
                        irisX + second.x, irisY + second.y,
                        color(sectorColor.r, sectorColor.g, sectorColor.b));
  }
  drawPupil(irisX, irisY, beatPulse, breathScale);
  drawEmotionLids(annoyance, loneliness);

  if (beatDotVisible) {
    const uint16_t indicatorColor = beatDotIsSync
      ? color(255, 0, 0)
      : (beatInBar == 1 ? color(255, 210, 0) : color(255, 255, 255));
    canvas.fillCircle(116, 11, 5, TFT_BLACK);
    canvas.fillCircle(116, 11, 3, indicatorColor);
  }
}

void EyeRenderer::drawBootSequence(uint32_t now, uint8_t bootProgress) {
  // Leave enough frames for the flash to be perceived as a fade at the
  // renderer's 25 ms cadence rather than as a single white frame.
  constexpr uint8_t kFlashStartsAt = 196;
  canvas.fillScreen(TFT_BLACK);
  if (bootProgress >= kFlashStartsAt) {
    const uint8_t flashProgress = static_cast<uint8_t>(
      (bootProgress - kFlashStartsAt) * 255U / (255U - kFlashStartsAt));
    drawBootRevealEye(now);
    blendWhiteOverlay(static_cast<uint8_t>(255U - flashProgress));
    return;
  }

  // The cheerful jingle gets a lightweight frontal arrival: three simple
  // circles hop in, then a short flash hands over to the detailed iris.
  const float progress = static_cast<float>(bootProgress) / kFlashStartsAt;
  const float hop = fabsf(sinf(progress * 3.0f * PI)) * (1.0f - progress) * 25.0f;
  // Keep the proportions fixed while the complete eye approaches the screen.
  // A fourth-power ease-in keeps it almost still at first, then makes the
  // arrival visibly snap forward immediately before the flash.
  const float easedProgress = progress * progress * progress * progress;
  const float scale = 0.12f + easedProgress * 0.88f;
  const int16_t centerY = static_cast<int16_t>(lroundf(64.0f + hop));
  const int16_t scleraRadius = static_cast<int16_t>(lroundf(61.0f * scale));
  const int16_t irisRadius = static_cast<int16_t>(lroundf(kIrisRadius * scale));
  const int16_t pupilRadius = static_cast<int16_t>(lroundf(15.0f * scale));

  // Concentric, upper-right-offset circles approximate the final sclera's
  // light direction without per-pixel gradients or a second sprite.
  canvas.fillCircle(64, centerY, scleraRadius, color(47, 61, 88));
  canvas.fillCircle(64 + scleraRadius / 12, centerY - scleraRadius / 12,
                    scleraRadius * 11 / 12, color(105, 122, 150));
  canvas.fillCircle(64 + scleraRadius / 5, centerY - scleraRadius / 5,
                    scleraRadius * 3 / 4, color(173, 187, 201));
  canvas.fillCircle(64 + scleraRadius / 3, centerY - scleraRadius / 3,
                    scleraRadius / 2, color(255, 247, 227));
  canvas.fillCircle(64, centerY, irisRadius, color(14, 29, 58));
  canvas.fillCircle(64, centerY, pupilRadius, TFT_BLACK);
}

}  // namespace decaflash::mainframe
