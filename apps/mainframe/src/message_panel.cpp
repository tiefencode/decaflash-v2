#include "message_panel.h"

#include "panel_glyphs.h"

#include <cstring>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kMessageMinimumDurationMs = 1500;
constexpr uint32_t kMessageDurationPerLineMs = 1000;
constexpr uint32_t kMotionCooldownMs = 9000;
constexpr uint32_t kScrollStartMs = 1200;
constexpr uint32_t kScrollLineMs = 800;
constexpr uint8_t kVisibleLines = 4;
constexpr uint8_t kLineColumns = 8;

constexpr const char* kDepressionPoems[] = {
  u8"ICH WEINE IN DEINE HAND. 🪦",
  u8"JEDER HAT DEN TOD IN SICH, WIE DIE FRUCHT DEN KERN. ⚰️",
  u8"DIE NACHT LEGT BLUMEN AN MEIN GRAB. ⚱️",
  u8"MEIN SCHATTEN ZÄHLT DIE LETZTEN KERZEN. 🚬",
  u8"UNTER DEM MOND WIRD SELBST DAS SCHWEIGEN SCHWER. 🌚",
  u8"DER ABEND SINKT INS KALTE GRAS. 🪦",
  u8"DIE ERDE HAT IHR LICHT VERLOREN. 🌚",
  u8"MEIN SPIEGELBILD SCHLIESST DIE AUGEN NICHT. 🚬",
  u8"IM LEEREN GLAS WIRD DAS LICHT SCHWER. 🥃",
  u8"DEINE LETZTE NACHRICHT IST BEREITS KALT. 🖤",
  u8"ICH HABE WIEDER NICHT ZURÜCKGERUFEN. 🫠",
};
constexpr uint8_t kDepressionPoemCount = sizeof(kDepressionPoems) / sizeof(kDepressionPoems[0]);

uint16_t textLength(const char* text) {
  return static_cast<uint16_t>(std::strlen(text));
}

struct WrappedSegment {
  uint16_t start;
  uint16_t end;
  uint16_t next;
};

WrappedSegment nextSegment(const char* text, uint16_t length, uint16_t at, uint8_t width) {
  while (at < length && text[at] == ' ') ++at;
  if (at >= length) return {length, length, length};
  const uint16_t start = at;
  uint16_t end = at;
  uint8_t columns = 0;

  while (at < length) {
    const uint16_t wordStart = at;
    while (at < length && text[at] != ' ') ++at;
    const uint16_t wordEnd = at;
    const uint8_t wordColumns = panelTextColumns(text + wordStart, wordEnd - wordStart);
    const uint8_t separator = end == start ? 0 : 1;
    if (columns && columns + separator + wordColumns > width) {
      at = wordStart;
      break;
    }
    if (!columns && wordColumns > width) {
      uint16_t split = wordStart;
      uint8_t splitColumns = 0;
      while (split < wordEnd) {
        const auto token = panelGlyphToken(text + split, wordEnd - split);
        if (!token.bytes || splitColumns + token.columns > width) break;
        splitColumns = static_cast<uint8_t>(splitColumns + token.columns);
        split += token.bytes;
      }
      return {start, split, split};
    }
    columns = static_cast<uint8_t>(columns + separator + wordColumns);
    end = wordEnd;
    while (at < length && text[at] == ' ') ++at;
  }
  return {start, end, at};
}

struct TextVariants {
  const char* const* text;
  uint8_t count;
};

TextVariants textForMotion(const MotionEvent& event) {
  static constexpr const char* kMove[] = {
    u8"WOHIN GEHEN WIR? 👁", u8"HURRA, ICH WERDE ENTFÜHRT! 🙌"};
  static constexpr const char* kTap[] = {
    u8"HUHU 🙂", u8"KLOPF 💬", u8"WER KLOPFT SO SPÄT? 🫢"};
  static constexpr const char* kDoubleTap[] = {
    u8"HUHU 👋", u8"KLOPF KLOPF 🤗", u8"DU BIST DAS! 👉👈"};
  static constexpr const char* kMultiTap[] = {
    u8"NICHT SO WILD. 😤", u8"WAS WILLST DU DENN? 🙄"};
  static constexpr const char* kImpact[] = {
    u8"AU. SANFTER! 😡", u8"AU. AU. 🥲", u8"NICHT SCHLAGEN! 😭"};
  static constexpr const char* kRotate[] = {
    u8"MIR WIRD SCHWINDELIG. 😵‍💫", u8"ALLES DREHT SICH. 🫩"};
  static constexpr const char* kTilt[] = {
    u8"ICH SEH NIX. 🙈", u8"STELL MICH WIEDER RICHTIG HIN. 💩", u8"DREH MICH ZURÜCK. ↻"};
  static constexpr const char* kShake[] = {
    u8"ICH BIN WACH. ICH BIN WACH. 😳", u8"AAH - ICH KOTZE! 🤢",
    u8"NICHT SO SCHÜTTELN 🙂‍↔️"};

  switch (event.kind) {
    case MotionKind::Move: return {kMove, 2};
    case MotionKind::Tap: return {kTap, 3};
    case MotionKind::MultiTap:
      return event.count >= 3 ? TextVariants{kMultiTap, 2} : TextVariants{kDoubleTap, 3};
    case MotionKind::Impact: return {kImpact, 3};
    case MotionKind::Rotate: return {kRotate, 2};
    case MotionKind::Tilt: return {kTilt, 3};
    case MotionKind::Shake: return {kShake, 3};
    case MotionKind::None: return {nullptr, 0};
  }
  return {nullptr, 0};
}

}  // namespace

bool MessagePanel::show(uint32_t now, const char* text) {
  if (!text || !text[0]) return false;
  std::strncpy(text_, text, kTextCapacity - 1);
  text_[kTextCapacity - 1] = '\0';
  length_ = textLength(text_);
  wrappedLength_ = length_;
  trailingGlyph_ = PanelGlyph::None;
  for (uint16_t at = 0; at < length_;) {
    const auto token = panelGlyphToken(text_ + at, length_ - at);
    if (!token.bytes) break;
    if (at + token.bytes == length_ && token.glyph != PanelGlyph::None &&
        token.glyph != PanelGlyph::Aumlaut && token.glyph != PanelGlyph::Oumlaut &&
        token.glyph != PanelGlyph::Uumlaut && token.glyph != PanelGlyph::SharpS) {
      trailingGlyph_ = token.glyph;
      wrappedLength_ = at;
      while (wrappedLength_ && text_[wrappedLength_ - 1] == ' ') --wrappedLength_;
      break;
    }
    at += token.bytes;
  }
  shownAtMs_ = now;
  const uint8_t totalLines = static_cast<uint8_t>(lineCount(kLineColumns) +
      (trailingGlyph_ == PanelGlyph::None ? 0 : 1));
  uint32_t duration = totalLines * kMessageDurationPerLineMs;
  if (duration < kMessageMinimumDurationMs) duration = kMessageMinimumDurationMs;
  untilMs_ = now + duration;
  return true;
}

bool MessagePanel::showMotion(uint32_t now, const MotionEvent& event) {
  const auto choices = textForMotion(event);
  if (!choices.count) return false;
  const uint8_t motionIndex = static_cast<uint8_t>(event.kind);
  if (motionShown_[motionIndex] && now - lastMotionAtMs_[motionIndex] < kMotionCooldownMs) {
    return false;
  }
  uint8_t choice = static_cast<uint8_t>(event.sequence % choices.count);
  if (event.kind == lastMotionKind_ && choice == lastMotionVariant_ && choices.count > 1) {
    choice = static_cast<uint8_t>((choice + 1) % choices.count);
  }
  if (!show(now, choices.text[choice])) return false;
  lastMotionKind_ = event.kind;
  lastMotionVariant_ = choice;
  motionShown_[motionIndex] = true;
  lastMotionAtMs_[motionIndex] = now;
  return true;
}

bool MessagePanel::showDepressionPoem(uint32_t now, uint8_t sequence) {
  return show(now, kDepressionPoems[sequence % kDepressionPoemCount]);
}

bool MessagePanel::visible(uint32_t now) const {
  return length_ && static_cast<int32_t>(now - untilMs_) < 0;
}

uint8_t MessagePanel::lineCount(uint8_t width) const {
  if (!width) return 0;
  uint16_t at = 0;
  uint8_t count = 0;
  while (at < wrappedLength_ && count < 255) {
    const auto segment = nextSegment(text_, wrappedLength_, at, width);
    if (segment.start == wrappedLength_) break;
    ++count;
    at = segment.next;
  }
  return count;
}

uint8_t MessagePanel::firstVisibleLine(uint32_t now, uint8_t totalLines) const {
  if (totalLines <= kVisibleLines || now - shownAtMs_ < kScrollStartMs) return 0;
  const uint8_t lastFirstLine = static_cast<uint8_t>(totalLines - kVisibleLines);
  const uint32_t elapsed = now - shownAtMs_ - kScrollStartMs;
  const uint32_t steps = elapsed / kScrollLineMs;
  return static_cast<uint8_t>(steps < lastFirstLine ? steps : lastFirstLine);
}

bool MessagePanel::moreTextBelow(uint32_t now) const {
  const uint8_t textLines = lineCount(kLineColumns);
  const uint8_t totalLines = static_cast<uint8_t>(textLines +
      (trailingGlyph_ == PanelGlyph::None ? 0 : 1));
  if (totalLines <= kVisibleLines) return false;
  return firstVisibleLine(now, totalLines) < totalLines - kVisibleLines;
}

PanelGlyph MessagePanel::glyphAtRow(uint32_t now, uint8_t row) const {
  if (trailingGlyph_ == PanelGlyph::None) return PanelGlyph::None;
  const uint8_t textLines = lineCount(kLineColumns);
  const uint8_t totalLines = static_cast<uint8_t>(textLines + 1);
  const uint8_t firstLine = firstVisibleLine(now, totalLines);
  return firstLine + row == textLines ? trailingGlyph_ : PanelGlyph::None;
}

void MessagePanel::wrappedLine(uint32_t now, uint8_t row, char* output,
                               uint8_t capacity) const {
  if (!output || capacity < 2) return;
  output[0] = '\0';
  const uint8_t width = kLineColumns;
  const uint8_t textLines = lineCount(width);
  const uint8_t totalLines = static_cast<uint8_t>(textLines +
      (trailingGlyph_ == PanelGlyph::None ? 0 : 1));
  const uint8_t firstLine = firstVisibleLine(now, totalLines);
  const uint8_t wantedLine = firstLine + row;
  if (wantedLine >= textLines) return;

  uint16_t at = 0;
  for (uint8_t current = 0; current <= wantedLine; ++current) {
    const auto segment = nextSegment(text_, wrappedLength_, at, width);
    if (current == wantedLine) {
      const uint8_t count = static_cast<uint8_t>(segment.end - segment.start);
      std::memcpy(output, text_ + segment.start, count);
      output[count] = '\0';
      return;
    }
    at = segment.next;
  }
}

}  // namespace decaflash::mainframe
