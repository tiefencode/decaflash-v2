#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <cmath>
#include "personality.h"
#include "message_panel.h"
#include "sound_engine.h"

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
constexpr uint32_t kLightingRefreshMs = 3000;

decaflash::mainframe::Personality personality;
decaflash::mainframe::MotionEvents motionEvents;
decaflash::mainframe::MessagePanel messagePanel;
bool moodDebug = false;
uint32_t lastMoodAtMs = 0;
uint32_t lastImuAtMs = 0;
uint32_t lastLightingSendAtMs = 0;
bool visualStateSent = false;
decaflash::NodeVisualState lastVisualState = {};
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
decaflash::mainframe::StartupSoundPreview startupSoundPreview;
decaflash::mainframe::MoodThresholdWatcher soundWatcher;
bool audioAnalysisRequested = false;
bool audioAnalysisStarted = false;

#if DECAFLASH_EYE_BENCHMARK
// Diagnostic-only cadence counters. They observe the normal main-loop work;
// no worker, radio or sensor scheduling is changed by benchmark builds.
struct EyeBenchmarkLoad {
  uint32_t startedAtMs = 0;
  uint32_t lastReportAtMs = 0;
  uint32_t lastAudioAtMs = 0;
  uint32_t lastMoodAtMs = 0;
  uint32_t lastSensorAtMs = 0;
  uint32_t audioCalls = 0;
  uint32_t moodUpdates = 0;
  uint32_t sensorUpdates = 0;
  uint32_t maxAudioGapMs = 0;
  uint32_t maxMoodGapMs = 0;
  uint32_t maxSensorGapMs = 0;
  uint32_t nodeTxAttempts = 0;
  uint32_t nodeTxRejected = 0;
};

EyeBenchmarkLoad eyeBenchmarkLoad;

void observeBenchmarkCadence(uint32_t now, uint32_t& previous, uint32_t& maximumGap,
                             uint32_t& calls) {
  if (previous != 0) {
    const uint32_t gap = now - previous;
    if (gap > maximumGap) maximumGap = gap;
  }
  previous = now;
  ++calls;
}

void reportEyeBenchmark(uint32_t now) {
  if (eyeBenchmarkLoad.startedAtMs == 0) eyeBenchmarkLoad.startedAtMs = now;
  if (now - eyeBenchmarkLoad.lastReportAtMs < 10000) return;
  eyeBenchmarkLoad.lastReportAtMs = now;
  const auto& renderer = eyeRenderer.benchmark();
  const uint32_t elapsedMs = now - eyeBenchmarkLoad.startedAtMs;
  const uint32_t outputFpsMilli = elapsedMs == 0 ? 0 : static_cast<uint32_t>(
    static_cast<uint64_t>(renderer.outputFrames) * 1000ULL * 1000ULL / elapsedMs);
  Serial.printf(
    "EYE_BENCH mode=%u triangles=%u layer_ms=%u output_fps=%.3f output_avg_us=%lu "
    "output_worst_us=%lu layer_avg_us=%lu layer_worst_us=%lu layer_frames=%lu\n",
    static_cast<unsigned>(renderer.mode), static_cast<unsigned>(renderer.triangles),
    static_cast<unsigned>(renderer.layerCadenceMs), outputFpsMilli / 1000.0f,
    static_cast<unsigned long>(renderer.outputAverageUs),
    static_cast<unsigned long>(renderer.outputWorstUs),
    static_cast<unsigned long>(renderer.layerAverageUs),
    static_cast<unsigned long>(renderer.layerWorstUs),
    static_cast<unsigned long>(renderer.layerFrames));
  Serial.printf(
    "EYE_MEM eye_bytes=%u psram_used=%u psram_free=%u psram_largest=%u "
    "psram_min_free=%u internal_used=%u internal_free=%u internal_largest=%u "
    "internal_min_free=%u emoji_cache_bytes=%u\n",
    static_cast<unsigned>(renderer.eyeAllocationBytes),
    static_cast<unsigned>(renderer.psramAllocationBytes),
    static_cast<unsigned>(renderer.psramFreeBytes),
    static_cast<unsigned>(renderer.psramLargestBlockBytes),
    static_cast<unsigned>(renderer.psramMinimumFreeBytes),
    static_cast<unsigned>(renderer.internalAllocationBytes),
    static_cast<unsigned>(renderer.internalFreeBytes),
    static_cast<unsigned>(renderer.internalLargestBlockBytes),
    static_cast<unsigned>(renderer.internalMinimumFreeBytes),
    static_cast<unsigned>(renderer.panelEmojiCacheBytes));
  Serial.printf(
    "EYE_PANEL png_decodes=%lu png_avg_us=%lu png_worst_us=%lu cache_blits=%lu\n",
    static_cast<unsigned long>(renderer.panelPngDecodes),
    static_cast<unsigned long>(renderer.panelPngDecodeAverageUs),
    static_cast<unsigned long>(renderer.panelPngDecodeWorstUs),
    static_cast<unsigned long>(renderer.panelEmojiCacheBlits));
  Serial.printf(
    "EYE_LOAD audio_calls=%lu audio_max_gap_ms=%lu audio_fresh=%u "
    "mood_updates=%lu mood_max_gap_ms=%lu sensor_updates=%lu sensor_max_gap_ms=%lu "
    "node_tx=%lu node_rejected=%lu\n",
    static_cast<unsigned long>(eyeBenchmarkLoad.audioCalls),
    static_cast<unsigned long>(eyeBenchmarkLoad.maxAudioGapMs),
    audioAnalysisStarted && voiceBaseInput.fresh(now) ? 1U : 0U,
    static_cast<unsigned long>(eyeBenchmarkLoad.moodUpdates),
    static_cast<unsigned long>(eyeBenchmarkLoad.maxMoodGapMs),
    static_cast<unsigned long>(eyeBenchmarkLoad.sensorUpdates),
    static_cast<unsigned long>(eyeBenchmarkLoad.maxSensorGapMs),
    static_cast<unsigned long>(eyeBenchmarkLoad.nodeTxAttempts),
    static_cast<unsigned long>(eyeBenchmarkLoad.nodeTxRejected));
}
#endif

decaflash::mainframe::MoodAudio moodAudio(uint32_t now) {
  decaflash::mainframe::MoodAudio input;
  if (!audioAnalysisStarted) return input;
  input.fresh = voiceBaseInput.fresh(now);
  const auto& features = voiceBaseInput.moodFeatures();
  input.silent = features.silent();
  input.bassValid = features.bassValid();
  input.bassPermille = features.bassPermille();
  // Once the show clock has an audio lock, Energy uses that same stable BPM
  // as the visible beat dot instead of a separate raw analyzer estimate.
  input.bpm = audioFollower.locked() ? currentBpm : beatAnalyzer.detectedBpm();
  input.confidence = beatAnalyzer.confidence();
  input.onsetAtMs = beatAnalyzer.lastOnsetAtMs();
  return input;
}

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
  if (!radioReady) {
#if DECAFLASH_EYE_BENCHMARK
    ++eyeBenchmarkLoad.nodeTxAttempts;
    ++eyeBenchmarkLoad.nodeTxRejected;
#endif
    return false;
  }
  const esp_err_t result = esp_now_send(
    decaflash::espnow_transport::kBroadcastMac,
    static_cast<const uint8_t*>(packet), packetSize);
  const bool accepted = result == ESP_OK;
#if DECAFLASH_EYE_BENCHMARK
  ++eyeBenchmarkLoad.nodeTxAttempts;
  if (!accepted) ++eyeBenchmarkLoad.nodeTxRejected;
#endif
  return accepted;
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

decaflash::NodeVisualState visualStateForMood(const decaflash::mainframe::Mood& mood) {
  decaflash::NodeVisualState state;
  if (mood.annoyance >= 90) {
    state.rgbMode = decaflash::RgbRenderMode::Solid;
    state.colorRed = 255;
    state.flashOverride = decaflash::FlashOverride::Full;
  } else if (mood.loneliness >= 90) {
    state.brightnessPercent = 15;
    state.flashOverride = decaflash::FlashOverride::Off;
  }
  return state;
}

bool sameVisualState(
  const decaflash::NodeVisualState& left,
  const decaflash::NodeVisualState& right
) {
  return left.rgbMode == right.rgbMode &&
    left.brightnessPercent == right.brightnessPercent &&
    left.colorRed == right.colorRed && left.colorGreen == right.colorGreen &&
    left.colorBlue == right.colorBlue && left.overlayRed == right.overlayRed &&
    left.overlayGreen == right.overlayGreen && left.overlayBlue == right.overlayBlue &&
    left.overlayOpacityPercent == right.overlayOpacityPercent &&
    left.flashOverride == right.flashOverride;
}

void sendVisualState(const decaflash::NodeVisualState& state) {
  const auto message = decaflash::protocol::makeNodeVisualStateMessage(state);
  if (sendPacket(&message, sizeof(message))) {
    lastVisualState = state;
    visualStateSent = true;
    lastLightingSendAtMs = millis();
  }
}

void serviceVisualState(uint32_t now, const decaflash::mainframe::Mood& mood) {
  const auto state = visualStateForMood(mood);
  if (!visualStateSent || !sameVisualState(state, lastVisualState) ||
      now - lastLightingSendAtMs >= kLightingRefreshMs) {
    sendVisualState(state);
  }
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
  config.internal_imu = true;
  config.internal_rtc = false;
  config.external_speaker.atomic_echo = true;
  M5.begin(config);
  M5.Display.setBrightness(64);
  M5.BtnA.setHoldThresh(1200);

  checkPsramSample();
  eyeRenderer.begin();
  const bool previewReady = startupSoundPreview.begin();
  initialiseRadio();
  Serial.printf("Sound preview: %s; audio analysis waits for first button press\n",
                previewReady ? "ready" : "failed");
}

void loop() {
  M5.update();
  if (M5.BtnA.wasHold()) {
    moodDebug = !moodDebug;
  }
  if (M5.BtnA.wasClicked()) {
    if (showRunning) {
      selectNextScene();
    } else {
      startShow();
      audioAnalysisRequested = true;
    }
  }
  const uint32_t now = millis();
  const bool previewFinished = startupSoundPreview.service(now);
  if (audioAnalysisRequested && previewFinished && !audioAnalysisStarted) {
    audioAnalysisStarted = voiceBaseInput.begin();
    Serial.printf("Audio analysis: %s\n", audioAnalysisStarted ? "started" : "failed");
  }
  if (audioAnalysisStarted) {
#if DECAFLASH_EYE_BENCHMARK
    observeBenchmarkCadence(now, eyeBenchmarkLoad.lastAudioAtMs,
                            eyeBenchmarkLoad.maxAudioGapMs, eyeBenchmarkLoad.audioCalls);
#endif
    voiceBaseInput.update(beatAnalyzer);
  }
  if (audioAnalysisStarted) applyAudioFollow(now);
  serviceShowClock(now);
  if (now - lastMoodAtMs >= 100) {
#if DECAFLASH_EYE_BENCHMARK
    observeBenchmarkCadence(now, eyeBenchmarkLoad.lastMoodAtMs,
                            eyeBenchmarkLoad.maxMoodGapMs, eyeBenchmarkLoad.moodUpdates);
#endif
    lastMoodAtMs = now;
    personality.update(now, moodAudio(now));
  }
  if (now - lastImuAtMs >= 20 && M5.Imu.isEnabled()) {
#if DECAFLASH_EYE_BENCHMARK
    observeBenchmarkCadence(now, eyeBenchmarkLoad.lastSensorAtMs,
                            eyeBenchmarkLoad.maxSensorGapMs, eyeBenchmarkLoad.sensorUpdates);
#endif
    lastImuAtMs = now;
    const auto updated = M5.Imu.update();
    const auto required = m5::IMU_Class::sensor_mask_accel | m5::IMU_Class::sensor_mask_gyro;
    if ((updated & required) == required) {
      const auto data = M5.Imu.getImuData();
      decaflash::mainframe::MotionSample sample = {
        data.accel.x, data.accel.y, data.accel.z,
        data.gyro.x, data.gyro.y, data.gyro.z};
      const auto event = motionEvents.feed(now, sample);
      if (event.kind != decaflash::mainframe::MotionKind::None) {
        personality.update(now, moodAudio(now));
        personality.onMotion(event);
        messagePanel.showMotion(now, event);
      }
    }
  }
  const auto mood = personality.snapshot();
  decaflash::mainframe::ThresholdCrossingEvent soundEvent;
  if (soundWatcher.update(mood, now,
                          !audioAnalysisRequested && startupSoundPreview.availableForMoodSound(),
                          soundEvent)) {
    startupSoundPreview.playMoodSound(soundEvent);
  }
  serviceVisualState(now, mood);
  const bool beatDotVisible = showRunning &&
    static_cast<int32_t>(now - beatDotUntilMs) < 0;
  const uint8_t beatPulse = beatDotVisible
    ? static_cast<uint8_t>((beatDotUntilMs - now) * 255UL / kBeatDotFlashMs)
    : 0;
  eyeRenderer.service(now, beatInBar, beatDotVisible, beatDotIsSync,
                      audioAnalysisStarted ? voiceBaseInput.vuLevel(millis()) : 0, beatPulse, mood.attention,
                      mood.annoyance, mood.loneliness, startupSoundPreview.bootProgress(now),
                      moodDebug ? &mood : nullptr,
                      moodDebug ? &motionEvents.latest() : nullptr,
                      &messagePanel);
#if DECAFLASH_EYE_BENCHMARK
  reportEyeBenchmark(now);
#endif
  delay(5);
}
