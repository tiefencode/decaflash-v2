#include "message_panel.h"

#include <cassert>
#include <cstring>
#include <cstdio>

using namespace decaflash::mainframe;

int main() {
  MessagePanel panel;
  assert(!panel.visible(0));
  assert(panel.show(100, "HELLO"));
  assert(panel.visible(5099));
  assert(!panel.visible(5100));
  assert(panel.show(5100, "NEXT MESSAGE"));
  char line[13] = {};
  panel.wrappedLine(5100, 0, line, sizeof(line));
  assert(std::strcmp(line, "NEXT MESSAGE") == 0);

  MotionEvent event;
  event.kind = MotionKind::MultiTap;
  event.count = 3;
  event.sequence = 2;
  assert(panel.showMotion(19000, event));
  panel.wrappedLine(19000, 0, line, sizeof(line));
  assert(std::strcmp(line, "NICHT SO") == 0);
  assert(!panel.showMotion(21000, event));
  event.kind = MotionKind::Impact;
  event.sequence = 0;
  assert(panel.showMotion(21000, event));
  panel.wrappedLine(21000, 0, line, sizeof(line));
  assert(std::strcmp(line, "AU. SANFTER!") == 0);

  MessagePanel variants;
  event.kind = MotionKind::Tap;
  event.sequence = 0;
  assert(variants.showMotion(0, event));
  variants.wrappedLine(0, 0, line, sizeof(line));
  assert(std::strcmp(line, "HUHU :)") == 0);
  event.sequence = 3;
  assert(variants.showMotion(9000, event));
  variants.wrappedLine(9000, 0, line, sizeof(line));
  assert(std::strcmp(line, "KLOPF") == 0);

  assert(panel.show(28000, "EINS ZWEI DREI VIER FUNF SECHS SIEBEN ACHT NEUN ZEHN"));
  panel.wrappedLine(28000, 0, line, sizeof(line));
  assert(std::strcmp(line, "EINS ZWEI") == 0);
  panel.wrappedLine(28000, 1, line, sizeof(line));
  assert(std::strcmp(line, "DREI VIER") == 0);
  panel.wrappedLine(30000, 0, line, sizeof(line));
  assert(std::strcmp(line, "DREI VIER") == 0);

  puts("PASS: panel lifetime, motion source, wrapping, and vertical scroll");
}
