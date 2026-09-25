#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>

#include "eye_renderer.h"
#include "espnow_transport.h"
#include "protocol.h"
#include "scene_programs.h"
#include "beat_analyzer.h"
#include "audio_follower.h"
#include "voice_base_input.h"

namespace {

constexpr uint16_t kDefaultBpm = 120;
constexpr uint8_t kBeatsPerBar = 4;
constexpr uint32_t kSceneRefreshMs = 30000;
constexpr uint32_t kBeatDotFlashMs = 140;

bool radioReady = false;
bool showRunning = false;
uint32_t nextBeatAtMs = 0;
uint32_t nextSceneRefreshAtMs = 0;
uint32_t beatDotUntilMs = 0;
bool beatDotIsSync = false;
uint32_t currentBar = 1;
uint8_t beatInBar = 1;
uint16_t currentBpm = kDefaultBpm;
size_t sceneIndex = 0;
decaflash::mainframe::EyeRenderer eyeRenderer;
decaflash::mainframe::BeatAnalyzer beatAnalyzer;
decaflash::mainframe::AudioFollower audioFollower;
decaflash::mainframe::VoiceBaseInput voiceBaseInput;

uint32_t beatIntervalMs() {
  return 60000UL / currentBpm;
}

bool checkPsramSample() {
  constexpr size_t bytes = 64 * 1024;
  auto* memory = static_cast<volatile uint8_t*>(
    heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!memory) return false;
  for (size_t i = 0; i < bytes; ++i) memory[i] = static_cast<uint8_t>(i ^ (i >> 8));
  bool ok = true;
  for (size_t i = 0; i < bytes; ++i) {
    if (memory[i] != static_cast<uint8_t>(i ^ (i >> 8))) {
      ok = false;
      break;
    }
  }
  heap_caps_free(const_cast<uint8_t*>(memory));
  return ok;
}

bool sendPacket(const void* packet, size_t packetSize) {
  if (!radioReady) return false;
  const esp_err_t result = esp_now_send(
    decaflash::espnow_transport::kBroadcastMac,
    static_cast<const uint8_t*>(packet), packetSize);
  return result == ESP_OK;
}

void sendMainframeHello() {
  const auto message = decaflash::protocol::makeMainframeHelloMessage();
  sendPacket(&message, sizeof(message));
}

void sendSceneSelect() {
  const auto message = decaflash::protocol::makeSceneSelectMessage(
    static_cast<uint8_t>(sceneIndex));
  sendPacket(&message, sizeof(message));
}

void sendClockSync() {
  const auto message = decaflash::protocol::makeClockSyncMessage(
    currentBpm, kBeatsPerBar, beatInBar, currentBar);
  sendPacket(&message, sizeof(message));
}

void triggerBeatDot(uint32_t now, bool isSync) {
  beatDotUntilMs = now + kBeatDotFlashMs;
  beatDotIsSync = isSync;
}

void startShow() {
  showRunning = true;
  currentBar = 1;
  beatInBar = 1;
  const uint32_t now = millis();
  nextBeatAtMs = now + beatIntervalMs();
  nextSceneRefreshAtMs = now + kSceneRefreshMs;
  sendSceneSelect();
  sendClockSync();
  triggerBeatDot(now, true);
}

void selectNextScene() {
  sceneIndex = (sceneIndex + 1U) % decaflash::scenes::kSceneCount;
  const uint32_t now = millis();
  sendSceneSelect();
  nextSceneRefreshAtMs = now + kSceneRefreshMs;
}

void applyAudioFollow(uint32_t now) {
  decaflash::mainframe::AudioFollowInput input = {};
  input.showRunning = showRunning;
  input.musicPresent = beatAnalyzer.musicPresent();
  input.clockBpm = beatAnalyzer.clockBpm();
  input.confidence = beatAnalyzer.confidence();
  input.onsetAtMs = beatAnalyzer.lastOnsetAtMs();
  input.currentBpm = currentBpm;
  input.nowMs = now;
  const auto output = audioFollower.update(input);
  if (!output.setBpm) return;

  currentBpm = output.bpm;
  if (output.acquired) {
    beatInBar = 1;
    ++currentBar;
    nextBeatAtMs = output.onsetAtMs + beatIntervalMs();
    sendClockSync();
    triggerBeatDot(now, true);
  }
}

void serviceShowClock(uint32_t now) {
  if (!showRunning) return;
  if (static_cast<int32_t>(now - nextBeatAtMs) >= 0) {
    if (beatInBar == 1) sendClockSync();
    if (beatInBar == kBeatsPerBar) {
      beatInBar = 1;
      ++currentBar;
    } else {
      ++beatInBar;
    }
    triggerBeatDot(now, false);
    nextBeatAtMs = now + beatIntervalMs();
  }
  if (static_cast<int32_t>(now - nextSceneRefreshAtMs) >= 0) {
    sendSceneSelect();
    nextSceneRefreshAtMs = now + kSceneRefreshMs;
  }
}

void initialiseRadio() {
  const auto init = decaflash::espnow_transport::initEspNow();
  if (!init.ok()) return;
  const auto peer = decaflash::espnow_transport::ensureBroadcastPeer();
  radioReady = peer.ok();
  if (radioReady) sendMainframeHello();
}

}  // namespace

void setup() {
  auto config = M5.config();
  config.internal_mic = false;
  config.internal_spk = false;
  config.external_speaker_value = 0;
  config.external_display_value = 0;
  config.internal_imu = false;
  config.internal_rtc = false;
  config.external_speaker.atomic_echo = true;
  M5.begin(config);
  M5.Display.setBrightness(64);

  checkPsramSample();
  voiceBaseInput.begin();
  initialiseRadio();
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    if (showRunning) {
      selectNextScene();
    } else {
      startShow();
    }
  }
  const uint32_t now = millis();
  voiceBaseInput.update(beatAnalyzer);
  applyAudioFollow(now);
  serviceShowClock(now);
  const bool beatDotVisible = showRunning &&
    static_cast<int32_t>(now - beatDotUntilMs) < 0;
  eyeRenderer.service(now, beatInBar, beatDotVisible, beatDotIsSync);
  delay(5);
}
