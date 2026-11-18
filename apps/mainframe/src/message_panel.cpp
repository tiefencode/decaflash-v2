#include "message_panel.h"

#include "panel_glyphs.h"

#include <cstring>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kMessageMinimumDurationMs = 4000;
constexpr uint32_t kMessageDurationPerLineMs = 1000;
// Extra display time for scrolling messages. It covers the 4 s start hold, so
// the end of the text stays on screen as long as before (first step at 2 s).
constexpr uint32_t kScrollingMessageExtraMs = 2000;
constexpr uint32_t kMotionCooldownMs = 9000;
// Time from show() until the first one-line scroll step.
constexpr uint32_t kScrollStartMs = 4000;
constexpr uint32_t kScrollLineMs = 800;
// Time a model request may wait for its emoji before the fixed text is shown.
constexpr uint32_t kModelEmojiWaitMs = 2000;
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

}  // namespace

bool MessagePanel::show(uint32_t now, const char* text) {
  if (!text || !text[0]) return false;
  std::strncpy(text_, text, kTextCapacity - 1);
  text_[kTextCapacity - 1] = '\0';
  length_ = textLength(text_);
  wrappedLength_ = length_;
  leadingGlyph_ = PanelGlyph::None;
  for (uint16_t at = 0; at < length_;) {
    const auto token = panelGlyphToken(text_ + at, length_ - at);
    if (!token.bytes) break;
    if (at + token.bytes == length_ && token.glyph != PanelGlyph::None &&
        token.glyph != PanelGlyph::Aumlaut && token.glyph != PanelGlyph::Oumlaut &&
        token.glyph != PanelGlyph::Uumlaut && token.glyph != PanelGlyph::SharpS) {
      leadingGlyph_ = token.glyph;
      wrappedLength_ = at;
      while (wrappedLength_ && text_[wrappedLength_ - 1] == ' ') --wrappedLength_;
      break;
    }
    at += token.bytes;
  }
  requestOpen_ = false;
  growing_ = false;
  modelText_ = false;
  shownAtMs_ = now;
  startDisplayTime(now);
  return true;
}

void MessagePanel::startDisplayTime(uint32_t now) {
  const uint8_t totalLines = static_cast<uint8_t>(lineCount(kLineColumns) +
      (leadingGlyph_ == PanelGlyph::None ? 0 : 1));
  uint32_t duration = totalLines * kMessageDurationPerLineMs;
  if (totalLines > kVisibleLines) duration += kScrollingMessageExtraMs;
  if (duration < kMessageMinimumDurationMs) duration = kMessageMinimumDurationMs;
  untilMs_ = now + duration;
}

bool MessagePanel::accept(uint32_t now, bool shown, uint32_t lastAt) {
  return !(shown && now - lastAt < kMotionCooldownMs);
}

bool MessagePanel::showFromBank(uint32_t now, TextModelEvent event, const Mood& mood) {
  if (!bank_) return false;
  const uint8_t index = static_cast<uint8_t>(event);
  const char* text = bank_->pick(index, bank_->leadFor(mood));
  return text && show(now, text);
}

bool MessagePanel::showMotion(uint32_t now, const MotionEvent& event, const Mood& mood) {
  TextModelEvent textEvent;
  const uint8_t index = static_cast<uint8_t>(event.kind);
  if (!textModelEventFor(event, textEvent) || index >= 8) return false;
  if (!accept(now, motionShown_[index], lastMotionAtMs_[index])) return false;
  if (!showFromBank(now, textEvent, mood)) return false;
  motionShown_[index] = true;
  lastMotionAtMs_[index] = now;
  return true;
}

bool MessagePanel::showPresence(uint32_t now, const PresenceEvent& event, const Mood& mood) {
  TextModelEvent textEvent;
  const uint8_t index = static_cast<uint8_t>(event.kind);
  if (!textModelEventFor(event, textEvent) || index >= kPresenceEventKinds) return false;
  if (!accept(now, presenceShown_[index], lastPresenceAtMs_[index])) return false;
  if (!showFromBank(now, textEvent, mood)) return false;
  presenceShown_[index] = true;
  lastPresenceAtMs_[index] = now;
  return true;
}

bool MessagePanel::showDepressionPoem(uint32_t now, uint8_t sequence) {
  return show(now, kDepressionPoems[sequence % kDepressionPoemCount]);
}

void MessagePanel::cancelModelText(uint32_t now) {
  requestOpen_ = false;
  if (growing_) {
    // The cancelled text is not completed; it stays until a new message
    // replaces it, or for its display time.
    growing_ = false;
    startDisplayTime(now);
  }
}

bool MessagePanel::requestMotion(uint32_t now, const MotionEvent& event, const Mood& mood) {
  TextModelEvent textEvent;
  const uint8_t index = static_cast<uint8_t>(event.kind);
  if (!textModelEventFor(event, textEvent) || index >= 8) return false;
  if (!accept(now, motionShown_[index], lastMotionAtMs_[index])) return false;
  motionShown_[index] = true;
  lastMotionAtMs_[index] = now;
  openRequest(now, textEvent, mood);
  return true;
}

bool MessagePanel::requestPresence(uint32_t now, const PresenceEvent& event, const Mood& mood) {
  TextModelEvent textEvent;
  const uint8_t index = static_cast<uint8_t>(event.kind);
  if (!textModelEventFor(event, textEvent) || index >= kPresenceEventKinds) return false;
  if (!accept(now, presenceShown_[index], lastPresenceAtMs_[index])) return false;
  presenceShown_[index] = true;
  lastPresenceAtMs_[index] = now;
  openRequest(now, textEvent, mood);
  return true;
}

void MessagePanel::openRequest(uint32_t now, TextModelEvent event, const Mood& mood) {
  cancelModelText(now);
  requestOpen_ = true;
  requestEvent_ = event;
  requestMood_ = mood;  // the snapshot of the acceptance, for a late fallback
  requestedAtMs_ = now;
}

bool MessagePanel::requestTimedOut(uint32_t now) const {
  return requestOpen_ && now - requestedAtMs_ >= kModelEmojiWaitMs;
}

bool MessagePanel::fallBack(uint32_t now) {
  if (!requestOpen_) return false;
  requestOpen_ = false;
  return showFromBank(now, requestEvent_, requestMood_);
}

bool MessagePanel::startModelText(uint32_t now, PanelGlyph emoji) {
  if (!requestOpen_ || emoji == PanelGlyph::None || emoji == PanelGlyph::Aumlaut ||
      emoji == PanelGlyph::Oumlaut || emoji == PanelGlyph::Uumlaut ||
      emoji == PanelGlyph::SharpS) {
    return false;
  }
  requestOpen_ = false;
  text_[0] = '\0';
  length_ = 0;
  wrappedLength_ = 0;
  leadingGlyph_ = emoji;
  growing_ = true;
  modelText_ = true;
  shownAtMs_ = now;
  untilMs_ = now;
  return true;
}

bool MessagePanel::appendModelWord(const char* word) {
  if (!growing_ || !word || !word[0]) return false;
  const uint16_t wordLength = textLength(word);
  const uint16_t separator = length_ ? 1 : 0;
  if (length_ + separator + wordLength > kTextCapacity - 1) return false;
  if (separator) text_[length_++] = ' ';
  std::memcpy(text_ + length_, word, wordLength);
  length_ = static_cast<uint16_t>(length_ + wordLength);
  text_[length_] = '\0';
  wrappedLength_ = length_;
  return true;
}

bool MessagePanel::completeModelText(uint32_t now) {
  if (!growing_) return false;
  growing_ = false;
  startDisplayTime(now);
  return true;
}

bool MessagePanel::visible(uint32_t now) const {
  if (growing_) return true;
  return (length_ || leadingGlyph_ != PanelGlyph::None) &&
         static_cast<int32_t>(now - untilMs_) < 0;
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
  if (totalLines <= kVisibleLines) return 0;
  const uint8_t lastFirstLine = static_cast<uint8_t>(totalLines - kVisibleLines);
  // A model text moves up as it grows and keeps its newest rows in view.
  if (modelText_) return lastFirstLine;
  if (now - shownAtMs_ < kScrollStartMs) return 0;
  const uint32_t elapsed = now - shownAtMs_ - kScrollStartMs;
  const uint32_t steps = elapsed / kScrollLineMs + 1;
  return static_cast<uint8_t>(steps < lastFirstLine ? steps : lastFirstLine);
}

bool MessagePanel::moreTextBelow(uint32_t now) const {
  const uint8_t textLines = lineCount(kLineColumns);
  const uint8_t totalLines = static_cast<uint8_t>(textLines +
      (leadingGlyph_ == PanelGlyph::None ? 0 : 1));
  if (totalLines <= kVisibleLines) return false;
  return firstVisibleLine(now, totalLines) < totalLines - kVisibleLines;
}

PanelGlyph MessagePanel::glyphAtRow(uint32_t now, uint8_t row) const {
  if (leadingGlyph_ == PanelGlyph::None) return PanelGlyph::None;
  const uint8_t totalLines = static_cast<uint8_t>(lineCount(kLineColumns) + 1);
  const uint8_t firstLine = firstVisibleLine(now, totalLines);
  return firstLine + row == 0 ? leadingGlyph_ : PanelGlyph::None;
}

void MessagePanel::wrappedLine(uint32_t now, uint8_t row, char* output,
                               uint8_t capacity) const {
  if (!output || capacity < 2) return;
  output[0] = '\0';
  const uint8_t width = kLineColumns;
  const uint8_t textLines = lineCount(width);
  const uint8_t totalLines = static_cast<uint8_t>(textLines +
      (leadingGlyph_ == PanelGlyph::None ? 0 : 1));
  const uint8_t firstLine = firstVisibleLine(now, totalLines);
  // The leading emoji is element 0; text lines follow it.
  const uint8_t element = static_cast<uint8_t>(firstLine + row);
  const uint8_t leading = leadingGlyph_ == PanelGlyph::None ? 0 : 1;
  if (element < leading) return;
  const uint8_t wantedLine = static_cast<uint8_t>(element - leading);
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
