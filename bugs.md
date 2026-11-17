# Bugs

Known defects in the V2 code and its documentation. Open design decisions, planned features and pending hardware acceptance stay in [docs/project-context.md](docs/project-context.md); this file lists only behaviour that is wrong or text that contradicts the code.

Each entry says what was verified and how. A bug seen on hardware needs a measurement on the device before a cause or a fix is named (see [CLAUDE.md](CLAUDE.md)).

## Open

### BUG-001: Node accepts any non-zero clock BPM

- **Area:** Node, clock sync
- **Where:** `apps/node/src/main.cpp`, `applyClockSync()` and `bpmToIntervalMs()`
- **Behaviour:** `applyClockSync()` rejects only `bpm == 0`. Any other value is passed to `bpmToIntervalMs()`, which returns `60000 / bpm` without a range check. A BPM above 60000 gives a beat interval of 0 ms; very low values give beats a minute or more apart. The Mainframe clock starts at 120 BPM and changes only through the tempo adoption, which accepts 80–180 BPM, so a valid V2 Mainframe is not known to send such values.
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

### BUG-014: Node stops following the Mainframe until the Node is reset

- **Area:** Node, radio receive
- **Where:** not located. Receive path in `apps/node/src/main.cpp` (`onEspNowReceive()`, `processPendingRadio()`), radio setup in `shared/include/espnow_transport.h`
- **Behaviour:** Reported by the user on 2026-11-16: a Node loses contact with the Mainframe and does not react when the Mainframe is restarted. Only a reset of the Node brings it back. Added by the user on 2026-11-17: the stuck Node kept playing its pattern, and the show was started again by button after the Mainframe restart, without effect on the Node. Not yet recorded: which Node kind and how long it had been running.
- **Verified:** user observation on hardware. No serial log of a stuck Node exists, so no cause is named. Code reading gives these facts, none of them tied to the observation:
  - The Node has no link supervision and never re-initialises its radio. `recoverEspNow()` in `espnow_transport.h` is not called anywhere. If the receive path stops, only a reset restores it. The missing timeout is specified behaviour ([protocol](openspec/specs/protocol/spec.md), "No automatic return to demo").
  - A restarted Mainframe sends one `MainframeHello`, unacknowledged, directly after `esp_now_init()`. Until the show is started by button it then sends only `NodeVisualState` every 3 s, which has no visible effect on a Node without an override. A Node that misses the single hello shows no reaction to the restart.
  - `serviceClock()` loops `while (now - nextBeatAtMs >= 0)` and adds `beatIntervalMs`. An interval of 0 ms (BUG-001, BPM above 60000) would never leave this loop and the Node would hang until reset. A valid V2 Mainframe is not known to send such a value.
  - `NodeOutput::flash100()` returns early while a visual override is active, so the connect confirmation after a hello is invisible on a Node that last received a non-default `NodeVisualState`.
- **Narrowed by the user's answers:** a Node that keeps playing has a running main loop, so the 0 ms loop does not fit. A restarted show sends `SceneSelect` and `ClockSync` every bar, so a missed hello does not explain it either. What remains is that packets stopped reaching the Node's receive path; where, is not measured.
- **Next measurement:** read the serial port of the stuck Node before resetting it, without toggling DTR/RTS (opening the port can reset the Atom). The Node prints a `VISUAL:` line for every `NodeVisualState`, so a healthy link shows one line every 3 s while the Mainframe runs. `status` answers: main loop alive; then `CONTROL: mode=` gives the run mode. No answer: main loop hung. Answer but no `VISUAL:` lines: packets no longer reach the receive callback.

### BUG-017: Unused `debugSnapshot()` and stale overlay comment

- **Area:** Mainframe, personality, debug overlay
- **Where:** `apps/mainframe/src/personality.h`, `debugSnapshot()` and the comment above it; `apps/mainframe/src/personality.cpp`, `Personality::debugSnapshot()`
- **Behaviour:** The header comment says the debug readout rounds the internal values with `debugSnapshot()` while control logic uses `snapshot()`. Since `511367c` `main.cpp` passes the `snapshot()` result to the overlay, so it shows values rounded down, and `debugSnapshot()` has no caller.
- **Decision:** User decision 2026-11-16: the overlay keeps showing the rounded-down `snapshot()` values, the same values the sound and light thresholds use. Fix: delete `debugSnapshot()` and the stale comment.
- **Verified:** code reading and `git show 511367c -- apps/mainframe/src/main.cpp`.
- **Source:** [openspec/specs/personality/spec.md](openspec/specs/personality/spec.md), requirement "Mood debug overlay content"

### BUG-018: Comment calls the creature start a low-energy state

- **Area:** Mainframe, personality, comments
- **Where:** `apps/mainframe/src/personality.cpp`, comment in `Personality::update()` where creature mode starts
- **Behaviour:** The comment says the creature display "starts in the configured low-energy state"; the code sets energy to 100, a full battery. The matching statements in [project-context.md](docs/project-context.md) (B.6 energy start value, section 0 energy after the first click) were corrected on 2026-11-16.
- **Verified:** compared with `apps/mainframe/src/main.cpp` (`moodAudio()`) and `tests/personality.cpp`.

### BUG-019: RGB long press in demo also advances the scene

- **Area:** Node, button
- **Where:** `apps/node/src/main.cpp`, `serviceButton()`; `switchNodeRole()` → `selectProgram()` → `resetDemoClockState()` → `clearButtonGesture()`
- **Behaviour:** On an RGB strip Node in `Demo`, a long press cycles the role, and the role switch restarts the demo with scene 0. That restart calls `clearButtonGesture()` while the button is still held, so the next loop pass registers a new press. Its release then counts as a short press and selects scene 1. Held for another long-press time, the button cycles the role again. A single long press thus ends with the next role on scene 1 instead of scene 0. Under Mainframe control the role switch does not restart the demo, so the gesture is not reset there.
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirements "Long press per Node kind" and "Applying a new profile"

### BUG-020: Role or kind switch while waiting replaces the idle output

- **Area:** Node, kind and role switching
- **Where:** `apps/node/src/main.cpp`, `switchNodeKind()` and `switchNodeRole()` (`mainframeOwned` branch)
- **Behaviour:** After a hello the Node shows the idle command and waits in `MainframeWaiting` for the first ClockSync. A kind or role switch by button or serial command in this state applies the command of the current scene index for the new kind and role. That index is usually still the one from the demo, so the Node shows a scene the Mainframe has not selected. Intended (user decision, 2026-11-16): a waiting Node keeps the idle output; the new kind or role takes effect with the next scene or clock from the Mainframe.
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirement "Applying a new profile"

### BUG-021: Node text overlay lights a muted flashlight

- **Area:** Node, mute and text overlay
- **Where:** `apps/node/src/main.cpp`, `onBeat()` → `startPendingNodeTextOverlay()`, `serviceOutput()`
- **Behaviour:** A long press mutes a flashlight Node. If it is in `MainframeRunning` and then receives a `NodeText`, the next beat starts the overlay regardless of the mute and calls `showTemporaryLit(true)` for the first Morse element. `serviceOutput()` returns early while muted, so the overlay never advances and the flashlight stays lit until something clears the overlay or ends the mute. The V2 Mainframe does not send `NodeText` yet, so this is not reachable in a V2-only installation today.
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirement "Output mute"; [protocol](openspec/specs/protocol/spec.md), requirement "Node text layout and handling"

### BUG-022: Node help text omits the demo-only limit for the flashlight short press

- **Area:** Node, serial help
- **Where:** `apps/node/src/main.cpp`, `printHelp()`
- **Behaviour:** For an RGB strip the help says `short press -> next scene (demo only)`; for a flashlight it says `short press -> next scene`. `serviceButton()` handles a short press only in `Demo` for both kinds, so the flashlight line is wrong under Mainframe control.
- **Verified:** code reading only.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirements "Short press" and "Help text"

### BUG-023: Filter monitor colour strength wraps to 0 at drift 255

- **Area:** Node, serial filter monitor
- **Where:** `apps/node/src/main.cpp`, `colorDriftStrength()`
- **Behaviour:** The strength is `(colorDrift - 127) * 2` cast to `uint8_t`. For a drift of 255 the result is 256 and wraps to 0, so the monitor and `status` print `COLOR: magenta 0` at the strongest magenta drift. The drift is a renderer mix value that can in principle reach 255.
- **Verified:** code reading only. Not observed in a log.
- **Spec:** [node-device](openspec/specs/node-device/spec.md), requirement "Filter monitor"

### BUG-024: Flashlight stays lit after a full flash override ends

- **Area:** Node, flashlight output and visual state
- **Where:** `apps/node/src/node_output.cpp`, `NodeOutput::service()`; `apps/node/src/flashlight_renderer.cpp`, `service()` and `setLit()`
- **Behaviour:** With flash override `Full`, `NodeOutput::service()` calls `setLit(true)` on every pass. When the Mainframe then sends the neutral directive, `service()` falls through to `FlashlightRenderer::service()`, which does nothing, and no other code turns the flash off. The flash stays fully lit until the next scene flash or burst ends with `allOff()`. Depending on the active motif this is up to one beat (beat pulse on every beat) or up to four bars (slow pulse, quad pulse, riser). The Mainframe sends `Full` at annoyance ≥ 90 and the neutral directive once annoyance falls below it.
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [node-output](openspec/specs/node-output/spec.md), Requirements "Visual override on a flashlight" and "Visual override".

### BUG-025: Node text overlay starts while its output is suppressed

- **Area:** Node, text overlay
- **Where:** `apps/node/src/main.cpp`, `onBeat()`, `startPendingNodeTextOverlay()`, `syncNodeTextOverlayToBeat()` and `serviceOutput()`
- **Behaviour:** A queued text starts on the next beat even when the output is muted or under a visual override, but only `serviceOutput()` advances it, and `serviceOutput()` returns before that in both cases. On a muted flashlight, `showTemporaryLit(true)` is not blocked by the mute, so the flash turns on for the first unit and stays lit as long as the mute lasts. Under a visual override the first lit unit is swallowed by `showTemporaryLit()`, the text is held, and it continues from the second unit once the override ends. Holding the text and continuing it after the override is the intended behaviour (user decision, 2026-11-16); losing the first unit is not. The V2 Mainframe does not send NodeText yet, so neither case can happen today.
- **Verified:** code reading only.
- **Spec:** [node-output](openspec/specs/node-output/spec.md), Requirements "Output precedence" and "Text overlay output".

### BUG-026: Connect confirmation is invisible under a visual override

- **Area:** Node, hello reaction and visual state
- **Where:** `apps/node/src/main.cpp`, `runMainframeConnectSequence()`; `apps/node/src/node_output.cpp`, `NodeOutput::flash100()`
- **Behaviour:** `flash100()` returns at once while `hasVisualOverride()` is true, so a Node that still holds a visual override shows none of the three connect flashes when it processes a MainframeHello, for example after the Mainframe rebooted during an override. The delays between the flashes still run, so the Node blocks without visible output. The connect confirmation should always be visible (user decision, 2026-11-16).
- **Verified:** code reading only. Not observed on a Node.
- **Spec:** [protocol](openspec/specs/protocol/spec.md), Requirement "Node reaction to hello"; [node-output](openspec/specs/node-output/spec.md), Requirement "Output precedence".

### BUG-027: project-context.md describes old scene flash profiles

- **Area:** Documentation, scenes
- **Where:** [project-context.md, A.3](docs/project-context.md#a3-scenes-renderers-and-concrete-problem-areas); the German original `8560c2a:docs/MAINFRAME_V2.md` says the same
- **Behaviour:** A.3 gives scene 1's flash profile as a window of eight bars with weights Drive/Heavy/Double/Quad/Riser = 30/14/24/20/12. `shared/include/scene_programs.h` has a window of twelve bars and weights pulse/slow pulse/double/quad/riser = 30/5/24/20/5, unchanged since the initial V2 commit; the motifs are called "Beat Pulse", "Slow Pulse", "Double Pulse", "Quad Pulse" and "Riser 5x". A.3 also says scene 2 uses Drive, Double and Quad motifs; its profile weights all five motifs (118/35/92/45/40).
- **Verified:** compared with `shared/include/scene_programs.h` and `git log` of that file.
- **Spec:** [node-output](openspec/specs/node-output/spec.md), Current values.

### BUG-028: Show clock runs slower than the BPM it sends

- **Area:** Mainframe, show clock
- **Where:** `apps/mainframe/src/main.cpp`, `serviceShowClock()` (`nextBeatAtMs = now + beatIntervalMs()`)
- **Behaviour:** When a beat is due, `serviceShowClock()` schedules the next one at `now + beatIntervalMs()`, where `now` is the time of the loop pass that noticed the beat, not the time the beat was due. Each beat is therefore late by the time between its due time and that pass (the loop sleeps 5 ms per pass plus display, radio and sensor work), and the lateness adds up from beat to beat. The show clock runs slower than the `currentBpm` it sends in ClockSync. The Nodes run their own clock at the received BPM and are pulled back on every bar's ClockSync, so the installation as a whole follows the slower Mainframe bar. The truncation of `60000 / currentBpm` to whole milliseconds shortens each beat by less than 1 ms, which only partly offsets this. Only a tempo adoption re-anchors the phase.
- **Verified:** code reading, and the lateness per beat measured on the Mainframe on 2026-11-17 with the measurement environment `mainframe-loop-load-trial` (branch `claude/blocked-sections-analysis-a46e53`, commits `6822e7b` and `1dd1a97`, not merged): beats fire on average 6–8 ms late with show and music, 12–14 ms late while a panel text is visible, at most 44 ms. Computed from these values, not measured: at about 120 BPM the clock runs roughly 1.4 % slow without text and 2.4 % slow with text between two tempo adoptions. How much of this reaches the Nodes with music playing has not been measured, because `applyV2Tempo` re-anchors the clock on every adoption.
- **Fix note:** a fix changes the timing the Nodes see. It needs its own change with a host test (`tests/show_clock.cpp`) and a check on the device.
- **Spec:** [tempo](openspec/specs/tempo/spec.md), Requirement "Free-running show clock".

### BUG-029: Loop stalls for 187–281 ms before the show starts

- **Area:** Mainframe, main loop, motion
- **Where:** `apps/mainframe/src/main.cpp`, `loop()`, the part after the IMU block and before `eyeRenderer.service` (depression poetry, sound watcher with mood sound and snore start, visual state, eye arguments)
- **Behaviour:** While the show is not started (creature mode, audio analysis off), single loop passes take 187–281 ms in that part of the loop. The gap between two IMU services is then 193–281 ms, above the 150 ms limit at which `MotionEvents::feed` clears its window, so a gesture in progress is dropped.
- **Verified:** measured on the Mainframe on 2026-11-17 with the measurement environment `mainframe-loop-load-trial` (branch `claude/blocked-sections-analysis-a46e53`, not merged): three passes of 187, 276 and 276 ms within about 60 s. The cause has not been measured; which call in that part blocks still has to be timed on the device.

## Closed

### BUG-015: A held tilt made the motion classifier drop gestures

- **Closed:** 2026-11-16, fixed by the motion rework (`33623b1`, merged in `381ef33`).
- **Was:** the reference pose was fixed at boot, so a Mainframe resting more than about 25 degrees off it reported Tilt every 2 s for as long as it stayed there, and the one-second event spacing dropped gestures after each Tilt (host trace at 45 degrees: 7 of 15 knocks reported).
- **Now:** Tilt is gone. Tipping beyond 60 degrees reports Rotate once. Turning away around the vertical reports Reposition every 2 s, and the classifier learns the new looking direction after 60 s without turning, which ends the repeats. Within those 60 s the one-second spacing can still drop a gesture after each Reposition; the user tried the new behaviour in the wooden sphere on 2026-11-16 and accepted it.
- **Spec:** [motion](openspec/specs/motion/spec.md), requirements "Reposition" and "Learning a new looking direction".

### BUG-016: project-context.md described Rotate as per-axis integration

- **Closed:** 2026-11-16, obsolete. The motion rework redefined Rotate as tipping or a full turn around the vertical, and [project-context.md, B.6](docs/project-context.md#b6-local-mood-parameters-and-debug-overlay--decision-of-2026-09-26) was rewritten with it. Integrating the rotation-rate magnitude inside a window now only decides whether a window counts as handling.

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
