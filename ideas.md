# Ideas

A collection point for things that are not decided yet. Nothing in this file counts as a rule or as an assignment.

## Rule candidates

Possible working rules for `CLAUDE.md`. A candidate is not a rule. The procedure is described by the skill `rule-candidates`.

- Move training data and models under `models/` (ignored) into the main checkout before removing a worktree; `git worktree remove` deletes ignored files without asking. (Nov 20)
- Before a function is switched off on request, say what the user will see on the device afterwards (example: gate off meant almost no verses any more). (Nov 20)


## Noted for later

Plans that are settled but wait for something else.

### Eye base cycle without text, or a low-level step

- **Why:** Decision of the user of 2026-11-17: check it as its own change after `reduce-panel-frame-rate`. Measured without text: 40 ms instead of 25 ms lowers the load of core 1 from 58.8 % to 47.1 % (`docs/project-context.md`, section E).
- **What:** either lower the base cycle or a fixed low-level step (e.g. 40 ms without text, 67 ms with text) that is switched on by known states, not by measured load. Clarify values and triggers in an interview.
- **Reserve:** Statement of the user of 2026-11-17: 40 ms as the general cycle of the eye, also without text, is an option we can use when the CPU is a bottleneck (for example for the language model). Measured in the sweep: audio gap without text 51 instead of 55 ms, with text at 40 ms 49 instead of 92 ms.
- **When:** after the merge of `reduce-panel-frame-rate`; fits the language model prototype.
- **Check:** look on the device, measurement with `mainframe-panel-rate-sweep`.

### Split the Node `main.cpp`

- **Why:** `apps/node/src/main.cpp` has about 1,660 lines and is too big. For comparison: `apps/mainframe/src/main.cpp` has about 500.
- **What:** move parts into their own files under `apps/node/src`. Candidates by today's structure: button gestures, Morse/text overlay, demo cycle and beat rendering, configuration in NVS (node kind, role), serial commands (help, status, programs). Pure restructuring, no new behaviour.
- **When:** after the end-to-end test with a flashed V2 node (rule in `CLAUDE.md`), so that there is a verified state to compare against.
- **Check:** `sh tests/run_all.sh`, `pio run -e node`, then compare against the old state on the device.

### Remove mute on the Flashlight node

- **Why:** Decision of the user of 2026-11-16: muting by long press has no recognisable use and is probably a leftover from V1. A second long press does not lift it; under Mainframe control it ends only with a scene change, a Hello or a serial `mode`/`role` command.
- **Today:** described in `openspec/specs/node-device/spec.md`, requirements "Long press per Node kind" and "Output mute".
- **What:** remove `outputMuted` and the mute branch in `serviceButton()`; the long press on the Flashlight then has no function. Adjust the help text (`long press -> turn output off`) and `status` (`muted=`). This also removes BUG-021 ("Node text overlay lights a muted flashlight") in `bugs.md`.
- **When:** after the end-to-end test with a flashed V2 node (rule in `CLAUDE.md`), as its own OpenSpec change; fits splitting the Node `main.cpp`.
- **Check:** `sh tests/run_all.sh`, `pio run -e node`, on the device a long press on the Flashlight in demo and under Mainframe control.

### Host tests for scenes and node output

- **Why:** Neither `shared/include/scene_programs.h` nor the node output in `apps/node/src` has a host test. The node-output spec (`openspec/specs/node-output/spec.md`) describes the behaviour but does not check it.
- **What:** host tests for the pure logic: motif choice and parameters of the flash variation profile (deterministic per cycle), trigger cycles and beats, Morse encoding of the text overlay with gaps of 1/2/4 units. Renderers with FastLED or GPIO stay out as long as they do not run without hardware.
- **When:** open; useful before or together with "Split the Node `main.cpp`", because the Morse logic has to be extracted from `main.cpp` for it.
- **Check:** `sh tests/run_all.sh`.

### Unify RGB patterns

- **Why:** The RGB pattern PulseRow (a row of short pulses, triggered on a beat) uses no scene, nor do its templates `heartbeat()` and `triplePulse()` in `shared/include/scene_programs.h`. The patterns Heartbeat and RiserPulse are at heart pulse rows with a hard-coded shape. Described in `openspec/specs/node-output/spec.md`.
- **What:** extend PulseRow so that it can represent Heartbeat and RiserPulse: a decay after the last pulse, times in beat fractions instead of milliseconds, computation from the cycle position instead of as an event (so that it survives a missed beat or an override), strength per pulse. Then make Heartbeat and RiserPulse PulseRow templates and remove the special renderers, or delete PulseRow with its templates if it is not needed.
- **When:** open; together with the final light design of the scenes and only after the end-to-end test with a flashed V2 node (rule in `CLAUDE.md`).
- **Check:** `sh tests/run_all.sh`, `pio run -e node`, on the device compare old and new Heartbeat or Riser; the node-output spec changes with it.

### Blinking at rest

- **Why:** According to the requirement in `docs/project-context.md` B.2, blinking belongs to the resting behaviour of the eye. It is not implemented; the lids close today only when sleeping (`drawSleepLids()` in `apps/mainframe/src/eye_renderer.cpp`).
- **What:** occasional short blinking while the eye is awake. The obvious way is to close and reopen the existing sleep lids briefly. Whether mood (for example `attention`) influences the frequency is open.
- **When:** open; does not depend on the end-to-end test, because it concerns neither radio nor node output.
- **Afterwards:** add a requirement to the eye-iris spec (`openspec/specs/eye-iris/spec.md`).

### Message panel: consume nothing unseen

- **When:** after the inventory of the specs, as its own OpenSpec change.
- **Today:** While sleeping and while the debug view is shown, the renderer does not draw the panel. Texts and poems are triggered nevertheless and count as shown: the cooldown of the motion kind starts, the poem is taken from the list and the two-minute interval restarts. During the boot animation the renderer, on the other hand, does draw the panel but should not (BUG-010). Described in `openspec/specs/message-panel/spec.md`, requirement "Panel hidden while sleeping or in debug view".
- **What:** texts on motions are discarded in these states and during the boot animation without starting the cooldown; if the motion wakes the being, the text comes as before. A due poem waits until the panel is visible again, as when the panel is busy.
- **Where:** `apps/mainframe/src/main.cpp` (call of `showMotion` and `serviceDepressionPoetry`) and `apps/mainframe/src/eye_renderer.cpp` (boot check when drawing), not in the panel itself. The spec changes with it.

### Presence bus at 400 kHz

- **State:** Proposal from the measurement of 2026-11-17, not yet decided by the user.
- **Why:** The presence sensor (STHS34PF80) is polled at 100 kHz and takes 4.4–4.5 % of core 1 that way. At 400 kHz it was 1.7–1.9 % in the measurement: a poll without sample 0.31–0.37 ms instead of 0.69–0.75 ms, one with sample 1.04–1.10 ms instead of 3.05–3.12 ms, without read errors.
- **What:** one constant, `kBusHz` in `apps/mainframe/src/presence_sensor.cpp`.
- **Limits:** At 400 kHz only about 65 s were measured on the current Grove cable. The data sheet (DS13916, section 3.1.2) states I2C up to 1 MHz.
- **Check:** `sh tests/run_all.sh`, `pio run -e mainframe`, then watch the read-error counter on the device over a longer run.
- **Source:** measurement environment `mainframe-loop-load-trial` on the branch `claude/blocked-sections-analysis-a46e53` (not merged).
- **Not part of it:** reading the motion sensor through the FIFO of the BMI270. It would bring all 100 samples per second instead of 43–65 to the classifier, but the knock thresholds are tuned to today's data, and detection from FIFO data has not been checked.

### Split the Mainframe main loop into tasks

- **State:** Proposal of 2026-11-17, noted by the user as a plan, scope and split not decided yet.
- **Why:** Today everything of the Mainframe runs one after another in `loop()` on core 1. A long pass (eye, panel) delays fetching the audio blocks and the IMU samples. In the language model measurement of 2026-11-17 (`docs/slm-prototype.md`, results for 6.4) the largest gaps with the model computing became 2 to 11 ms longer (text state: audio 67 instead of 56 ms, IMU 50 instead of 39 ms), although the model computes on core 0. A separate task with higher priority for audio and IMU would decouple these gaps from the eye.
- **What:** audio fetching, tempo and IMU (possibly the show clock) into a task above `loop()`; eye, panel, personality and presence stay in `loop()`. Exchange shared data without blocking.
- **Limits:** Not measured whether it helps: core 1 also becomes slower with the model without more code running there; if that is memory or cache contention, a separate task does not help against it. Large change to the productive audio and tempo path.
- **When:** after the BPM detection is clarified, as its own OpenSpec change with an interview.
- **Check:** before and after on the device with `mainframe-loop-load` or `mainframe-slm-capture` (audio and IMU gaps, beat delay), `sh tests/run_all.sh`.
- **Related:** "Presence bus at 400 kHz" saves about 2.6 percentage points of core 1, "Eye base cycle without text" about 7 to 11 points; both smaller and without restructuring.
- **Not meant:** merely splitting `apps/mainframe/src/main.cpp` into files; that changes nothing about the timing.
