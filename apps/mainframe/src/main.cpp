#include <Arduino.h>
#include <M5Unified.h>
#include <esp_heap_caps.h>
#include <cmath>
#include "debug_view.h"
#include "personality.h"
#include "presence_debug.h"
#include "presence_decision.h"
#include "presence_sensor.h"
#include "message_panel.h"
#include "motion_trace.h"
#include "sound_engine.h"

#include "eye_renderer.h"
#include "espnow_transport.h"
#include "protocol.h"
#include "scene_programs.h"
#include "show_clock.h"
#include "slm/slm_task.h"
#include "v2_tempo_adoption.h"
#include "voice_base_input.h"

namespace {

constexpr uint16_t kDefaultBpm = 120;
constexpr uint8_t kBeatsPerBar = 4;
constexpr uint32_t kSceneRefreshMs = 30000;
constexpr uint32_t kBeatDotFlashMs = 140;
constexpr uint32_t kLightingRefreshMs = 3000;
constexpr uint32_t kDepressionPoemIntervalMs = 120000;
constexpr uint32_t kSerialTxTimeoutMs = 10;

#ifndef DECAFLASH_BEAT_TRACKER_TELEMETRY
#define DECAFLASH_BEAT_TRACKER_TELEMETRY 0
#endif

#ifndef DECAFLASH_MOTION_TRACE
#define DECAFLASH_MOTION_TRACE 0
#endif

#ifndef DECAFLASH_LOOP_LOAD
#define DECAFLASH_LOOP_LOAD 0
#endif

#if DECAFLASH_PANEL_FRAME_TIMING && !DECAFLASH_LOOP_LOAD
#error "DECAFLASH_PANEL_FRAME_TIMING extends the loop-load report"
#endif

#ifndef DECAFLASH_SLM_AUTO_RUN
#define DECAFLASH_SLM_AUTO_RUN 0
#endif

#if DECAFLASH_SLM_AUTO_RUN && !DECAFLASH_LOOP_LOAD
#error "DECAFLASH_SLM_AUTO_RUN is a measurement sequence for the loop-load report"
#endif

decaflash::mainframe::Personality personality;
decaflash::mainframe::MotionEvents motionEvents;
decaflash::mainframe::MessagePanel messagePanel;
decaflash::mainframe::DebugViewState debugView;
// Presence samples reach the rest of the Mainframe only through the presence
// decision: attention base, loneliness, presence events and their texts.
decaflash::mainframe::PresenceSensor presenceSensor;
decaflash::mainframe::PresenceDebug presenceDebug;
decaflash::mainframe::PresenceDecision presenceDecision;
uint32_t presenceDroppedLines = 0;
uint32_t lastMoodAtMs = 0;
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
decaflash::mainframe::V2TempoAdoption v2TempoAdoption;
size_t sceneIndex = 0;
decaflash::mainframe::EyeRenderer eyeRenderer;
decaflash::mainframe::VoiceBaseInput voiceBaseInput;
decaflash::mainframe::StartupSoundPreview startupSoundPreview;
decaflash::mainframe::MoodThresholdWatcher soundWatcher;
bool audioAnalysisRequested = false;
bool audioAnalysisStarted = false;
uint32_t depressionPoemDueAtMs = 0;
uint8_t depressionPoemSequence = 0;

#if DECAFLASH_MOTION_TRACE
// Measurement only: reports raw IMU activity below the classifier thresholds
// next to the classifier's own result. The classifier behaves as in the
// normal build.
constexpr uint8_t kImuAddress = 0x68;  // BMI270 on the AtomS3R internal bus
constexpr uint8_t kMotionRawPerLine = 16;
decaflash::mainframe::MotionTrace motionTrace;
decaflash::mainframe::MotionTraceWindow motionTraceOut;
bool motionTraceSummaryPending = false;
uint8_t motionTraceRawSent = 0;
uint32_t motionTraceDropped = 0;

void reportImuConfig() {
  const auto reg = [](uint8_t address) {
    return static_cast<unsigned>(M5.In_I2C.readRegister8(kImuAddress, address, 400000));
  };
  Serial.printf("MOTION_CFG chip_id=0x%02x acc_conf=0x%02x acc_range=0x%02x "
                "gyr_conf=0x%02x gyr_range=0x%02x\n",
                reg(0x00), reg(0x40), reg(0x41), reg(0x42), reg(0x43));
}

// Writes at most one line per loop pass and only when it fits the transmit
// buffer, so capture never stalls audio analysis.
void serviceMotionTraceOutput() {
  if (Serial.availableForWrite() < 128) return;
  const auto& w = motionTraceOut;
  if (motionTraceSummaryPending) {
    motionTraceSummaryPending = false;
    Serial.printf("MOTION_WIN n=%lu t=%lu peak_mg=%u peak_dps=%u peak_at_ms=%u "
                  "over_ms=%u/%u/%u/%u/%u samples=%u dropped=%lu\n",
                  static_cast<unsigned long>(w.sequence),
                  static_cast<unsigned long>(w.startMs), w.peakMg, w.peakDps, w.peakAtMs,
                  w.overMs[0], w.overMs[1], w.overMs[2], w.overMs[3], w.overMs[4],
                  w.samples, static_cast<unsigned long>(motionTraceDropped));
    return;
  }
  if (motionTraceRawSent >= w.rawCount) return;
  char line[128];
  int used = snprintf(line, sizeof(line), "MOTION_RAW n=%lu i=%u",
                      static_cast<unsigned long>(w.sequence), motionTraceRawSent);
  for (uint8_t i = 0; i < kMotionRawPerLine && motionTraceRawSent < w.rawCount; ++i) {
    used += snprintf(line + used, sizeof(line) - used, " %u", w.rawMg[motionTraceRawSent++]);
  }
  Serial.println(line);
}

void traceMotionSample(uint32_t now, const decaflash::mainframe::MotionSample& sample) {
  using Output = decaflash::mainframe::MotionTrace::Output;
  const auto output = motionTrace.feed(now, sample);
  if (output == Output::Window) {
    // A window still being printed is replaced; the summary counts the loss.
    if (motionTraceSummaryPending || motionTraceRawSent < motionTraceOut.rawCount) {
      ++motionTraceDropped;
    }
    motionTraceOut = motionTrace.window();
    motionTraceSummaryPending = true;
    motionTraceRawSent = 0;
  } else if (output == Output::Idle && Serial.availableForWrite() >= 128) {
    const auto& idle = motionTrace.idle();
    Serial.printf("MOTION_IDLE t=%lu max_mg=%u max_dps=%u samples=%u max_gap_ms=%u\n",
                  static_cast<unsigned long>(idle.atMs), idle.maxMg, idle.maxDps,
                  idle.samples, idle.maxGapMs);
  }
}
#endif

#if DECAFLASH_BEAT_TRACKER_TELEMETRY
uint32_t lastTempoTelemetryAtMs = 0;

void reportTempoTelemetry(uint32_t now) {
  if (!audioAnalysisStarted || now - lastTempoTelemetryAtMs < 1000U) return;
  lastTempoTelemetryAtMs = now;
  const auto& tempo = voiceBaseInput.v2Tempo();
  const auto& metrics = voiceBaseInput.v2TempoMetrics();
  const uint32_t averageUs = metrics.processedFrames == 0 ? 0 :
    metrics.totalMicros / metrics.processedFrames;
  Serial.printf(
    "BEAT_V2 bpm=%u confidence=%u raw_bpm=%u raw_conf=%u direct_bpm=%u "
    "periodicity=%u harmonic=%u harmonic_periodicity=%u safety=%u show_bpm=%u evaluations=%lu "
    "tracker_avg_us=%lu tracker_max_us=%lu analysis_drops=%lu "
    "requeue_failures=%lu backlog_max=%u pcm_ring_bytes=%u psram_free=%u internal_free=%u fresh=%u\n",
    static_cast<unsigned>(tempo.bpm), static_cast<unsigned>(tempo.confidence),
    static_cast<unsigned>(tempo.rawBpm), static_cast<unsigned>(tempo.rawConfidence),
    static_cast<unsigned>(tempo.directBpm),
    static_cast<unsigned>(tempo.periodicityPermille),
    static_cast<unsigned>(tempo.harmonicBpm),
    static_cast<unsigned>(tempo.harmonicPeriodicityPermille),
    tempo.confidence >= 30U ? 1U : 0U, static_cast<unsigned>(currentBpm),
    static_cast<unsigned long>(tempo.tempoEvaluations),
    static_cast<unsigned long>(averageUs), static_cast<unsigned long>(metrics.maxMicros),
    static_cast<unsigned long>(voiceBaseInput.analysisDrops()),
    static_cast<unsigned long>(voiceBaseInput.requeueFailures()),
    static_cast<unsigned>(voiceBaseInput.analysisBacklogHighWater()),
    static_cast<unsigned>(decaflash::mainframe::VoiceBaseInput::analysisRingBytes()),
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
    voiceBaseInput.fresh(now) ? 1U : 0U);
}
#endif

#if DECAFLASH_LOOP_LOAD
// Measurement only: per-section timing of loop() on core 1, reported once per
// second. It observes the normal loop work and changes no scheduling.
struct LoopSection {
  uint32_t count = 0;
  uint64_t sumUs = 0;
  uint32_t maxUs = 0;
  void add(uint32_t us) {
    ++count;
    sumUs += us;
    if (us > maxUs) maxUs = us;
  }
  uint32_t averageUs() const { return count == 0 ? 0 : static_cast<uint32_t>(sumUs / count); }
};

struct LoopLoad {
  uint32_t windowStartedAtUs = 0;
  uint32_t lastReportAtMs = 0;
  LoopSection busy;          // whole loop() pass without delay(5) and this report
  LoopSection period;        // start of one pass to start of the next
  LoopSection input;         // M5.update, button handling, sound preview service
  LoopSection audio;         // voiceBaseInput.update + applyV2Tempo
  LoopSection showClock;
  LoopSection presence;
  LoopSection personality;   // periodic personality.update
  LoopSection imu;           // M5.Imu.update + motion classifier
  LoopSection other;         // poetry, sound watcher, visual state, eye arguments
  LoopSection eyeIdle;       // eyeRenderer.service passes without a frame
  LoopSection eyeFrame;      // frames without a visible panel message
  LoopSection eyePanelFrame; // frames with a visible panel message
  uint32_t lastPassStartUs = 0;
  uint32_t lastAudioBlockAtMs = 0;
  uint32_t maxAudioBlockGapMs = 0;
  uint32_t lastProcessedFrames = 0;
  uint32_t audioBlocks = 0;
  uint32_t lastImuAtMs = 0;
  uint32_t maxImuGapMs = 0;
  uint32_t imuSamples = 0;
  uint32_t txAttempts = 0;    // ESP-NOW packets handed to the radio
  uint32_t txRejected = 0;    // not accepted by esp_now_send or radio not ready
  uint32_t beats = 0;
  uint32_t maxBeatLateMs = 0; // loop serviced a show beat this long after its planned time
#if DECAFLASH_PANEL_FRAME_TIMING
  LoopSection panelEye;       // sections of frames with the message panel
  LoopSection panelShade;
  LoopSection panelWrap;
  LoopSection panelText;
  LoopSection panelEmoji;
  LoopSection panelPush;
  LoopSection motion;         // personality and panel work for one motion event
  uint32_t panelFirstFrames = 0;
  uint32_t panelEmojiDecodes = 0;
#endif
};

LoopLoad loopLoad;

uint32_t elapsedUs(uint32_t& mark) {
  const uint32_t nowUs = micros();
  const uint32_t elapsed = nowUs - mark;
  mark = nowUs;
  return elapsed;
}

void observeGap(uint32_t now, uint32_t& previous, uint32_t& maximumGap) {
  if (previous != 0 && now - previous > maximumGap) maximumGap = now - previous;
  previous = now;
}

// The report is split into short lines: HWCDC's transmit buffer holds fewer
// bytes than one full report. At most one line goes out per loop pass, and
// only when it fits the buffer, so reporting never waits on the host.
#if DECAFLASH_SLM && DECAFLASH_PANEL_FRAME_TIMING
#error "DECAFLASH_SLM and DECAFLASH_PANEL_FRAME_TIMING both add a sixth report line"
#endif
constexpr uint8_t kLoopLoadLines = (DECAFLASH_SLM || DECAFLASH_PANEL_FRAME_TIMING) ? 6 : 5;
char loopLoadLines[kLoopLoadLines][200];
uint8_t loopLoadLinesPending = 0;
uint8_t loopLoadLineNext = 0;
uint32_t loopLoadDroppedReports = 0;

#if DECAFLASH_PANEL_FRAME_TIMING
// Single events between the one-second reports: slow or first panel frames and
// large audio or IMU gaps. Lines that do not fit the queue are counted, not kept.
constexpr uint8_t kTimingEventLines = 16;
char timingEventLines[kTimingEventLines][200];
uint8_t timingEventHead = 0;
uint8_t timingEventCount = 0;
uint32_t timingEventsDropped = 0;
uint32_t passMotionUs = 0;
constexpr uint32_t kSlowPanelFrameUs = 30000;
constexpr uint32_t kReportedAudioGapMs = 60;
constexpr uint32_t kReportedImuGapMs = 55;

char* nextTimingEventLine() {
  if (timingEventCount >= kTimingEventLines) {
    ++timingEventsDropped;
    return nullptr;
  }
  char* line = timingEventLines[(timingEventHead + timingEventCount) % kTimingEventLines];
  ++timingEventCount;
  return line;
}

void queueGapEvent(uint32_t now, const char* kind, uint32_t previous, uint32_t threshold) {
  if (previous == 0 || now - previous < threshold) return;
  char* line = nextTimingEventLine();
  if (!line) return;
  snprintf(line, 200, "GAP t=%lu kind=%s gap_ms=%lu\n", static_cast<unsigned long>(now), kind,
           static_cast<unsigned long>(now - previous));
}

void queuePanelFrameEvent(uint32_t now, uint32_t passUs) {
  const auto& f = eyeRenderer.lastPanelFrameTiming();
  auto& l = loopLoad;
  l.panelEye.add(f.eyeUs);
  l.panelShade.add(f.shadeUs);
  l.panelWrap.add(f.wrapUs);
  l.panelText.add(f.textUs);
  l.panelEmoji.add(f.emojiUs);
  l.panelPush.add(f.pushUs);
  if (f.firstOfText) ++l.panelFirstFrames;
  l.panelEmojiDecodes += f.emojiDecodes;
  if (!f.firstOfText && f.totalUs < kSlowPanelFrameUs) return;
  char* line = nextTimingEventLine();
  if (!line) return;
  snprintf(line, 200,
    "PANEL_F t=%lu first=%u glyph=%u shown=%lu total=%lu eye=%lu shade=%lu wrap=%lu text=%lu "
    "inline=%lu/%u emoji=%lu decode=%lu/%u push=%lu motion=%lu pass=%lu atlas=%u\n",
    static_cast<unsigned long>(now), f.firstOfText ? 1U : 0U, static_cast<unsigned>(f.glyph),
    static_cast<unsigned long>(f.textShownAtMs), static_cast<unsigned long>(f.totalUs),
    static_cast<unsigned long>(f.eyeUs), static_cast<unsigned long>(f.shadeUs),
    static_cast<unsigned long>(f.wrapUs), static_cast<unsigned long>(f.textUs),
    static_cast<unsigned long>(f.inlineDecodeUs), static_cast<unsigned>(f.inlineDecodes),
    static_cast<unsigned long>(f.emojiUs), static_cast<unsigned long>(f.emojiDecodeUs),
    static_cast<unsigned>(f.emojiDecodes), static_cast<unsigned long>(f.pushUs),
    static_cast<unsigned long>(passMotionUs), static_cast<unsigned long>(passUs),
    eyeRenderer.panelAtlasEnabled() ? 1U : 0U);
}

// Automatic texts, so the comparison needs no knocking: 10 s after audio
// analysis starts, ten texts every 3 s with the earlier PNG paths (phase 1),
// then the same ten with the emoji atlas (phase 2). Consecutive texts change
// the trailing emoji; the last one also has an inline emoji.
constexpr const char* kTimingDemoTexts[] = {
  u8"KLOPF 💬", u8"WER IST DA? 🙂", u8"NICHT SO WILD. 😤", u8"AU. SANFTER! 😡",
  u8"AU. AU. 🥲", u8"ICH SEH NIX. 🙈", u8"DU BIST DAS! 👉👈", u8"AAH - ICH KOTZE! 🤢",
  u8"SCHON WEG? 😢", u8"HALLO 🌚 DU 🤗"};
constexpr uint8_t kTimingDemoTextCount = sizeof(kTimingDemoTexts) / sizeof(kTimingDemoTexts[0]);
constexpr uint32_t kTimingDemoStartMs = 10000;
constexpr uint32_t kTimingDemoStepMs = 3000;
uint32_t timingDemoAudioAtMs = 0;
uint8_t timingDemoStep = 0;  // texts shown so far, both phases
uint8_t timingDemoPhase = 0; // 0 before, 1 PNG paths, 2 atlas, 3 done

void serviceTimingDemo(uint32_t now) {
  if (!audioAnalysisStarted || timingDemoPhase == 3) return;
  if (timingDemoAudioAtMs == 0) timingDemoAudioAtMs = now;
  const uint32_t elapsed = now - timingDemoAudioAtMs;
  if (elapsed < kTimingDemoStartMs) return;
  // One step slot is left empty between the phases.
  const uint32_t slot = (elapsed - kTimingDemoStartMs) / kTimingDemoStepMs;
  const uint32_t wanted = slot < kTimingDemoTextCount ? slot + 1
    : slot == kTimingDemoTextCount ? kTimingDemoTextCount : slot;
  if (wanted <= timingDemoStep) return;
  if (timingDemoStep >= 2 * kTimingDemoTextCount) {
    timingDemoPhase = 3;
    eyeRenderer.setPanelAtlasEnabled(true);
    return;
  }
  timingDemoPhase = timingDemoStep < kTimingDemoTextCount ? 1 : 2;
  eyeRenderer.setPanelAtlasEnabled(timingDemoPhase == 2);
  messagePanel.show(now, kTimingDemoTexts[timingDemoStep % kTimingDemoTextCount]);
  ++timingDemoStep;
}

bool serviceTimingEventOutput() {
  if (!timingEventCount) return false;
  const char* line = timingEventLines[timingEventHead];
  const size_t length = strlen(line);
  if (Serial.availableForWrite() < static_cast<int>(length)) return true;
  Serial.write(reinterpret_cast<const uint8_t*>(line), length);
  timingEventHead = static_cast<uint8_t>((timingEventHead + 1) % kTimingEventLines);
  --timingEventCount;
  return true;
}
#endif

// Per-pass timing of the calls between the IMU block and the eye, which the
// one-second report only sums up as "other". A pass longer than kStallUs,
// every pass that tries to start a creature sound, and the pass in which the
// creature sounds become ready queue one LOOP_STALL line.
enum PassCall : uint8_t {
  kPassPrepare, kPassPoetry, kPassWatcher, kPassMoodSound, kPassSnore, kPassVisual, kPassEyeArgs,
  kPassEye, kPassCallCount
};
constexpr uint32_t kStallUs = 80000;
constexpr uint8_t kStallLines = 8;
uint32_t loopPassUs[kPassCallCount];
int8_t loopPassSound = -1;      // -1 none, else SoundState * 2 + direction, 10 = snore
bool loopPassSoundPlayed = false;
bool loopSoundsWereReady = false;
char loopStallLines[kStallLines][200];
uint8_t loopStallHead = 0;
uint8_t loopStallTail = 0;
uint32_t loopStallsDropped = 0;

void reportLoopPass(uint32_t now, uint32_t busyUs) {
  const bool ready = startupSoundPreview.creatureSoundsReady();
  const bool becameReady = ready && !loopSoundsWereReady;
  loopSoundsWereReady = ready;
  if (busyUs < kStallUs && loopPassSound < 0 && !becameReady) return;
  const uint8_t next = static_cast<uint8_t>((loopStallHead + 1U) % kStallLines);
  if (next == loopStallTail) {
    ++loopStallsDropped;
    return;
  }
  const auto us = [](PassCall call) { return static_cast<unsigned long>(loopPassUs[call]); };
  snprintf(loopStallLines[loopStallHead], sizeof(loopStallLines[0]),
    "LOOP_STALL t=%lu busy=%lu prep=%lu poem=%lu watch=%lu mood=%lu snore=%lu vis=%lu args=%lu "
    "eye=%lu sound=%d played=%u ready=%u sleep=%u panel=%u drop=%lu\n",
    static_cast<unsigned long>(now), static_cast<unsigned long>(busyUs), us(kPassPrepare),
    us(kPassPoetry), us(kPassWatcher), us(kPassMoodSound), us(kPassSnore), us(kPassVisual),
    us(kPassEyeArgs), us(kPassEye), static_cast<int>(loopPassSound),
    loopPassSoundPlayed ? 1U : 0U, ready ? 1U : 0U, personality.sleeping() ? 1U : 0U,
    messagePanel.visible(now) ? 1U : 0U, static_cast<unsigned long>(loopStallsDropped));
  loopStallHead = next;
}

void serviceLoopLoadOutput() {
  if (loopStallTail != loopStallHead) {
    const char* stall = loopStallLines[loopStallTail];
    const size_t stallLength = strlen(stall);
    if (Serial.availableForWrite() < static_cast<int>(stallLength)) return;
    Serial.write(reinterpret_cast<const uint8_t*>(stall), stallLength);
    loopStallTail = static_cast<uint8_t>((loopStallTail + 1U) % kStallLines);
    return;
  }
#if DECAFLASH_PANEL_FRAME_TIMING
  if (loopLoadLineNext >= loopLoadLinesPending) {
    serviceTimingEventOutput();
    return;
  }
#endif
  if (loopLoadLineNext >= loopLoadLinesPending) return;
  const char* line = loopLoadLines[loopLoadLineNext];
  const size_t length = strlen(line);
  if (Serial.availableForWrite() < static_cast<int>(length)) return;
  Serial.write(reinterpret_cast<const uint8_t*>(line), length);
  ++loopLoadLineNext;
}

void formatSection(char* out, size_t size, const LoopSection& section) {
  snprintf(out, size, "%lu/%lu/%lu", static_cast<unsigned long>(section.count),
           static_cast<unsigned long>(section.averageUs()),
           static_cast<unsigned long>(section.maxUs));
}

void reportLoopLoad(uint32_t now) {
  auto& l = loopLoad;
  if (l.windowStartedAtUs == 0) l.windowStartedAtUs = micros();
  if (now - l.lastReportAtMs < 1000U) return;
  const uint32_t windowUs = micros() - l.windowStartedAtUs;
  const uint32_t busyPermille = windowUs == 0 ? 0 : static_cast<uint32_t>(
    l.busy.sumUs * 1000ULL / windowUs);
  if (loopLoadLineNext < loopLoadLinesPending) ++loopLoadDroppedReports;
  char busy[32], period[32], input[32], audio[32], clock[32], presence[32], pers[32], imu[32],
    other[32], eyeIdle[32], eyeFrame[32], eyePanel[32];
  formatSection(busy, sizeof(busy), l.busy);
  formatSection(period, sizeof(period), l.period);
  formatSection(input, sizeof(input), l.input);
  formatSection(audio, sizeof(audio), l.audio);
  formatSection(clock, sizeof(clock), l.showClock);
  formatSection(presence, sizeof(presence), l.presence);
  formatSection(pers, sizeof(pers), l.personality);
  formatSection(imu, sizeof(imu), l.imu);
  formatSection(other, sizeof(other), l.other);
  formatSection(eyeIdle, sizeof(eyeIdle), l.eyeIdle);
  formatSection(eyeFrame, sizeof(eyeFrame), l.eyeFrame);
  formatSection(eyePanel, sizeof(eyePanel), l.eyePanelFrame);
  const unsigned long t = static_cast<unsigned long>(now);
  // Sections: count/avg_us/max_us.
  snprintf(loopLoadLines[0], sizeof(loopLoadLines[0]),
    "LOOP_A t=%lu win_us=%lu busy_pm=%lu busy=%s period=%s show=%u audio_on=%u "
    "sleeping=%u panel=%u view=%u frame_ms=%u panel_ms=%u dropped_reports=%lu\n",
    t, static_cast<unsigned long>(windowUs), static_cast<unsigned long>(busyPermille), busy,
    period, showRunning ? 1U : 0U, audioAnalysisStarted ? 1U : 0U,
    personality.sleeping() ? 1U : 0U, messagePanel.visible(now) ? 1U : 0U,
    static_cast<unsigned>(debugView.view()),
    static_cast<unsigned>(eyeRenderer.frameIntervalMs()),
    static_cast<unsigned>(eyeRenderer.panelFrameIntervalMs()),
    static_cast<unsigned long>(loopLoadDroppedReports));
  snprintf(loopLoadLines[1], sizeof(loopLoadLines[1]),
    "LOOP_B t=%lu input=%s audio=%s clock=%s presence=%s personality=%s imu=%s other=%s\n",
    t, input, audio, clock, presence, pers, imu, other);
  snprintf(loopLoadLines[2], sizeof(loopLoadLines[2]),
    "LOOP_C t=%lu eye_idle=%s eye_frame=%s eye_panel=%s audio_blocks=%lu audio_gap_ms=%lu "
    "imu_samples=%lu imu_gap_ms=%lu audio_drops=%lu backlog_max=%u\n",
    t, eyeIdle, eyeFrame, eyePanel,
    static_cast<unsigned long>(l.audioBlocks), static_cast<unsigned long>(l.maxAudioBlockGapMs),
    static_cast<unsigned long>(l.imuSamples), static_cast<unsigned long>(l.maxImuGapMs),
    static_cast<unsigned long>(audioAnalysisStarted ? voiceBaseInput.analysisDrops() : 0),
    static_cast<unsigned>(audioAnalysisStarted ? voiceBaseInput.analysisBacklogHighWater() : 0));
  snprintf(loopLoadLines[3], sizeof(loopLoadLines[3]),
    "LOOP_D t=%lu psram_free=%u psram_min=%u int_free=%u int_min=%u\n", t,
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
    static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)),
    static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
    static_cast<unsigned>(heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL)));
  snprintf(loopLoadLines[4], sizeof(loopLoadLines[4]),
    "LOOP_E t=%lu tx=%lu tx_rejected=%lu beats=%lu beat_late_max_ms=%lu\n", t,
    static_cast<unsigned long>(l.txAttempts), static_cast<unsigned long>(l.txRejected),
    static_cast<unsigned long>(l.beats), static_cast<unsigned long>(l.maxBeatLateMs));
#if DECAFLASH_PANEL_FRAME_TIMING
  {
    // Panel frame sections: avg_us/max_us; motion: count/avg_us/max_us.
    char motion[32];
    formatSection(motion, sizeof(motion), l.motion);
    const auto am = [](const LoopSection& s) {
      return static_cast<unsigned long>(s.averageUs());
    };
    const auto mx = [](const LoopSection& s) { return static_cast<unsigned long>(s.maxUs); };
    snprintf(loopLoadLines[5], sizeof(loopLoadLines[5]),
      "LOOP_G t=%lu pf=%lu first=%lu decodes=%lu eye=%lu/%lu shade=%lu/%lu wrap=%lu/%lu "
      "text=%lu/%lu emoji=%lu/%lu push=%lu/%lu motion=%s ev_dropped=%lu demo=%u atlas_us=%lu\n", t,
      static_cast<unsigned long>(l.panelEye.count), static_cast<unsigned long>(l.panelFirstFrames),
      static_cast<unsigned long>(l.panelEmojiDecodes), am(l.panelEye), mx(l.panelEye),
      am(l.panelShade), mx(l.panelShade), am(l.panelWrap), mx(l.panelWrap), am(l.panelText),
      mx(l.panelText), am(l.panelEmoji), mx(l.panelEmoji), am(l.panelPush), mx(l.panelPush),
      motion, static_cast<unsigned long>(timingEventsDropped),
      static_cast<unsigned>(timingDemoPhase),
      static_cast<unsigned long>(eyeRenderer.benchmark().panelAtlasDecodeUs));
  }
#endif
#if DECAFLASH_SLM
  {
    const auto slm = decaflash::mainframe::slmTakeReport();
    snprintf(loopLoadLines[5], sizeof(loopLoadLines[5]),
      "LOOP_F t=%lu slm=\"%s\" fwd=%lu/%lu/%lu pieces=%lu runs=%lu mismatches=%lu "
      "stack_free=%lu\n", t, slm.state,
      static_cast<unsigned long>(slm.forwards), static_cast<unsigned long>(slm.forwardAvgUs),
      static_cast<unsigned long>(slm.forwardMaxUs), static_cast<unsigned long>(slm.pieces),
      static_cast<unsigned long>(slm.runs), static_cast<unsigned long>(slm.mismatches),
      static_cast<unsigned long>(slm.stackFreeBytes));
  }
#endif
  loopLoadLinesPending = kLoopLoadLines;
  loopLoadLineNext = 0;
  const uint32_t lastPass = l.lastPassStartUs;
  const uint32_t lastAudio = l.lastAudioBlockAtMs;
  const uint32_t processed = l.lastProcessedFrames;
  const uint32_t lastImu = l.lastImuAtMs;
  l = LoopLoad{};
  l.lastPassStartUs = lastPass;
  l.lastAudioBlockAtMs = lastAudio;
  l.lastProcessedFrames = processed;
  l.lastImuAtMs = lastImu;
  l.lastReportAtMs = now;
  l.windowStartedAtUs = micros();
}
#define LOOP_LOAD_MARK(section) loopLoad.section.add(elapsedUs(loopLoadMarkUs))
#define LOOP_PASS_MARK(call) loopPassUs[call] += elapsedUs(loopPassMarkUs)
#else
#define LOOP_LOAD_MARK(section) do {} while (0)
#define LOOP_PASS_MARK(call) do {} while (0)
#endif

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
  input.creatureMode = !audioAnalysisRequested;
  if (!audioAnalysisStarted) return input;
  input.fresh = voiceBaseInput.fresh(now);
  const auto& features = voiceBaseInput.moodFeatures();
  input.silent = features.silent();
  input.bassValid = features.bassValid();
  input.bassPermille = features.bassPermille();
  const auto& tempo = voiceBaseInput.v2Tempo();
  input.bpm = tempo.bpm;
  input.confidence = tempo.confidence;
  input.onsetAtMs = tempo.lastOnsetAtMs;
  return input;
}

void serviceDepressionPoetry(uint32_t now, const decaflash::mainframe::Mood& mood) {
  const bool eligible = mood.depression > 50;
  if (!eligible) {
    depressionPoemDueAtMs = 0;
    return;
  }
  if (depressionPoemDueAtMs == 0) {
    depressionPoemDueAtMs = now + kDepressionPoemIntervalMs;
    return;
  }
  if (static_cast<int32_t>(now - depressionPoemDueAtMs) < 0 || messagePanel.visible(now)) return;
  if (messagePanel.showDepressionPoem(now, depressionPoemSequence++)) {
    depressionPoemDueAtMs = now + kDepressionPoemIntervalMs;
  }
}

#if DECAFLASH_PANEL_RATE_SWEEP
// Measurement only: 80 s after the show and audio analysis have started, the
// build cycles through the steps below, each 15 s, with 3 s at the normal
// intervals and without demo text in between. Text steps keep a fixed demo
// text on the panel. The loop-load report names both intervals, so every
// one-second window can be assigned to its step.
struct PanelRateSweepStep {
  uint16_t frameMs;
  uint16_t panelFrameMs;
  bool text;
};
constexpr PanelRateSweepStep kPanelRateSweepSteps[] = {
  {25, 25, true}, {25, 40, true}, {25, 50, true}, {25, 67, true},
  {25, 25, false}, {40, 25, false}};
constexpr uint8_t kPanelRateSweepStepCount =
  sizeof(kPanelRateSweepSteps) / sizeof(kPanelRateSweepSteps[0]);
constexpr uint32_t kPanelRateSweepDelayMs = 80000;
constexpr uint32_t kPanelRateSweepStepMs = 15000;
constexpr uint32_t kPanelRateSweepGapMs = 3000;
const char* const kPanelRateSweepText = u8"MESSUNG LÄUFT. 👁";
uint32_t panelRateSweepShowAtMs = 0;
uint32_t panelRateSweepPhaseAtMs = 0;
uint8_t panelRateSweepStep = 0;
bool panelRateSweepStarted = false;
bool panelRateSweepInStep = false;

void servicePanelRateSweep(uint32_t now) {
  if (!showRunning || !audioAnalysisStarted) return;
  if (panelRateSweepShowAtMs == 0) panelRateSweepShowAtMs = now;
  if (!panelRateSweepStarted) {
    if (now - panelRateSweepShowAtMs < kPanelRateSweepDelayMs) return;
    panelRateSweepStarted = true;
    panelRateSweepPhaseAtMs = now - kPanelRateSweepGapMs;
  }
  const uint32_t elapsed = now - panelRateSweepPhaseAtMs;
  const auto& step = kPanelRateSweepSteps[panelRateSweepStep];
  if (!panelRateSweepInStep) {
    if (elapsed < kPanelRateSweepGapMs) return;
    eyeRenderer.setFrameIntervals(step.frameMs, step.panelFrameMs);
    if (step.text) messagePanel.show(now, kPanelRateSweepText);
    panelRateSweepInStep = true;
    panelRateSweepPhaseAtMs = now;
    return;
  }
  if (step.text) {
    // Other texts may replace the demo text; it returns once they close.
    if (messagePanel.visible(now)) return;
    if (elapsed < kPanelRateSweepStepMs) {
      messagePanel.show(now, kPanelRateSweepText);
      return;
    }
  } else if (elapsed < kPanelRateSweepStepMs) {
    return;
  }
  eyeRenderer.setFrameIntervals(decaflash::mainframe::kFrameIntervalMs,
                                decaflash::mainframe::kPanelFrameIntervalMs);
  panelRateSweepStep = static_cast<uint8_t>((panelRateSweepStep + 1) % kPanelRateSweepStepCount);
  panelRateSweepInStep = false;
  panelRateSweepPhaseAtMs = now;
}
#endif

uint32_t presenceErrors() {
  return presenceSensor.readErrors() + presenceDroppedLines;
}

// Always hand the line to Serial: HWCDC only re-detects a reading host and
// drains its buffer inside write(), so skipping writes while the buffer is
// full keeps it full for good (seen on the device: one TMOS line, then none).
// The short TX timeout set in setup() bounds the wait; whatever cannot be
// queued in time counts as dropped.
void writePresenceLine(const char* line, size_t length) {
  if (length == 0) return;
  if (Serial.write(reinterpret_cast<const uint8_t*>(line), length) < length) {
    ++presenceDroppedLines;
  }
}

void servicePresence(uint32_t now) {
  presenceSensor.service(now);
  char line[200];
  bool sensorOk = false;
  if (presenceSensor.takeStateChange(sensorOk)) {
    if (!sensorOk) presenceDecision.sensorMissing(now);
    presenceDebug.setState(presenceSensor.state(), presenceErrors());
    writePresenceLine(line, decaflash::mainframe::formatPresenceStateLine(
      line, sizeof(line), sensorOk, presenceErrors()));
  }
  decaflash::mainframe::PresenceConfig config;
  if (presenceSensor.takeConfig(config)) {
    presenceDecision.sensorConfigured(now);
    presenceDebug.setConfig(config);
    writePresenceLine(line, decaflash::mainframe::formatPresenceConfigLine(
      line, sizeof(line), config));
  }
  presenceDebug.setState(presenceSensor.state(), presenceErrors());
  decaflash::mainframe::PresenceSample sample;
  if (presenceSensor.takeSample(sample)) {
    presenceDebug.add(sample);
    decaflash::mainframe::PresenceSampleInput input;
    input.valid = sample.valid;
    input.presence = sample.presence;
    input.motionFlag = sample.motionFlag;
    input.atMs = sample.atMs;
    const auto event = presenceDecision.feed(input);
    if (sample.valid) {
      writePresenceLine(line, decaflash::mainframe::formatPresenceSampleLine(
        line, sizeof(line), sample, presenceErrors(), presenceDecision.state()));
    }
    if (event.kind != decaflash::mainframe::PresenceEventKind::None) {
      writePresenceLine(line, decaflash::mainframe::formatPresenceEventLine(
        line, sizeof(line), event));
      personality.update(now, moodAudio(now), presenceDecision.mood());
      personality.onPresence(event);
      messagePanel.showPresence(now, event);
    }
  }
  presenceDebug.setPerson(presenceDecision.state(), presenceDecision.stateChangedAtMs(),
                          presenceDecision.haveState(),
                          presenceDecision.config().enterThreshold,
                          presenceDecision.config().nearEnterThreshold);
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
#if DECAFLASH_LOOP_LOAD
  ++loopLoad.txAttempts;
#endif
  if (!radioReady) {
#if DECAFLASH_LOOP_LOAD
    ++loopLoad.txRejected;
#endif
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
#if DECAFLASH_LOOP_LOAD
  if (!accepted) ++loopLoad.txRejected;
#endif
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
  v2TempoAdoption.reset();
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

void applyV2Tempo(uint32_t now) {
  if (!showRunning || !voiceBaseInput.fresh(now)) return;
  const auto& tempo = voiceBaseInput.v2Tempo();
  const uint16_t acceptedBpm = v2TempoAdoption.observe(
    tempo.tempoEvaluations, currentBpm, tempo.bpm, tempo.confidence,
    tempo.lastOnsetAtMs, now);
  if (acceptedBpm == 0U) return;

  // Two 250-ms V2 evaluations are enough to reject a one-window outlier.
  // This is deliberately not a song-change lock: there is no long holdover,
  // silence state or half/double-tempo rewrite.
  currentBpm = acceptedBpm;
  beatInBar = 1;
  ++currentBar;
  nextBeatAtMs = tempo.lastOnsetAtMs + beatIntervalMs();
  while (static_cast<int32_t>(now - nextBeatAtMs) >= 0) nextBeatAtMs += beatIntervalMs();
  sendClockSync();
  triggerBeatDot(now, true);
}

void serviceShowClock(uint32_t now) {
  if (!showRunning) return;
#if DECAFLASH_LOOP_LOAD
  // Lateness of the first due beat against its planned time, before catching up.
  if (static_cast<int32_t>(now - nextBeatAtMs) >= 0) {
    ++loopLoad.beats;
    if (now - nextBeatAtMs > loopLoad.maxBeatLateMs) loopLoad.maxBeatLateMs = now - nextBeatAtMs;
  }
#endif
  bool beatFired = false;
  bool onBarStart = false;
  while (decaflash::mainframe::takeDueShowBeat(now, nextBeatAtMs, beatIntervalMs())) {
    onBarStart = decaflash::mainframe::advanceShowBeat(beatInBar, currentBar, kBeatsPerBar);
    beatFired = true;
  }
  if (beatFired) {
    // A Node renders the ClockSync position on arrival, so after catching up
    // missed beats only the beat that starts now may be sent.
    if (onBarStart) sendClockSync();
    triggerBeatDot(now, false);
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

#if DECAFLASH_SLM_AUTO_RUN
// Measurement only (docs/slm-prototype.md, task 6.4): runs the three states of
// Decision 6 without hands. Idle until kAutoRunIdleMs after boot; then the
// show starts as on a button press (a press before that starts it earlier);
// kAutoRunShowMs of show without scripted texts after the audio analysis
// starts; then a new text every kAutoRunTextStepMs for kAutoRunTextMs, so a
// text is visible throughout. Music is the user's part. AUTO_RUN lines name
// each phase change.
constexpr uint32_t kAutoRunIdleMs = 11UL * 60UL * 1000UL;
constexpr uint32_t kAutoRunShowMs = 11UL * 60UL * 1000UL;
constexpr uint32_t kAutoRunTextMs = 11UL * 60UL * 1000UL;
constexpr uint32_t kAutoRunTextStepMs = 3000;
constexpr const char* kAutoRunTexts[] = {
  u8"KLOPF 💬", u8"WER IST DA? 🙂", u8"NICHT SO WILD. 😤", u8"AU. SANFTER! 😡",
  u8"AU. AU. 🥲", u8"ICH SEH NIX. 🙈", u8"DU BIST DAS! 👉👈", u8"AAH - ICH KOTZE! 🤢",
  u8"SCHON WEG? 😢", u8"HALLO 🌚 DU 🤗"};
constexpr uint8_t kAutoRunTextCount = sizeof(kAutoRunTexts) / sizeof(kAutoRunTexts[0]);
uint8_t autoRunPhase = 0;  // 0 idle, 1 show, 2 text, 3 done
uint32_t autoRunAudioAtMs = 0;
uint32_t autoRunNextTextAtMs = 0;
uint8_t autoRunText = 0;

void autoRunReport(uint32_t now, const char* phase) {
  Serial.printf("AUTO_RUN t=%lu phase=%s\n", static_cast<unsigned long>(now), phase);
}

void serviceAutoRun(uint32_t now) {
  if (autoRunPhase == 3) return;
  if (autoRunPhase == 0) {
    if (!showRunning) {
      if (now < kAutoRunIdleMs) return;
      startShow();
      audioAnalysisRequested = true;
      autoRunReport(now, "show start=auto");
    } else {
      autoRunReport(now, "show start=button");
    }
    autoRunPhase = 1;
  }
  if (!audioAnalysisStarted) return;
  if (autoRunAudioAtMs == 0) autoRunAudioAtMs = now;
  const uint32_t elapsed = now - autoRunAudioAtMs;
  if (elapsed < kAutoRunShowMs) return;
  if (elapsed >= kAutoRunShowMs + kAutoRunTextMs) {
    autoRunPhase = 3;
    autoRunReport(now, "done");
    return;
  }
  if (autoRunPhase == 1) {
    autoRunPhase = 2;
    autoRunNextTextAtMs = now;
    autoRunReport(now, "text");
  }
  if (static_cast<int32_t>(now - autoRunNextTextAtMs) < 0) return;
  messagePanel.show(now, kAutoRunTexts[autoRunText]);
  autoRunText = static_cast<uint8_t>((autoRunText + 1U) % kAutoRunTextCount);
  autoRunNextTextAtMs += kAutoRunTextStepMs;
}
#endif

}  // namespace

void setup() {
  // USB CDC is enabled by the build flags, but it still needs an explicit
  // Arduino serial start before diagnostics and benchmark output are usable.
  // Do not wait for a host here: the installation must boot headlessly.
  Serial.begin(115200);
  // Bound every serial write so a host that stops reading never stalls the
  // loop for long (HWCDC default: 100 ms). Value in add-presence-sensor-debug design.md.
  Serial.setTxTimeoutMs(kSerialTxTimeoutMs);
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

#if DECAFLASH_MOTION_TRACE
  reportImuConfig();
#endif
  checkPsramSample();
  presenceSensor.begin(millis());
  eyeRenderer.begin();
  const bool previewReady = startupSoundPreview.begin();
  initialiseRadio();
#if DECAFLASH_SLM
  decaflash::mainframe::slmBegin();
#endif
  Serial.printf("Sound preview: %s; audio analysis waits for first button press\n",
                previewReady ? "ready" : "failed");
}

void loop() {
#if DECAFLASH_LOOP_LOAD
  const uint32_t loopStartUs = micros();
  uint32_t loopLoadMarkUs = loopStartUs;
  uint32_t loopPassMarkUs = loopStartUs;
  memset(loopPassUs, 0, sizeof(loopPassUs));
  loopPassSound = -1;
  loopPassSoundPlayed = false;
  if (loopLoad.lastPassStartUs != 0) loopLoad.period.add(loopStartUs - loopLoad.lastPassStartUs);
  loopLoad.lastPassStartUs = loopStartUs;
#endif
  M5.update();
  if (M5.BtnA.wasHold()) {
    debugView.onHold();
  }
  // Inside debug mode a short press only switches the debug screen.
  if (M5.BtnA.wasClicked() && debugView.onClick()) {
    if (showRunning) {
      selectNextScene();
    } else {
      startShow();
      audioAnalysisRequested = true;
    }
  }
  const uint32_t now = millis();
  const bool previewFinished = startupSoundPreview.service(now);
  // Creature sounds exist only before the analysis is requested.
#if DECAFLASH_LOOP_LOAD
  loopPassMarkUs = micros();
#endif
  if (!audioAnalysisRequested) startupSoundPreview.prepareSounds();
  LOOP_PASS_MARK(kPassPrepare);
  if (audioAnalysisRequested && previewFinished && !audioAnalysisStarted) {
    audioAnalysisStarted = voiceBaseInput.begin();
    Serial.printf("Audio analysis: %s\n", audioAnalysisStarted ? "started" : "failed");
  }
  LOOP_LOAD_MARK(input);
  if (audioAnalysisStarted) {
#if DECAFLASH_EYE_BENCHMARK
    observeBenchmarkCadence(now, eyeBenchmarkLoad.lastAudioAtMs,
                            eyeBenchmarkLoad.maxAudioGapMs, eyeBenchmarkLoad.audioCalls);
#endif
    voiceBaseInput.update();
  }
  if (audioAnalysisStarted) applyV2Tempo(now);
#if DECAFLASH_LOOP_LOAD
  if (audioAnalysisStarted) {
    const uint32_t processed = voiceBaseInput.v2TempoMetrics().processedFrames;
    if (processed != loopLoad.lastProcessedFrames) {
      loopLoad.audioBlocks += processed - loopLoad.lastProcessedFrames;
      loopLoad.lastProcessedFrames = processed;
#if DECAFLASH_PANEL_FRAME_TIMING
      queueGapEvent(now, "audio", loopLoad.lastAudioBlockAtMs, kReportedAudioGapMs);
#endif
      observeGap(now, loopLoad.lastAudioBlockAtMs, loopLoad.maxAudioBlockGapMs);
    }
  }
#endif
  LOOP_LOAD_MARK(audio);
#if DECAFLASH_BEAT_TRACKER_TELEMETRY
  reportTempoTelemetry(now);
#endif
  serviceShowClock(now);
  LOOP_LOAD_MARK(showClock);
  servicePresence(now);
  LOOP_LOAD_MARK(presence);
  if (now - lastMoodAtMs >= 100) {
#if DECAFLASH_EYE_BENCHMARK
    observeBenchmarkCadence(now, eyeBenchmarkLoad.lastMoodAtMs,
                            eyeBenchmarkLoad.maxMoodGapMs, eyeBenchmarkLoad.moodUpdates);
#endif
    lastMoodAtMs = now;
    personality.update(now, moodAudio(now), presenceDecision.mood());
    LOOP_LOAD_MARK(personality);
  }
#if DECAFLASH_LOOP_LOAD
  loopLoadMarkUs = micros();
#endif
  // Every new sensor sample goes to the classifier: the peak of a knock on
  // the enclosure lasts 6-12 ms and was missed at the earlier 20 ms cadence.
  // The BMI270 delivers accelerometer data at 100 Hz.
  if (M5.Imu.isEnabled()) {
    const auto updated = M5.Imu.update();
    const auto required = m5::IMU_Class::sensor_mask_accel | m5::IMU_Class::sensor_mask_gyro;
    if ((updated & required) == required) {
#if DECAFLASH_EYE_BENCHMARK
      observeBenchmarkCadence(now, eyeBenchmarkLoad.lastSensorAtMs,
                              eyeBenchmarkLoad.maxSensorGapMs, eyeBenchmarkLoad.sensorUpdates);
#endif
#if DECAFLASH_LOOP_LOAD
      ++loopLoad.imuSamples;
#if DECAFLASH_PANEL_FRAME_TIMING
      queueGapEvent(now, "imu", loopLoad.lastImuAtMs, kReportedImuGapMs);
#endif
      observeGap(now, loopLoad.lastImuAtMs, loopLoad.maxImuGapMs);
#endif
      const auto data = M5.Imu.getImuData();
      decaflash::mainframe::MotionSample sample = {
        data.accel.x, data.accel.y, data.accel.z,
        data.gyro.x, data.gyro.y, data.gyro.z};
      const auto event = motionEvents.feed(now, sample);
#if DECAFLASH_MOTION_TRACE
      traceMotionSample(now, sample);
      if (event.kind != decaflash::mainframe::MotionKind::None &&
          Serial.availableForWrite() >= 96) {
        Serial.printf("MOTION_CLS t=%lu kind=%s count=%u peak_mg=%u\n",
                      static_cast<unsigned long>(now),
                      decaflash::mainframe::motionName(event.kind), event.count,
                      static_cast<unsigned>(event.peakG * 1000.0f));
      }
#endif
      if (event.kind != decaflash::mainframe::MotionKind::None) {
#if DECAFLASH_PANEL_FRAME_TIMING
        const uint32_t motionStartedAtUs = micros();
#endif
        personality.update(now, moodAudio(now), presenceDecision.mood());
        personality.onMotion(event);
        messagePanel.showMotion(now, event);
#if DECAFLASH_PANEL_FRAME_TIMING
        passMotionUs = micros() - motionStartedAtUs;
        loopLoad.motion.add(passMotionUs);
#endif
      }
    }
  }
#if DECAFLASH_MOTION_TRACE
  serviceMotionTraceOutput();
#endif
  LOOP_LOAD_MARK(imu);
#if DECAFLASH_LOOP_LOAD
  loopPassMarkUs = loopLoadMarkUs;
#endif
  const auto mood = personality.snapshot();
  serviceDepressionPoetry(now, mood);
  LOOP_PASS_MARK(kPassPoetry);
#if DECAFLASH_PANEL_RATE_SWEEP
  servicePanelRateSweep(now);
#endif
#if DECAFLASH_PANEL_FRAME_TIMING
  serviceTimingDemo(now);
#endif
#if DECAFLASH_SLM_AUTO_RUN
  serviceAutoRun(now);
#endif
  decaflash::mainframe::ThresholdCrossingEvent soundEvent;
  if (soundWatcher.update(mood, now,
                          !audioAnalysisRequested && !personality.sleeping() &&
                          startupSoundPreview.availableForMoodSound(),
                          soundEvent)) {
    LOOP_PASS_MARK(kPassWatcher);
#if DECAFLASH_LOOP_LOAD
    loopPassSound = static_cast<int8_t>(static_cast<uint8_t>(soundEvent.state) * 2U +
                                        static_cast<uint8_t>(soundEvent.direction));
    loopPassSoundPlayed = startupSoundPreview.playMoodSound(soundEvent);
#else
    startupSoundPreview.playMoodSound(soundEvent);
#endif
    LOOP_PASS_MARK(kPassMoodSound);
  }
  if (!audioAnalysisRequested && personality.sleeping() &&
      startupSoundPreview.availableForMoodSound() && personality.consumeSnore(now)) {
    LOOP_PASS_MARK(kPassWatcher);
#if DECAFLASH_LOOP_LOAD
    loopPassSound = 10;
    loopPassSoundPlayed = startupSoundPreview.playSleepSnore();
#else
    startupSoundPreview.playSleepSnore();
#endif
    LOOP_PASS_MARK(kPassSnore);
  }
  LOOP_PASS_MARK(kPassWatcher);
  serviceVisualState(now, mood);
  LOOP_PASS_MARK(kPassVisual);
  const bool beatDotVisible = showRunning &&
    static_cast<int32_t>(now - beatDotUntilMs) < 0;
  const uint8_t beatPulse = beatDotVisible
    ? static_cast<uint8_t>((beatDotUntilMs - now) * 255UL / kBeatDotFlashMs)
    : 0;
  const auto& v2Estimate = voiceBaseInput.v2Tempo();
  decaflash::mainframe::TempoDebugInfo tempoDebug;
  tempoDebug.showBpm = currentBpm;
  tempoDebug.candidateBpm = v2Estimate.bpm;
  tempoDebug.confidence = v2Estimate.confidence;
  const bool moodScreen = debugView.view() == decaflash::mainframe::DebugView::Mood;
  const bool presenceScreen = debugView.view() == decaflash::mainframe::DebugView::Presence;
#if DECAFLASH_LOOP_LOAD
  // Same condition under which the renderer draws the message panel.
  const bool panelFrame = decaflash::mainframe::drawsMessagePanel(
    moodScreen || presenceScreen, personality.sleeping(), messagePanel.visible(now));
  const uint32_t framesBefore = eyeRenderer.benchmark().outputFrames;
#endif
#if DECAFLASH_PANEL_FRAME_TIMING
  const uint32_t panelTimingFramesBefore = eyeRenderer.panelTimingFrames();
#endif
  LOOP_PASS_MARK(kPassEyeArgs);
  LOOP_LOAD_MARK(other);
  eyeRenderer.service(now, currentBpm, beatInBar, beatDotVisible, beatDotIsSync,
                      audioAnalysisStarted ? voiceBaseInput.vuLevel(millis()) : 0, beatPulse, mood.attention,
                      mood.annoyance, mood.loneliness, startupSoundPreview.bootProgress(now),
                      personality.sleeping(),
                      moodScreen ? &mood : nullptr,
                      moodScreen ? &motionEvents.latest() : nullptr,
                      moodScreen ? &tempoDebug : nullptr,
                      &messagePanel,
                      presenceScreen ? &presenceDebug : nullptr);
#if DECAFLASH_LOOP_LOAD
  {
    const uint32_t eyeUs = elapsedUs(loopLoadMarkUs);
    loopPassUs[kPassEye] = eyeUs;
    if (eyeRenderer.benchmark().outputFrames == framesBefore) {
      loopLoad.eyeIdle.add(eyeUs);
    } else if (panelFrame) {
      loopLoad.eyePanelFrame.add(eyeUs);
    } else {
      loopLoad.eyeFrame.add(eyeUs);
    }
  }
#if DECAFLASH_PANEL_FRAME_TIMING
  if (eyeRenderer.panelTimingFrames() != panelTimingFramesBefore) {
    queuePanelFrameEvent(now, micros() - loopStartUs);
  }
  passMotionUs = 0;
#endif
  const uint32_t passBusyUs = micros() - loopStartUs;
  loopLoad.busy.add(passBusyUs);
  reportLoopPass(now, passBusyUs);
  reportLoopLoad(now);
  serviceLoopLoadOutput();
#endif
#if DECAFLASH_EYE_BENCHMARK
  reportEyeBenchmark(now);
#endif
  delay(5);
}
