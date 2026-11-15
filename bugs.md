# Bugs

Known defects in the V2 code and its documentation. Open design decisions, planned features and pending hardware acceptance stay in [docs/project-context.md](docs/project-context.md); this file lists only behaviour that is wrong or text that contradicts the code.

Each entry says what was verified and how. A bug seen on hardware needs a measurement on the device before a cause or a fix is named (see [CLAUDE.md](CLAUDE.md)).

## Open

### BUG-001: Node accepts any non-zero clock BPM

- **Area:** Node, clock sync
- **Where:** `apps/node/src/main.cpp`, `applyClockSync()` and `bpmToIntervalMs()`
- **Behaviour:** `applyClockSync()` rejects only `bpm == 0`. Any other value is passed to `bpmToIntervalMs()`, which returns `60000 / bpm` without a range check. A BPM above 60000 gives a beat interval of 0 ms; very low values give beats a minute or more apart. The Mainframe clock is limited to 60–180 BPM and the tempo adoption to 80–180 BPM, so a valid V2 Mainframe is not known to send such values.
- **Verified:** code reading only. No invalid packet has been observed.
- **Source:** [project-context.md, A.5](docs/project-context.md#a5-the-node-clock-is-currently-not-a-soft-sync)

### BUG-002: Blocking Node output delays radio and clock handling

- **Area:** Node, output and radio
- **Where:** `apps/node/src/flashlight_renderer.cpp` (`delay(8)`, `delay(flashMs)`), `apps/node/src/rgb_strip_renderer.cpp` (`delay(8)`, `delay(flashMs)`), hello confirmation in `apps/node/src/main.cpp`
- **Behaviour:** Flash output blocks the main loop with `delay(flashMs)` plus 8 ms pauses. The hello confirmation blocks for about 2.3 seconds with three flashes, and the role confirmation blocks as well. Staged radio packets are processed only afterwards, so scene and clock arrive late. Because `applyClockSync()` uses the processing time as `now`, the beat phase shifts by the blocked time.
- **Verified:** code reading only. The delay has not been measured on a Node.
- **Source:** [project-context.md, A.3 item 7 and A.5](docs/project-context.md#a3-scenes-renderers-and-concrete-problem-areas)

### BUG-003: Personality uses a confidence threshold from the old tempo scale

- **Area:** Mainframe, personality
- **Where:** `apps/mainframe/src/personality.cpp`, the checks `audio.confidence >= 68` and the beat bonus `(audio.confidence - 68)`
- **Behaviour:** The threshold of 68 comes from the `BeatAnalyzer` confidence scale. Since `30ac9a7` the confidence comes from `V2TempoTracker`, which uses a different heuristic scale (host test with synthetic kicks: 55–83). If real music stays below 68, tempo trust and the beat bonus for energy never activate.
- **Verified:** code reading and host test values only. Whether real music reaches 68 is untested on the device; this is hardware check 8 in [project-context.md, section 0](docs/project-context.md#open-hardware-checks-for-the-productive-tempo-path).

### BUG-004: `platformio.ini` comments describe the tempo path before `30ac9a7`

- **Area:** Build configuration, comments
- **Where:** `platformio.ini`, comments above `mainframe-beat-tracker` and `mainframe-spectral-capture`
- **Behaviour:** The comment on `mainframe-beat-tracker` says the production `BeatAnalyzer` stays in charge of the show clock; the comment on `mainframe-spectral-capture` calls the spectral values the input of the replacement V2. Since `30ac9a7` `V2TempoTracker` drives the show clock and does not use the spectral values.
- **Verified:** compared with `apps/mainframe/src/main.cpp` and `v2_tempo_tracker.cpp`.

### BUG-005: Fixture README calls the discarded `BpmTracker` the V2 core

- **Area:** Tests, documentation
- **Where:** `tests/fixtures/bpm-bands/README.md`
- **Behaviour:** The README says the report compares the fixtures with the "V2-Kern" (V2 core). The report replays them through `BpmTracker`, which is not productive. The productive `V2TempoTracker` cannot be evaluated with these fixtures, because they contain no PCM and no onset strength.
- **Verified:** compared with `tests/run_bpm_band_trace_report.sh` and the analysis in [project-context.md, section 0](docs/project-context.md#fixtures-and-host-tests).

### BUG-008: Message panel truncation can cut a UTF-8 sequence

- **Area:** Mainframe, message panel
- **Where:** `apps/mainframe/src/message_panel.cpp`, `MessagePanel::show()`
- **Behaviour:** `show()` copies the text with `strncpy` into a 193-byte buffer and cuts it after 192 bytes, without checking for a UTF-8 boundary. A longer text can end in an incomplete multi-byte sequence, which the panel then draws as one or more `?`. All current texts and poems are far shorter than 192 bytes, so it does not happen today.
- **Verified:** code reading only.
- **Spec:** [message-panel](openspec/specs/message-panel/spec.md), Requirement "Single product text surface".

### BUG-009: Poem order jumps after 256 poems

- **Area:** Mainframe, message panel
- **Where:** `apps/mainframe/src/main.cpp`, `depressionPoemSequence` (`uint8_t`) in `serviceDepressionPoetry()`; `MessagePanel::showDepressionPoem()`
- **Behaviour:** The poem is chosen as the 8-bit counter modulo the 11 poems. When the counter wraps from 255 to 0, the order jumps from poem 3 to poem 1 instead of continuing with poem 4, because 256 is not a multiple of 11. At one poem every two minutes this needs more than eight hours of continuously high depression.
- **Verified:** code reading only.
- **Spec:** [message-panel](openspec/specs/message-panel/spec.md), Requirement "Poem selection".

### BUG-010: Message panel can appear over the boot animation

- **Area:** Mainframe, message panel and eye
- **Where:** `apps/mainframe/src/eye_renderer.cpp`, `EyeRenderer::service()`; `apps/mainframe/src/main.cpp`, `loop()`
- **Behaviour:** The main loop reads the IMU and passes motion events to the panel while the boot animation runs. `service()` draws the panel whenever it is visible and the personality is not sleeping, without checking the boot progress, so a text triggered by moving the device during boot is drawn over the boot animation. According to the user (2026-11-15) the panel must not appear there. Poems cannot occur during boot, because depression starts at 0.
- **Expected:** no panel during the boot animation. A text triggered during boot is dropped without starting its cooldown, as in the proposal "Message-Panel: nichts ungesehen verbrauchen" in [ideas.md](ideas.md).
- **Verified:** code reading only. Not observed on the device.
- **Spec:** [message-panel](openspec/specs/message-panel/spec.md), Requirement "Drawing over the eye".

### BUG-011: A mood in cooldown silences other crossings in the same pass

- **Area:** Mainframe, sound
- **Where:** `apps/mainframe/src/sound_personality.cpp`, `MoodThresholdWatcher::update()`
- **Behaviour:** In one pass the watcher selects only the crossing of the first mood in priority order (annoyance, depression, attention, loneliness, energy) and disarms the thresholds of every other mood that crossed. If the selected mood is still in its 10 s cooldown, no phrase plays, and the other crossings of that pass are lost as well. This contradicts the host test `tests/sound_personality.cpp`, which states that a mood's cooldown does not block a different mood; that test only covers crossings in separate passes.
- **Verified:** code reading only. Not observed on the device.
- **Source:** [openspec/specs/sound/spec.md](openspec/specs/sound/spec.md), requirement "One crossing per pass"

### BUG-012: Failed microphone start is retried every loop pass

- **Area:** Mainframe, audio analysis start
- **Where:** `apps/mainframe/src/main.cpp`, `loop()` (`voiceBaseInput.begin()`), `apps/mainframe/src/voice_base_input.cpp`, `VoiceBaseInput::begin()`
- **Behaviour:** After the first button press, `loop()` calls `voiceBaseInput.begin()` as long as `audioAnalysisStarted` is false. If the start fails, it is repeated on every loop pass (about every 5 ms), with no back-off, and each attempt prints "Audio analysis: failed" and allocates and frees the PSRAM analysis ring again. [project-context.md, section 0](docs/project-context.md#creature-sound-audition-before-the-analysis-2026-09-28) says that VoiceBaseInput "then starts exactly once", which holds only when the first attempt succeeds.
- **Verified:** code reading only. A failed microphone start has not been observed.

### BUG-013: Texts describe the pre-analysis sound behaviour wrongly

- **Area:** Mainframe, sound, comments and documentation
- **Where:** `apps/mainframe/src/sound_personality.h`, comment above `sound_config`; [project-context.md, section 0](docs/project-context.md#creature-sound-audition-before-the-analysis-2026-09-28), third paragraph
- **Behaviour:** The header comment calls the crossing policy one "for a later pre-analysis preview", but the policy is active: `main.cpp` plays mood phrases from it until the first button press. project-context.md says that with the first click "the PCM output ends before the microphone starts". The code does not end a playing phrase or snore; the microphone waits until the sound has run to its natural end.
- **Verified:** compared with `apps/mainframe/src/main.cpp` and `apps/mainframe/src/sound_engine.cpp`.

## Closed

### BUG-007: Mainframe sent the bar's ClockSync one beat late

- **Closed:** 2026-11-15, fixed in code.
- **Cause:** `serviceShowClock()` in `apps/mainframe/src/main.cpp` checked `beatInBar == 1` and sent ClockSync before it advanced `beatInBar`. The ClockSync for beat 1 therefore went out on the beat the Mainframe itself showed as beat 2, and after show start or a tempo adoption the next tick sent beat 1 of the same bar a second time.
- **Fix:** the beat advance moved into `advanceShowBeat()` in `apps/mainframe/src/show_clock.h`, which advances first and reports whether the new beat is beat 1. `serviceShowClock()` sends ClockSync only then. Covered by the host test `tests/show_clock.cpp`.
- **Verified:** host test and `pio run -e mainframe`. Not verified on hardware: no Node has been observed with the old or the new firmware.
- **Spec:** [protocol](openspec/specs/protocol/spec.md), requirements "Mainframe clock sending" and "Node beat alignment on clock sync"

### BUG-006: Scene changes not taken over reliably by Nodes

- **Closed:** 2026-11-14, according to the user the problem has not occurred for a long time.
- **Cause:** not proven. No log, measurement or recorded hardware test ties the fix to a specific change. The only change to the scene path in that period is the switch to the single `SceneSelect` message in the old project (`26f4500`, 2026-09-23).
- **Details:** [project-context.md, A.3](docs/project-context.md#a3-scenes-renderers-and-concrete-problem-areas)
