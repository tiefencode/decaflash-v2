#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "protocol.h"

namespace p = decaflash::protocol;

// Frozen V1 receiver contract, independent of the V2 constants under test.
// Source: original protocol v13 and espnow_transport::isValidHeader.
bool v1Accepts(const p::MessageHeader& header, p::MessageType expected) {
  return header.magic == 0x4443464C && header.version == 13 && header.type == expected;
}

template <typename Packet>
void checkPacket(const Packet& packet, p::MessageType expected) {
  assert(p::isValidHeader(packet.header, expected));
  assert(!v1Accepts(packet.header, expected));

  auto legacy = packet;
  legacy.header.magic = 0x4443464C;
  legacy.header.version = 13;
  assert(v1Accepts(legacy.header, expected));
  assert(!p::isValidHeader(legacy.header, expected));

  auto invalid = packet.header;
  invalid.version++;
  assert(!p::isValidHeader(invalid, expected));
  invalid = packet.header;
  invalid.magic = 0;
  assert(!p::isValidHeader(invalid, expected));
  for (auto other : {p::MessageType::SceneSelect, p::MessageType::ClockSync,
                     p::MessageType::MainframeHello, p::MessageType::NodeText}) {
    if (other != expected) assert(!p::isValidHeader(packet.header, other));
  }
}

int main() {
  static_assert(p::kProtocolMagic == 0x44434632, "V2 identity must remain stable");
  static_assert(sizeof(p::MessageHeader) == 8, "Header layout changed");
  static_assert(sizeof(p::SceneSelectMessage) == 12, "Scene layout changed");
  static_assert(sizeof(p::ClockSyncMessage) == 16, "Clock layout changed");
  static_assert(sizeof(p::MainframeHelloMessage) == 8, "Hello layout changed");
  static_assert(sizeof(p::NodeTextMessage) == 60, "Text layout changed");
  checkPacket(p::makeSceneSelectMessage(1), p::MessageType::SceneSelect);
  checkPacket(p::makeClockSyncMessage(120, 4, 1, 3), p::MessageType::ClockSync);
  checkPacket(p::makeMainframeHelloMessage(), p::MessageType::MainframeHello);
  for (auto kind : {decaflash::NodeKind::Flashlight, decaflash::NodeKind::RgbStrip}) {
    checkPacket(p::makeNodeTextMessage(kind, "TEST"), p::MessageType::NodeText);
    checkPacket(p::makeNodeTextMessage(kind, "", p::kNodeTextFlagCancel), p::MessageType::NodeText);
  }
  std::puts("PASS: V1/V2 mutual rejection, V2 acceptance, text/cancel, invalid headers, wire sizes");
}
