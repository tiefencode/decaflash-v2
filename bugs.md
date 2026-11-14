# Bugs

Known defects in the V2 code and its documentation. Open design decisions, planned features and pending hardware acceptance stay in [docs/MAINFRAME_V2.md](docs/MAINFRAME_V2.md); this file lists only behaviour that is wrong or text that contradicts the code.

Each entry says what was verified and how. A bug seen on hardware needs a measurement on the device before a cause or a fix is named (see [CLAUDE.md](CLAUDE.md)).

## Open

### BUG-001: Node accepts any non-zero clock BPM

- **Area:** Node, clock sync
- **Where:** `apps/node/src/main.cpp`, `applyClockSync()` and `bpmToIntervalMs()`
- **Behaviour:** `applyClockSync()` rejects only `bpm == 0`. Any other value is passed to `bpmToIntervalMs()`, which returns `60000 / bpm` without a range check. A BPM above 60000 gives a beat interval of 0 ms; very low values give beats a minute or more apart. The Mainframe clock is limited to 60–180 BPM and the tempo adoption to 80–180 BPM, so a valid V2 Mainframe is not known to send such values.
- **Verified:** code reading only. No invalid packet has been observed.
- **Source:** [MAINFRAME_V2.md, A.5](docs/MAINFRAME_V2.md#a5-the-node-clock-is-currently-not-a-soft-sync)

### BUG-002: Blocking Node output delays radio and clock handling

- **Area:** Node, output and radio
- **Where:** `apps/node/src/flashlight_renderer.cpp` (`delay(8)`, `delay(flashMs)`), `apps/node/src/rgb_strip_renderer.cpp` (`delay(8)`, `delay(flashMs)`), hello confirmation in `apps/node/src/main.cpp`
- **Behaviour:** Flash output blocks the main loop with `delay(flashMs)` plus 8 ms pauses. The hello confirmation blocks for about 2.3 seconds with three flashes, and the role confirmation blocks as well. Staged radio packets are processed only afterwards, so scene and clock arrive late. Because `applyClockSync()` uses the processing time as `now`, the beat phase shifts by the blocked time.
- **Verified:** code reading only. The delay has not been measured on a Node.
- **Source:** [MAINFRAME_V2.md, A.3 item 7 and A.5](docs/MAINFRAME_V2.md#a3-scenes-renderers-and-concrete-problem-areas)

### BUG-003: Personality uses a confidence threshold from the old tempo scale

- **Area:** Mainframe, personality
- **Where:** `apps/mainframe/src/personality.cpp`, the checks `audio.confidence >= 68` and the beat bonus `(audio.confidence - 68)`
- **Behaviour:** The threshold of 68 comes from the `BeatAnalyzer` confidence scale. Since `30ac9a7` the confidence comes from `V2TempoTracker`, which uses a different heuristic scale (host test with synthetic kicks: 55–83). If real music stays below 68, tempo trust and the beat bonus for energy never activate.
- **Verified:** code reading and host test values only. Whether real music reaches 68 is untested on the device; this is hardware check 8 in [MAINFRAME_V2.md, section 0](docs/MAINFRAME_V2.md#open-hardware-checks-for-the-productive-tempo-path).

### BUG-004: `platformio.ini` comments describe the tempo path before `30ac9a7`

- **Area:** Build configuration, comments
- **Where:** `platformio.ini`, comments above `mainframe-beat-tracker` and `mainframe-spectral-capture`
- **Behaviour:** The comment on `mainframe-beat-tracker` says the production `BeatAnalyzer` stays in charge of the show clock; the comment on `mainframe-spectral-capture` calls the spectral values the input of the replacement V2. Since `30ac9a7` `V2TempoTracker` drives the show clock and does not use the spectral values.
- **Verified:** compared with `apps/mainframe/src/main.cpp` and `v2_tempo_tracker.cpp`.

### BUG-005: Fixture README calls the discarded `BpmTracker` the V2 core

- **Area:** Tests, documentation
- **Where:** `tests/fixtures/bpm-bands/README.md`
- **Behaviour:** The README says the report compares the fixtures with the "V2-Kern" (V2 core). The report replays them through `BpmTracker`, which is not productive. The productive `V2TempoTracker` cannot be evaluated with these fixtures, because they contain no PCM and no onset strength.
- **Verified:** compared with `tests/run_bpm_band_trace_report.sh` and the analysis in [MAINFRAME_V2.md, section 0](docs/MAINFRAME_V2.md#fixtures-and-host-tests).

## Closed

### BUG-006: Scene changes not taken over reliably by Nodes

- **Closed:** 2026-11-14, according to the user the problem has not occurred for a long time.
- **Cause:** not proven. No log, measurement or recorded hardware test ties the fix to a specific change. The only change to the scene path in that period is the switch to the single `SceneSelect` message in the old project (`26f4500`, 2026-09-23).
- **Details:** [MAINFRAME_V2.md, A.3](docs/MAINFRAME_V2.md#a3-scenes-renderers-and-concrete-problem-areas)
