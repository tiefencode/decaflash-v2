#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>

#include "espnow_transport.h"
#include "protocol.h"
#include "scene_programs.h"

namespace {

constexpr uint16_t kDefaultBpm = 120;
constexpr uint8_t kBeatsPerBar = 4;
constexpr uint32_t kSceneRefreshMs = 30000;
constexpr uint32_t kReportIntervalMs = 2000;

bool psramSampleOk = false;
bool radioReady = false;
bool showRunning = false;
uint32_t lastReportAtMs = 0;
uint32_t nextBeatAtMs = 0;
uint32_t nextSceneRefreshAtMs = 0;
uint32_t currentBar = 1;
uint8_t beatInBar = 1;
size_t sceneIndex = 0;

uint32_t beatIntervalMs() {
  return 60000UL / kDefaultBpm;
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

void report() {
  Serial.printf(
    "MAINFRAME chip=%s rev=%u flash=%u psram=%u sample64k=%s board_id=%d display=%dx%d radio=%s show=%s scene=%u bpm=%u\n",
    ESP.getChipModel(), ESP.getChipRevision(), ESP.getFlashChipSize(),
    ESP.getPsramSize(), psramSampleOk ? "PASS" : "FAIL",
    static_cast<int>(M5.getBoard()), M5.Display.width(), M5.Display.height(),
    radioReady ? "READY" : "FAILED", showRunning ? "RUNNING" : "IDLE",
    static_cast<unsigned>(sceneIndex), kDefaultBpm);
}

void drawStatus() {
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.fillRect(0, 0, 42, 12, TFT_RED);
  M5.Display.fillRect(42, 0, 43, 12, TFT_GREEN);
  M5.Display.fillRect(85, 0, 43, 12, TFT_BLUE);
  M5.Display.drawRect(0, 0, 128, 128, TFT_WHITE);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(5, 16);
  M5.Display.printf("MAINFRAME\nPSRAM %s\nRADIO %s\nSHOW  %s\nSCENE %u/%u\nBPM   %u",
    psramSampleOk ? "PASS" : "FAIL", radioReady ? "OK" : "ERR",
    showRunning ? "ON" : "OFF", static_cast<unsigned>(sceneIndex + 1),
    static_cast<unsigned>(decaflash::scenes::kSceneCount), kDefaultBpm);
}

void probeVoiceBus() {
  const auto internalPort = M5.In_I2C.getPort();
  if (internalPort != I2C_NUM_0 && internalPort != I2C_NUM_1) {
    Serial.println("MAINFRAME voice_i2c=SKIP internal_bus_unknown");
    return;
  }
  M5.Ex_I2C.release();
  const auto voicePort = internalPort == I2C_NUM_0 ? I2C_NUM_1 : I2C_NUM_0;
  if (!M5.Ex_I2C.begin(voicePort, 38, 39)) {
    Serial.println("MAINFRAME voice_i2c=FAIL init");
    return;
  }
  unsigned found = 0;
  for (uint8_t address = 8; address < 0x78; ++address) {
    if (M5.Ex_I2C.scanID(address)) {
      Serial.printf("MAINFRAME voice_i2c_ack=0x%02x\n", address);
      ++found;
    }
  }
  Serial.printf("MAINFRAME voice_i2c_devices=%u (ACK is not an audio test)\n", found);
  M5.Ex_I2C.release();
}

bool sendPacket(const void* packet, size_t packetSize, const char* label) {
  if (!radioReady) return false;
  const esp_err_t result = esp_now_send(
    decaflash::espnow_transport::kBroadcastMac,
    static_cast<const uint8_t*>(packet), packetSize);
  if (result != ESP_OK) {
    Serial.printf("RADIO send=%s result=%d\n", label, static_cast<int>(result));
    return false;
  }
  return true;
}

void sendMainframeHello() {
  const auto message = decaflash::protocol::makeMainframeHelloMessage();
  sendPacket(&message, sizeof(message), "hello");
}

void sendSceneSelect() {
  const auto message = decaflash::protocol::makeSceneSelectMessage(
    static_cast<uint8_t>(sceneIndex));
  if (sendPacket(&message, sizeof(message), "scene")) {
    Serial.printf("RADIO scene=%u name=%s\n", static_cast<unsigned>(sceneIndex),
      decaflash::scenes::sceneName(sceneIndex));
  }
}

void sendClockSync() {
  const auto message = decaflash::protocol::makeClockSyncMessage(
    kDefaultBpm, kBeatsPerBar, beatInBar, currentBar);
  sendPacket(&message, sizeof(message), "clock");
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
  drawStatus();
  report();
}

void selectNextScene() {
  sceneIndex = (sceneIndex + 1U) % decaflash::scenes::kSceneCount;
  const uint32_t now = millis();
  sendSceneSelect();
  nextSceneRefreshAtMs = now + kSceneRefreshMs;
  drawStatus();
  report();
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
    nextBeatAtMs = now + beatIntervalMs();
  }
  if (static_cast<int32_t>(now - nextSceneRefreshAtMs) >= 0) {
    sendSceneSelect();
    nextSceneRefreshAtMs = now + kSceneRefreshMs;
  }
}

void initialiseRadio() {
  const auto init = decaflash::espnow_transport::initEspNow();
  if (!init.ok()) {
    Serial.printf("RADIO init=FAILED netif=%d event=%d wifi=%d mode=%d start=%d channel=%d espnow=%d\n",
      static_cast<int>(init.netifInit), static_cast<int>(init.eventLoopCreate),
      static_cast<int>(init.wifiInit), static_cast<int>(init.wifiSetMode),
      static_cast<int>(init.wifiStart), static_cast<int>(init.wifiSetChannel),
      static_cast<int>(init.espNowInit));
    return;
  }
  const auto peer = decaflash::espnow_transport::ensureBroadcastPeer();
  radioReady = peer.ok();
  Serial.printf("RADIO init=%s peer=%s channel=%u\n", radioReady ? "READY" : "FAILED",
    peer.alreadyExisted ? "existing" : (peer.ok() ? "added" : "failed"),
    decaflash::espnow_transport::kWifiChannel);
  if (radioReady) sendMainframeHello();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t started = millis();
  while (!Serial && millis() - started < 2000) delay(10);

  auto config = M5.config();
  config.internal_mic = false;
  config.internal_spk = false;
  config.external_speaker_value = 0;
  config.external_display_value = 0;
  config.internal_imu = false;
  config.internal_rtc = false;
  M5.begin(config);
  M5.Display.setBrightness(64);

  psramSampleOk = checkPsramSample();
  initialiseRadio();
  drawStatus();
  report();
  probeVoiceBus();
  Serial.println("MAINFRAME button=start/cycle scene; serial 'i'=voice I2C probe");
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
  if (Serial.available() && Serial.read() == 'i') probeVoiceBus();

  const uint32_t now = millis();
  serviceShowClock(now);
  if (now - lastReportAtMs >= kReportIntervalMs) {
    lastReportAtMs = now;
    report();
  }
  delay(5);
}
