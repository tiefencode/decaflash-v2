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
constexpr int16_t kPanelEmojiSize = 32;
constexpr size_t kPanelEmojiCacheBytes = kPanelEmojiSize * kPanelEmojiSize * 2;

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
M5Canvas panelEmojiCache(&M5.Display);
IrisFacets irisFacets;
Point irisBoundary[kIrisSegments];
Point pupilBoundary[12];
bool lowPolyScleraReady = false;
bool panelEmojiCacheReady = false;
PanelGlyph cachedPanelEmoji = PanelGlyph::None;
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

bool panelTextSpriteIndex(PanelGlyph glyph, uint8_t& index) {
  switch (glyph) {
    case PanelGlyph::Smile: index = 0; return true;
    case PanelGlyph::Wave: index = 1; return true;
    case PanelGlyph::Eye: index = 2; return true;
    case PanelGlyph::Dizzy: index = 3; return true;
    case PanelGlyph::Nauseous: index = 4; return true;
    case PanelGlyph::Warning: index = 5; return true;
    case PanelGlyph::AngerBurst: index = 6; return true;
    case PanelGlyph::Speech: index = 7; return true;
    case PanelGlyph::Music: index = 8; return true;
    case PanelGlyph::Loop: index = 9; return true;
    case PanelGlyph::Love: index = 10; return true;
    case PanelGlyph::Laugh: index = 11; return true;
    case PanelGlyph::Melt: index = 12; return true;
    case PanelGlyph::HeartEyes: index = 13; return true;
    case PanelGlyph::Kiss: index = 14; return true;
    case PanelGlyph::TearSmile: index = 15; return true;
    case PanelGlyph::Hug: index = 16; return true;
    case PanelGlyph::Shy: index = 17; return true;
    case PanelGlyph::HeadTurn: index = 18; return true;
    case PanelGlyph::Plead: index = 19; return true;
    case PanelGlyph::Cry: index = 20; return true;
    case PanelGlyph::Scream: index = 21; return true;
    case PanelGlyph::Angry: index = 22; return true;
    case PanelGlyph::Skull: index = 23; return true;
    case PanelGlyph::Poop: index = 24; return true;
    case PanelGlyph::SeeNoEvil: index = 25; return true;
    case PanelGlyph::OkHand: index = 26; return true;
    case PanelGlyph::PointTogether: index = 27; return true;
    case PanelGlyph::Praise: index = 28; return true;
    case PanelGlyph::Blush: index = 29; return true;
    case PanelGlyph::EyeRoll: index = 30; return true;
    case PanelGlyph::Exhausted: index = 31; return true;
    case PanelGlyph::Relaxed: index = 32; return true;
    case PanelGlyph::Sad: index = 33; return true;
    case PanelGlyph::DizzySpiral: index = 34; return true;
    case PanelGlyph::Pleading: index = 35; return true;
    case PanelGlyph::Huffy: index = 36; return true;
    case PanelGlyph::Swearing: index = 37; return true;
    case PanelGlyph::SkullCrossbones: index = 38; return true;
    case PanelGlyph::BlackHeart: index = 39; return true;
    case PanelGlyph::Knife: index = 40; return true;
    case PanelGlyph::Glass: index = 41; return true;
    case PanelGlyph::Moon: index = 42; return true;
    case PanelGlyph::Coffin: index = 43; return true;
    case PanelGlyph::Cigarette: index = 44; return true;
    case PanelGlyph::Grave: index = 45; return true;
    case PanelGlyph::Urn: index = 46; return true;
    default: return false;
  }
}

bool decodeTextSprite(M5Canvas& target, int16_t x, int16_t y, PanelGlyph glyph,
                      int16_t width = kPanelEmojiSize, int16_t height = kPanelEmojiSize,
                      float scale = 1.0f) {
  uint8_t index = 0;
  if (!panelTextSpriteIndex(glyph, index)) return false;
  return target.drawPng(kPanelTextSpritesPng, kPanelTextSpritesPngLength, x, y,
                        width, height, (index % 8) * kPanelEmojiSize,
                        (index / 8) * kPanelEmojiSize, scale, scale);
}

bool drawInlineTextSprite(int16_t x, int16_t y, PanelGlyph glyph) {
  return decodeTextSprite(canvas, x, y, glyph, 16, 16, 0.5f);
}

void drawPanelLine(const char* text, int16_t x, int16_t y, uint16_t textColor,
                   bool shadow) {
  for (size_t at = 0, length = std::strlen(text); at < length;) {
    const auto token = panelGlyphToken(text + at, length - at);
    if (!token.bytes) break;

    if (token.glyph != PanelGlyph::None && token.glyph != PanelGlyph::Aumlaut &&
        token.glyph != PanelGlyph::Oumlaut && token.glyph != PanelGlyph::Uumlaut &&
        token.glyph != PanelGlyph::SharpS) {
      if (!shadow) drawInlineTextSprite(x + 4, y - 1, token.glyph);
      x += token.columns * 12;
      at += token.bytes;
      continue;
    }

    char character = text[at];
    if (token.glyph == PanelGlyph::Aumlaut) character =
        (text[at] == '\xc3' && text[at + 1] == '\x84') ? 'A' : 'a';
    if (token.glyph == PanelGlyph::Oumlaut) character =
        (text[at] == '\xc3' && text[at + 1] == '\x96') ? 'O' : 'o';
    if (token.glyph == PanelGlyph::Uumlaut) character =
        (text[at] == '\xc3' && text[at + 1] == '\x9c') ? 'U' : 'u';
    if (token.glyph == PanelGlyph::SharpS) character = 'B';
    if (static_cast<uint8_t>(character) < 0x20 ||
        static_cast<uint8_t>(character) >= 0x80) character = '?';

    canvas.setTextColor(textColor);
    canvas.setCursor(x, y);
    canvas.print(character);
    if (token.glyph == PanelGlyph::Aumlaut || token.glyph == PanelGlyph::Oumlaut ||
        token.glyph == PanelGlyph::Uumlaut) {
      canvas.fillRect(x + 2, y, 3, 3, textColor);
      canvas.fillRect(x + 8, y, 3, 3, textColor);
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
  sampleBenchmarkMemory();
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
    panelEmojiCache.setColorDepth(16);
    panelEmojiCacheReady = panelEmojiCache.createSprite(kPanelEmojiSize, kPanelEmojiSize) != nullptr;
    if (panelEmojiCacheReady) {
      panelEmojiCache.fillScreen(TFT_BLACK);
      benchmark_.panelEmojiCacheBytes = kPanelEmojiCacheBytes;
    }
    benchmark_.triangles = kLowPolyTriangles;
    benchmark_.layerCadenceMs = kLowPolyLayerIntervalMs;
    captureBenchmarkMemory(psramBefore, internalBefore);
  }
  return canvasReady_;
}

void EyeRenderer::captureBenchmarkMemory(size_t psramBefore, size_t internalBefore) {
  benchmark_.psramFreeBytes = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  benchmark_.psramMinimumFreeBytes = benchmark_.psramFreeBytes;
  benchmark_.psramLargestBlockBytes = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  benchmark_.internalFreeBytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  benchmark_.internalMinimumFreeBytes = benchmark_.internalFreeBytes;
  benchmark_.internalLargestBlockBytes = heap_caps_get_largest_free_block(
    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  benchmark_.psramAllocationBytes = psramBefore - benchmark_.psramFreeBytes;
  benchmark_.internalAllocationBytes = internalBefore - benchmark_.internalFreeBytes;
  benchmark_.eyeAllocationBytes = benchmark_.psramAllocationBytes +
    benchmark_.internalAllocationBytes;
}

void EyeRenderer::sampleBenchmarkMemory() {
#if DECAFLASH_EYE_BENCHMARK
  const size_t psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  const size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (psramFree < benchmark_.psramMinimumFreeBytes) benchmark_.psramMinimumFreeBytes = psramFree;
  if (internalFree < benchmark_.internalMinimumFreeBytes) {
    benchmark_.internalMinimumFreeBytes = internalFree;
  }
#endif
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

void EyeRenderer::recordPanelPngDecode(uint32_t durationUs) {
  ++benchmark_.panelPngDecodes;
  benchmark_.panelPngDecodeAverageUs = static_cast<uint32_t>(
    (static_cast<uint64_t>(benchmark_.panelPngDecodeAverageUs) *
      (benchmark_.panelPngDecodes - 1U) + durationUs) / benchmark_.panelPngDecodes);
  if (durationUs > benchmark_.panelPngDecodeWorstUs) {
    benchmark_.panelPngDecodeWorstUs = durationUs;
  }
}

bool EyeRenderer::refreshPanelEmojiCache(PanelGlyph glyph) {
  if (!panelEmojiCacheReady) return false;
  if (glyph == cachedPanelEmoji) return true;

  panelEmojiCache.fillScreen(TFT_BLACK);
  const uint32_t decodeStartedAtUs = micros();
  const bool decoded = decodeTextSprite(panelEmojiCache, 0, 0, glyph);
  recordPanelPngDecode(micros() - decodeStartedAtUs);
  if (decoded) cachedPanelEmoji = glyph;
  return decoded;
}

void EyeRenderer::drawCachedPanelEmoji(int16_t x, int16_t y, PanelGlyph glyph) {
  if (!refreshPanelEmojiCache(glyph)) return;
  panelEmojiCache.pushSprite(&canvas, x, y);
  ++benchmark_.panelEmojiCacheBlits;
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
    drawCachedPanelEmoji(46, y - 4, glyph);
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
