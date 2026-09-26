#include "message_panel.h"

#include <cstring>

namespace decaflash::mainframe {
namespace {

constexpr uint32_t kMessageDurationMs = 5000;
constexpr uint32_t kMotionCooldownMs = 9000;
constexpr uint32_t kScrollStartMs = 1200;
constexpr uint32_t kScrollLineMs = 800;
constexpr uint8_t kVisibleLines = 4;

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
  uint16_t end = static_cast<uint16_t>(at + width);
  if (end > length) end = length;
  if (end < length && text[end] != ' ') {
    uint16_t split = end;
    while (split > at && text[split - 1] != ' ') --split;
    if (split > at) end = split;
  }
  while (end > at && text[end - 1] == ' ') --end;
  uint16_t next = end;
  while (next < length && text[next] == ' ') ++next;
  return {at, end, next};
}

struct TextVariants {
  const char* const* text;
  uint8_t count;
};

TextVariants textForMotion(const MotionEvent& event) {
  static constexpr const char* kMove[] = {
    "WOHIN GEHEN WIR?", "HURRA, ICH WERDE ENTFUEHRT!"};
  static constexpr const char* kTap[] = {
    "HUHU :)", "KLOPF", "WER KLOPFT SO SPAET?"};
  static constexpr const char* kDoubleTap[] = {
    "HUHU :)", "KLOPF KLOPF", "DU BIST DAS!"};
  static constexpr const char* kMultiTap[] = {
    "NICHT SO WILD.", "WAS WILLST DU DENN?"};
  static constexpr const char* kImpact[] = {
    "AU. SANFTER!", "AU. AU. :(", "NICHT SCHLAGEN :("};
  static constexpr const char* kRotate[] = {
    "MIR WIRD SCHWINDELIG.", "ALLES DREHT SICH."};
  static constexpr const char* kTilt[] = {
    "ICH SEH NIX.", "STELL MICH WIEDER RICHTIG HIN.", "DREH MICH ZURUECK."};
  static constexpr const char* kShake[] = {
    "ICH BIN WACH. ICH BIN WACH.", "AAH - ICH KOTZE!"};

  switch (event.kind) {
    case MotionKind::Move: return {kMove, 2};
    case MotionKind::Tap: return {kTap, 3};
    case MotionKind::MultiTap:
      return event.count >= 3 ? TextVariants{kMultiTap, 2} : TextVariants{kDoubleTap, 3};
    case MotionKind::Impact: return {kImpact, 3};
    case MotionKind::Rotate: return {kRotate, 2};
    case MotionKind::Tilt: return {kTilt, 3};
    case MotionKind::Shake: return {kShake, 2};
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
  shownAtMs_ = now;
  untilMs_ = now + kMessageDurationMs;
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

bool MessagePanel::visible(uint32_t now) const {
  return length_ && static_cast<int32_t>(now - untilMs_) < 0;
}

uint8_t MessagePanel::lineCount(uint8_t width) const {
  if (!width) return 0;
  uint16_t at = 0;
  uint8_t count = 0;
  while (at < length_ && count < 255) {
    const auto segment = nextSegment(text_, length_, at, width);
    if (segment.start == length_) break;
    ++count;
    at = segment.next;
  }
  return count;
}

void MessagePanel::wrappedLine(uint32_t now, uint8_t row, char* output,
                               uint8_t capacity) const {
  if (!output || capacity < 2) return;
  output[0] = '\0';
  const uint8_t width = capacity - 1;
  const uint8_t totalLines = lineCount(width);
  uint8_t firstLine = 0;
  if (totalLines > kVisibleLines && now - shownAtMs_ >= kScrollStartMs) {
    const uint32_t elapsed = now - shownAtMs_ - kScrollStartMs;
    firstLine = static_cast<uint8_t>((elapsed / kScrollLineMs) %
                                     (totalLines + kVisibleLines));
  }
  const uint8_t wantedLine = firstLine + row;
  if (wantedLine >= totalLines) return;

  uint16_t at = 0;
  for (uint8_t current = 0; current <= wantedLine; ++current) {
    const auto segment = nextSegment(text_, length_, at, width);
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
