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
- **Behaviour:** Flash output blocks the main loop with `delay(flashMs)` plus 8 ms pauses. The hello confirmation blocks for about 2.3 seconds with three flashes. Staged radio packets are processed only afterwards, so scene and clock arrive late. Because `applyClockSync()` uses the processing time as `now`, the beat phase shifts by the blocked time.
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

### BUG-007: Mainframe sends the bar's ClockSync one beat late

- **Area:** Mainframe, show clock and clock sync
- **Where:** `apps/mainframe/src/main.cpp`, `serviceShowClock()`; also `startShow()` and `applyV2Tempo()`
- **Behaviour:** On each show beat `serviceShowClock()` checks `beatInBar == 1` and sends ClockSync before it advances `beatInBar`. The beat dot is triggered after the advance and counts correctly (red or yellow on beat 1, white on 2 to 4). The ClockSync for beat 1 therefore goes out on the beat the Mainframe itself shows as beat 2, so Nodes run one beat behind the Mainframe's bar. At show start and after every tempo adoption the Mainframe has already sent ClockSync for beat 1; its next tick sends ClockSync for the same beat 1 and bar again, and a Node renders beat 1, beat 2 from its own clock, then beat 1 again.
- **Verified:** code reading only. Neither the offset nor the double beat 1 has been observed on a Node.
- **Spec:** [protocol](openspec/specs/protocol/spec.md), requirements "Mainframe clock sending" and "Node beat alignment on clock sync"; [eye-iris](openspec/specs/eye-iris/spec.md), beat dot

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

### BUG-XXX: RGB long press in demo also advances the scene

- **Area:** Node, button
- **Where:** `apps/node/src/main.cpp`, `serviceButton()`; `switchNodeRole()` → `selectProgram()` → `resetDemoClockState()` → `clearButtonGesture()`
- **Behaviour:** On an RGB strip Node in `Demo`, a long press cycles the role, and the role switch restarts the demo with scene 0. That restart calls `clearButtonGesture()` while the button is still held, so the next loop pass registers a new press. Its release then counts as a short press and selects scene 1. Held for another long-press time, the button cycles the role again. A single long press thus ends with the next role on scene 1 instead of scene 0. Under Mainframe control the role switch does not restart the demo, so the gesture is not reset there.
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirements "Long press per Node kind" and "Applying a new profile"

### BUG-XXX: Role or kind switch while waiting replaces the idle output

- **Area:** Node, kind and role switching
- **Where:** `apps/node/src/main.cpp`, `switchNodeKind()` and `switchNodeRole()` (`mainframeOwned` branch)
- **Behaviour:** After a hello the Node shows the idle command and waits in `MainframeWaiting` for the first ClockSync. A kind or role switch by button or serial command in this state applies the command of the current scene index for the new kind and role. That index is usually still the one from the demo, so the Node shows a scene the Mainframe has not selected. Intended (user decision, 2026-11-16): a waiting Node keeps the idle output; the new kind or role takes effect with the next scene or clock from the Mainframe.
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirement "Applying a new profile"

### BUG-XXX: Node text overlay lights a muted flashlight

- **Area:** Node, mute and text overlay
- **Where:** `apps/node/src/main.cpp`, `onBeat()` → `startPendingNodeTextOverlay()`, `serviceOutput()`
- **Behaviour:** A long press mutes a flashlight Node. If it is in `MainframeRunning` and then receives a `NodeText`, the next beat starts the overlay regardless of the mute and calls `showTemporaryLit(true)` for the first Morse element. `serviceOutput()` returns early while muted, so the overlay never advances and the flashlight stays lit until something clears the overlay or ends the mute. The V2 Mainframe does not send `NodeText` yet, so this is not reachable in a V2-only installation today.
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirement "Output mute"; [protocol](openspec/specs/protocol/spec.md), requirement "Node text layout and handling"

### BUG-XXX: Node help text omits the demo-only limit for the flashlight short press

- **Area:** Node, serial help
- **Where:** `apps/node/src/main.cpp`, `printHelp()`
- **Behaviour:** For an RGB strip the help says `short press -> next scene (demo only)`; for a flashlight it says `short press -> next scene`. `serviceButton()` handles a short press only in `Demo` for both kinds, so the flashlight line is wrong under Mainframe control.
- **Verified:** code reading only.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirements "Short press" and "Help text"

### BUG-XXX: Filter monitor colour strength wraps to 0 at drift 255

- **Area:** Node, serial filter monitor
- **Where:** `apps/node/src/main.cpp`, `colorDriftStrength()`
- **Behaviour:** The strength is `(colorDrift - 127) * 2` cast to `uint8_t`. For a drift of 255 the result is 256 and wraps to 0, so the monitor and `status` print `COLOR: magenta 0` at the strongest magenta drift. The drift is a renderer mix value that can in principle reach 255.
- **Verified:** code reading only. Not observed in a log.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirement "Filter monitor"

## Closed

### BUG-006: Scene changes not taken over reliably by Nodes

- **Closed:** 2026-11-14, according to the user the problem has not occurred for a long time.
- **Cause:** not proven. No log, measurement or recorded hardware test ties the fix to a specific change. The only change to the scene path in that period is the switch to the single `SceneSelect` message in the old project (`26f4500`, 2026-09-23).
- **Details:** [project-context.md, A.3](docs/project-context.md#a3-scenes-renderers-and-concrete-problem-areas)
