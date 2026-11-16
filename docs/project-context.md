# Mainframe v2 – Project Context

As of 2026-09-25, last reconciled with the code on 2026-11-14 (commit `2786c11`). The reconciliation covered the tempo/BPM work from 2026-10-01 to 2026-10-03, the active module state in A.1 and the plan-versus-actual comparison in B.5; it was purely reading and documentation work with host tests, without a firmware build, flash or device measurement. Independent V2 starting point: commit `ed5dc7a` (`initial commit. copy from decaflash original`). Historical initial analysis from 2026-09-22: commit `f9cabdf1d04d314f240faecbc239ec706aa4da84` in the previous project, with the communication changes described afterwards. When this was written, those changes were not yet proven on hardware as a fix for the reported scene problem; according to the user (2026-11-14) the problem has not occurred for a long time and is fixed, see A.3.

This document separates the **actual state verified from the code**, the **v2 planned by the user** and **open or experimental decisions**. It is not an implementation and does not approve one. The README remains the entry point for operational commands; this document bundles the explicitly requested v2 context.

The active central controller is called **Mainframe**. All versioned V2 sources and documents use this name. Only the local ignored copy of the historical M5Atom code keeps its old identifiers.

Scope of the historical initial analysis: all versioned firmware sources and headers, the shared protocol and scenes, the PlatformIO configuration, the Cloudflare Worker including its configuration, the existing documentation and the configuration templates. Local secrets, generated build artefacts and third-party libraries are not part of the source review. At that time the controller and node were built after the communication fix; no firmware was flashed and no cloud calls or hardware measurements were made. The current V2 state, including flashing and hardware checks, is stated bindingly in section 0.

## 0. Binding V2 state as of 2026-09-25

This section and the corrected statements below replace contradicting older planning assumptions. The full review scope and the builds described above belong to the historical analysis. In the first review step, the project takeover, the Git configuration, selected code paths and the USB inventory were checked read-only; only this document was updated. The V2 documentation was then consolidated: the README as a short entry point, this context for analysis and decisions, the Worker README for cloud details and the V1 outline as a historical draft. These first documentation steps included no builds or firmware changes. In the following implementation step, the radio identity was changed, an isolated hardware test was added, the Mainframe was flashed and the first offline radio path was activated. No cloud calls or deployments.

### Independent project and takeover

- Working directory and own Git root: `/Users/tiefencode/Projekte/decaflash-v2`, own `.git`; configured fetch/push target: `git@github.com:tiefencode/decaflash-v2.git`. Reachability and remote content were not checked. The working tree was clean before this documentation change.
- Manually taken-over state: M5Atom controller with PDM audio/BPM, audio follow, matrix VU/text and optional cloud connection; Node firmware with flashlight/RGB output; shared scenes, clock/text protocol and ESP-NOW transport; Worker and documentation.
- In the initial comparison, the checked files in `apps`, `shared`, `include`, `workers` and `docs` were byte-identical to the old project (generated Worker dependencies and `.wrangler` excluded). No symlinks and no file inodes shared with the old counterpart were found there. Secrets were not printed; the local Wi-Fi/cloud configuration files are ignored and only their templates are versioned.
- The old hardware and the old project remain independently functional and unchanged. No shared mutable source files. Where needed, V2 includes its own Mainframe **and** Node firmware; selective reuse instead of a wholesale rewrite.
- The README and working rules are maintained for the independent V2 repo. Operational references to the old project and the old Worker deployment guide were removed in the consolidation step; historical analysis references remain as such.
- The active `mainframe` build targets the AtomS3R and contains the validated bring-up for memory, display, button and Voice Base I²C as well as the offline ESP-NOW core: show start and scene selection via the front button, scene broadcast and clock sync. The show starts at 120 BPM; since 2026-10-03 the show clock adopts the confirmed tempo of the PCM tempo tracker (see below). Voice Base audio analysis, eye/VU, personality, IMU gestures and the text panel are included (active module state in A.1). Wi-Fi and cloud are still not part of the V2 Mainframe. `node` remains the M5Atom/FastLED firmware. A local ignored copy of the former M5Atom controller lives under `reference/m5atom_controller`; V2 takes individual building blocks from it where needed but does not version the copy. The original project and its Git history are canonical.
- The Worker was copied along as source code. Taking over its old Cloudflare deployment connection has not been requested; an external V2 connection was neither checked nor set up.

### Iris VU based on commit `0501020`

The current Mainframe contains Voice Base capture and the procedural low-poly EyeRenderer. `BeatAnalyzer` and `AudioFollower` were the tempo path when this section was written; since commit `30ac9a7` they are no longer wired in (see the following section). The iris now uses the already DC-corrected mean absolute level of each 256-sample block (16 ms), independent of the music gate. A dedicated display envelope has an immediate attack and a release of at most about 160 ms. The provisional absolute thresholds are 80 (black) and 2400 (full scale); codec/gain and room noise must be checked on the device. With missing audio blocks the VU level drops to 0 after 250 ms, with a failed capture immediately; all iris faces then fall back to the dark-blue base colour, so the iris does not turn black.

After the first hardware test (VU works according to the user, rendering too coarse) the iris consists of 36 independent VU polygons that fill the full iris circle. As with the old brain meter, a shuffled order determines the active faces; every 220 ms four active/inactive slots swap. Each face fades in and out separately and runs through black → dark blue `(8,34,110)` → light blue `(40,212,255)` → magenta `(255,30,230)`. Even in silence the full circle stays in a visible dark blue: every face is drawn with at least a fixed base value. The audio level determines how many faces are active; active faces fade up towards magenta and inactive faces fade down to the base colour. A deterministic brightness deviation of at most 25 percent is applied to each RGB channel only afterwards: it produces faceted lighter and darker variants of the same base colour without shifting the colour scale per polygon. The iris and pupil logic remain independent of this. The eyeball is drawn procedurally and flat-shaded into a single 128×128 RGB565 sclera buffer, using the same confirmed 16-segment/two-ring faceting. This layer is deliberately updated only every 125 ms; there are no longer 45 precomputed full-screen sprites. The gaze still consists of short, S-curve-accelerated saccades to randomly chosen targets followed by fixations; it does not circle continuously. With high `attention` the targets become much wider, the saccades faster and the fixations more frequent; at 100 the gaze reaches up to ±22 and ±12 pixels and changes after about 350–850 ms. From 90 `annoyance` a black, centre-heavy low-poly upper lid lies over the ball and the iris switches to a saturated dark red → red → reddish orange; the same threshold also triggers the Node light profile. From 90 `loneliness` a black upper lid appears that tapers to a point in the middle, and the gaze drops slightly. The existing 128×128 output sprite is retained. The VU evaluation needs only constant integer work per audio block and is independent of the tempo path.

Host check: `sh tests/run_iris_vu.sh` checks the gate, monotonic level, saturation, facet count, release, capture failure and time overflow. In addition, run all host tests via `sh tests/run_all.sh` and `pio run -e mainframe`. Visual acceptance, level calibration and runtime/radio measurements under audio load are still pending for this change. Sector fades and drift advance per drawn frame, so they slow down when frames are delayed; only the VU level itself follows the audio blocks (code reading, 2026-11-15).

### PCM tempo tracker: from experiment to show clock (2026-09-30 to 2026-10-03, hardware acceptance open)

**Summary (state `2786c11`):** The show clock is driven by `V2TempoTracker` (PCM-based) through the confirmation stage `V2TempoAdoption`. `BeatAnalyzer` and `AudioFollower` have not been connected to `main.cpp` since commit `30ac9a7`; they are still in the source tree and are used only by their host tests. For the productive path after this switch there is **no** measurement in this document or in the local device logs.

#### History according to Git

| Date | Commits | Step |
| --- | --- | --- |
| 09-30 | `5a025c0` | Experimental `TempoTracker` (PCM, four-band flux, autocorrelation, tempo lock) runs in parallel; `BeatAnalyzer` + `AudioFollower` keep driving the clock. Sample time base and 8 KiB PCM ring in `VoiceBaseInput`. |
| 10-01 | `2e04ca1`, `c018767` | Change to the AudioFollower response time, reverted immediately. |
| 10-01 | `f37a876`–`cc7312e` | `TempoTracker` and its host test deleted. Restart with `BpmTracker`: integer-only, works on 16 ms level frames instead of PCM, 80–180 BPM, no song-change lock. Still only a diagnostic candidate next to the productive path. |
| 10-01 to 10-03 | `548cdbf`–`5f6b5cb` | Trace capture from the device (`BPM_TRACE3`, `SPECTRAL_TRACE`), checked-in reference windows under `tests/fixtures/bpm*` and offline reports. |
| 10-03 | `30ac9a7` | The PCM estimator returns as `V2TempoTracker` – the same onset and correlation core as `TempoTracker`, but without lock state – and becomes the source of the show clock. `BeatAnalyzer`, `AudioFollower` and `BpmTracker` are removed from `main.cpp`/`VoiceBaseInput`. |
| 10-03 | `67c4f96` | `V2TempoAdoption`: the show adopts a new tempo only after two matching evaluations. |
| 10-03 | `c2cc4aa`–`1588d7d` | `V2_TRACE` capture, `feedOnsetTrace()` for replays, debug display `show BPM/candidate`. |

`a986f20` (`tracker-v2`) is the merge of this branch and changes no content.

#### What drives the show clock now

1. **Capture and time base (`voice_base_input.*`):** The analysis time comes from a continuous 16 kHz sample counter, not from the main-loop processing time. The capture callback re-queues the two M5Unified recording buffers immediately and copies each finished 256-sample block (16 ms) into a 16-frame PCM ring (8 KiB PSRAM). The evaluation runs in the main loop. A full ring is counted; the following sequence jump resets the tracker instead of claiming a tempo from gapped PCM.
2. **Onset strength (`v2_tempo_tracker.cpp`, `feed`):** Per block the maximum of the broadband level rise, the bass rise (high-pass near 40 Hz, two low-pass stages near 180 Hz) and a log-scaled four-band spectral flux (one-pole cutoffs at about 200, 800, 2500 and 6000 Hz). The rises are normalised to the current level; there is no music gate and no absolute onset threshold as in the old scanner. No FFT, no additional library.
3. **Individual onsets:** local maximum above `mean + 2.2 × mean deviation`, refractory time 96 ms; the last 16 onset timestamps are kept.
4. **Tempo evaluation (`updateTempo`, every 250 ms):** only from 384 frames (6.14 s) of gapless history. Normalised autocorrelation of up to 512 onset frames (8.2 s), decimated to 32 ms; coarse search 70–190 BPM in 2 BPM steps, then refinement by ±2 BPM. The double tempo replaces the correlation winner only if the individual onsets support it by at least 3 points more and its correlation reaches at least 35 % of the winner's. A direct mean of up to eight recent onset intervals (300–900 ms, each deviating by at most 8 %) replaces the value if it matches it, its double or its half within ±4 BPM.
5. **Confidence:** `0.72 × periodicity + 0.28 × separation from the best competitor (at least 4 BPM away)`, as 0–100; at least 55 when it agrees with the direct onset tempo. This is a heuristic value on a different scale from the BeatAnalyzer confidence, not a calibrated probability.
6. **No lock in the tracker:** `estimate().bpm` is always the current raw value. The lock described in `5a025c0` (four close estimates to acquire, twelve counter-observations to switch, half/double tempo as one family) no longer exists. A time jump of more than 40 ms between two blocks resets the tracker completely.
7. **Adoption into the show (`v2_tempo_adoption.h`, `applyV2Tempo` in `main.cpp`):** only while the show is running and audio is fresh (last block at most 250 ms old). Each new evaluation is considered exactly once. A value is rejected if it is outside 80–180 BPM, has a confidence below 30, has no onset or its last onset is older than 1500 ms; this also clears a pending candidate. A value that differs from the show tempo becomes a candidate; if the next evaluation is at most 4 BPM away, the rounded mean of both is adopted. This costs about 500 ms and filters a single outlier.
8. **Effect of an adoption:** `currentBpm` is set, the beat jumps to 1 of the next bar, the phase is placed at `last onset + beat interval`, and a `ClockSync` goes to the Nodes immediately. In between the clock runs free: each beat schedules the next as `now + beat interval`; `ClockSync` follows on beat 1 of every bar. Without a tempo change there is no phase correction against the music.
9. **No follow state:** `Searching`/`Locked`/`Holdover`, the music gate and the AudioFollower's 1 BPM step limit are gone. During silence, a break or missing audio the show keeps its last tempo; it does not fall back to 120 BPM. The adoption stage knows no tempo families: two matching evaluations at half or double tempo would be adopted.
10. **Other consumers:** The personality receives BPM, confidence and the last onset directly from the tracker (see B.6). The debug overlay (long front-button press) shows `show BPM/candidate` at the top next to the still visible beat dot; the confidence appears only in the USB telemetry.

#### Present in the source tree but not wired into production

`build_src_filter` compiles all files under `apps/mainframe/src/`; the following modules are therefore compiled but not called by any productive path:

- `beat_analyzer.*`, `audio_follower.*`: former tempo/follow path (ported V1 scanner from A.4). Only `tests/run_beat_analyzer.sh` and `tests/run_audio_follower.sh` remain.
- `bpm_tracker.*`: level-frame estimator from 10-01. Only its host test and the two trace reports remain.
- `spectral_onset_features.*`: twelve Goertzel bands (125 Hz to 6 kHz) per block; active only in the `mainframe-spectral-capture` environment. `V2TempoTracker` does not use these values.

User decision of 2026-11-14: these modules, their host tests and the fixtures stay as a comparison baseline until the new tempo path has been accepted on the device. They are removed afterwards.

#### Diagnostic environments

All of them extend `mainframe` and are intended for measurements only; flash the normal `mainframe` build again afterwards.

| Environment | Additional USB CDC output |
| --- | --- |
| `mainframe-beat-tracker` | once per second `BEAT_V2`: tracker BPM/confidence, raw value, direct onset tempo, periodicity, double-tempo candidate, `show_bpm`, number of evaluations, mean/maximum tracker runtime, ring drops, requeue failures, ring high-water mark, free PSRAM/internal RAM. |
| `mainframe-bpm-capture` | additionally per 16 ms block `BPM_TRACE3`: sequence, audio time, level, percussive level and four band levels (bass, low-mid, mid, high). Lines are skipped when the send buffer is full instead of blocking the capture. |
| `mainframe-spectral-capture` | additionally per block `SPECTRAL_TRACE`: sequence, audio time, level, percussive level and twelve band energies; the same skipping when the buffer is full. |
| `mainframe-v2-capture` | per block `V2_TRACE`: sequence, audio time and the onset strength of `V2TempoTracker` in permille; without `BEAT_V2` telemetry. |

Outdated comments on these environments in `platformio.ini` are tracked as BUG-004 in [bugs.md](../bugs.md).

#### Fixtures and host tests

| Script | Checks | Kind |
| --- | --- | --- |
| `sh tests/run_v2_tempo_tracker.sh` | Synthetic 16 kHz kick tracks at 100, 117, 126, 128, 160 and 180 BPM (each at most 2 BPM off, confidence ≥ 45), reset after a capture gap, 100→120 BPM transition without restart, replay of a synthetic onset trace via `feedOnsetTrace()`. | Assertions |
| `sh tests/run_v2_tempo_adoption.sh` | A single outlier does not change the show tempo, the same evaluation counts only once, low confidence clears the candidate. | Assertions |
| `sh tests/run_bpm_tracker.sh` | `BpmTracker` (not productive) with synthetic pulses at 100/120/160/180 BPM and silence, with the UB sanitizer. | Assertions |
| `sh tests/run_spectral_onset_features.sh` | Band separation and rise values of the twelve Goertzel bands. | Assertions |
| `sh tests/run_bpm_trace_report.sh` | Replays `tests/fixtures/bpm/atom-*.csv` (level and percussive level, 10-01) through `BpmTracker`. | Report, fails only on unreadable data |
| `sh tests/run_bpm_band_trace_report.sh` | Replays `tests/fixtures/bpm-bands/atom-*.csv` (four additional bands, 10-03) through `BpmTracker` in seven input combinations. | Report, fails only on unreadable data |

Each fixture set is five 12-second windows (750 frames) of real device features for hand-tapped references of 98, 100, 120, 160 and 180 BPM. Both reports evaluate the **non-productive** `BpmTracker` (the misleading name in the fixture README is BUG-005 in [bugs.md](../bugs.md)). They contain no PCM and no onset strength and therefore cannot evaluate `V2TempoTracker`. The first report counts, for example, 0 of 16 correct evaluations for `BpmTracker` at 98 BPM and 0 of 16 at 180 BPM (6 at half tempo); it is a finding about this discarded candidate, not a regression test. A `V2_TRACE` reference recorded from the device is not checked in: `feedOnsetTrace()` is tested only against a synthetically generated trace.

`sh tests/run_all.sh` runs all 15 host tests; they pass at state `2786c11`.

#### Measurements: what is proven and what is not

The following device values from 2026-09-30 come from the predecessor `TempoTracker` with lock, when it ran in parallel to the then productive path. The onset and correlation stages are unchanged in today's `V2TempoTracker`; the statements about locked values do not apply to it.

- Memory and runtime: 0 ring drops, 0 requeue failures, at most eight buffered frames, 8,103,323 bytes of free PSRAM and 257,372 bytes of free internal RAM with music playing. Tempo evaluation at most 2.51 ms per 250 ms update, on average about 0.33 ms per 16 ms block. The PCM ring occupies 8 KiB of PSRAM, the onset history about 2 KiB and its decimated correlation view 1 KiB; no additional task stack.
- Build sizes from 09-30: production build 54,780 bytes of static RAM and 1,059,417 bytes of flash. They were not re-measured for today's state.
- Reference tracks: 126 BPM consistently 125–127 BPM over 28 seconds. Hand-tapped 124 BPM track 123–128 BPM after the four-band change. At 180 BPM the raw value swung between 178/180 and 89/90; the lock at that time held 178. In the A/B test the 100 BPM lock held back raw-value outliers, while the real 120 BPM track produced no stable 120 candidate.

The local, unversioned device logs under `logs/` end for the `BEAT_V2` telemetry on 2026-10-03 at 17:14, before the switch of the clock source (`30ac9a7`, 17:30); their format with `legacy_bpm`/`v2_bpm` belongs to the `BpmTracker` diagnostic state. No log line contains the `show_bpm` field of today's format. The only log after the switch is a `V2_TRACE` recording of 348 lines (about 5.6 s), shorter than the 384 frames the tracker needs for its first evaluation.

ESP-DSP/FFT deliberately remains unintegrated: the official Apache 2.0 library is an ESP-IDF component, not a cleanly supported Arduino/PlatformIO dependency; a framework change is not planned for it.

#### Open hardware checks for the productive tempo path

1. Flashed state: according to the user (2026-11-14) the AtomS3R runs the last state from 2026-10-03, the normal `mainframe` build with the new tempo path and the `show BPM/candidate` debug display. `2786c11` changes no firmware source compared with it. This was not verified on the device; flash again before a measurement if in doubt.
2. Record reference tracks (at least 98, 100, 120, 124, 126, 160, 180 BPM) with `mainframe-beat-tracker`: `bpm`, `raw_bpm`, `confidence`, `show_bpm`, time to the first adoption, outliers. In particular the real 120 BPM track that was unstable on 09-30.
3. Half/double tempo on the device: does `show_bpm` jump between octaves at 160/180 BPM because the adoption stage knows no tempo families?
4. Behaviour during a break, silence, speech and a song change: the show keeps the last tempo without a holdover state; measure the time until a new tempo is adopted.
5. Phase position: distance of the beat dot and the Node beats from the music over several minutes at unchanged tempo, since the phase is reset only on a tempo adoption.
6. Effect of the beat/bar jump at every adoption on running Node scenes.
7. Re-measure the runtime and ring behaviour of today's tracker under display, IMU and radio load (`tracker_max_us`, `analysis_drops`, `backlog_max`).
8. Personality thresholds: `personality.cpp` still requires confidence ≥ 68 for the beat bonus and tempo trust, a value from the BeatAnalyzer scale. Whether `V2TempoTracker` reaches it with real music is untested (host test: 55–83 with synthetic kicks); tracked as BUG-003 in [bugs.md](../bugs.md).
9. Record a device `V2_TRACE` of sufficient length and check it in as a fixture if the productive tracker is to be regression-tested against real traces.

User decision of 2026-11-14: whether the adoption stage needs octave protection, a holdover state or phase correction is decided only after these measurements (items 3 to 5). Until then the current behaviour is neither confirmed as intended nor scheduled to change. This hardware acceptance comes after the end-to-end test with a V2 Node (B.5).

### Creature sound audition before the analysis (2026-09-28)

On the Atomic Voice Base, microphone and speaker share the same I2S path. The speaker therefore owns this path only before the analysis: after a restart the microphone stays off and a short custom handheld-adventure jingle plays. The animation shows three simple circles for sclera, iris and pupil that grow towards the viewer and bounce with decreasing height; all three use the same strong fourth-order ease-in scale factor and accelerate together only immediately before the flash. The simple iris is a deliberately desaturated night blue. Four offset sclera circles simulate the later light from the upper right without an expensive pixel gradient. A white full-screen flash fades out evenly over about 550 ms above the already rendered detailed eye view. When idle, the complete eye breathes between 95 and 100 percent of its size over five seconds; the existing sclera sprite is only scaled for this, and iris and pupil use the same factor. The earlier complete phrase audition was only a debug step and has been removed. Loneliness deliberately has no DOWN phrase.

The first short front-button click still starts the show and requests the analysis. If the audition is still running, it finishes completely first; VoiceBaseInput then starts exactly once. From then on the microphone stays active. There are no creature sounds, no I2S switch and no pause for VU, BPM, bass or sound analysis during operation.

The crossing policy is active after the start audition until the first front-button click: annoyance, depression, attention and loneliness have UP thresholds at 10, 30, 50, 70 and 90. Annoyance, depression and attention have the same five DOWN thresholds; loneliness stays silent when falling. Energy reacts only at 20 downwards and 80 upwards. Each mood type has its own cooldown of 10000 ms. While a PCM phrase is playing, other crossings are discarded and not caught up. With the first front-button click the PCM output ends before the microphone starts; after that the audio analysis runs without interruption.

Before the first analysis click, energy is additionally a visible creature battery instead of a music value: it starts at 100 and slowly falls to 0. At 20 a tired 10-second timer starts; without an IMU event the creature then falls asleep, at 0 immediately. During sleep the other moods keep running, threshold phrases are discarded and not caught up; the battery slowly recharges meanwhile. Until energy reaches 25, the sleep cannot be interrupted by events; after that the next IMU event wakes it as before. When the battery reaches 100, it also wakes up on its own. The display closes or opens two curved black lids for this; in the fully closed state a clearly readable eye line remains. During sleep the free speaker produces a quiet snore at deterministic intervals of 30–65 seconds. This substitute path ends with the analysis click and does not affect the music-driven energy path. Depression remains the same shared value: loneliness above 80 raises it slowly, attention above 80 lowers it at the same rate; after the analysis click, tempo and bass are added as music contributions.

Open hardware check (2026-11-15): mood phrases and the snore are synthesised synchronously in the main loop right before playback, up to 1.4 s of 44.1 kHz audio per phrase. How long this blocks the loop on the AtomS3R, and whether it delays radio, show clock or display, has not been measured.

### Radio separation implemented in code, hardware check open

V2 uses magic `0x44434632` (`DCF2`), V1 unchanged `0x4443464C` (`DCFL`). Payload version `13`, packet layouts and ESP-NOW channel `1` stay the same. All senders produce the shared V2 header; all four Node receive paths check it before staging a packet. The check function now lives in the hardware-independent `protocol.h` and is reused by the transport.

`sh tests/run_protocol_identity.sh` uses the real V2 packet constructors and the frozen V1 header contract to check the mutual rejection of scene, clock, hello and text, including text cancel for flashlight/RGB. It also checks valid V2 headers, wrong version/type/identity and unchanged packet sizes. These host tests prove the header contract, not packet reception or execution on hardware. V1 sources remain unchanged; both V2 device types must later be flashed with the new identity. The identity separates installations but is not authentication.

There are still no ACKs, sessions or scene-table checksums. The radio identity change did not address the scene problem; its status is described in A.3.

### Hardware state and limits of confirmation

According to the user, the **AtomS3R and Atomic Voice Base have arrived and are connected**. Target equipment: ESP32-S3-PICO-1-N8R8, 8 MB flash, 8 MB PSRAM, 128×128 display, BMI270, front button, microphone and speaker. No camera and no second ESP/controller in the Mainframe. U185/STHS34PF80 is still missing and is not needed for now.

The hardware check on the connected `/dev/cu.usbmodem101` was carried out. The flashed AtomS3R reports ESP32-S3-PICO-1 revision 0, 8,388,608 bytes of flash, 8,385,783 bytes of PSRAM, M5Unified board ID 18 and a 128×128 display; the 64 KiB PSRAM write/read test returns `PASS`. The Voice Base I²C probe confirms ACKs at `0x18` and `0x43`. This is an electrical bus confirmation, not proof of the codec or of audio capture. After flashing, the offline Mainframe reports `radio=READY`, `show=IDLE`, scene 0 and 120 BPM. No further serial Node was visible over USB; this says nothing about separately powered radio Nodes.

Before the port, the actual variants and pin assignment must be confirmed. The available manufacturer notes name GC9107 and ST7735 as display drivers; the revision of the connected device is open. The Voice Base needs I²S/codec instead of Unit Mini PDM. With a later TMOS there will be three separate I²C pin pairs on two hardware I²C controllers; the bus management remains open. Checked again while preparing the hardware test: the [AtomS3R documentation](https://docs.m5stack.com/en/core/AtomS3R) has named ST7735 instead of GC9107 since 2026-05-14; the [combination page](https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot) still names GC9107. The specific display revision remains to be checked on the device.

### Binding USB CDC debug path and renderer benchmark (2026-09-30)

The Mainframe uses USB CDC/JTAG at 115200 baud. The build flags enable USB CDC, and `setup()` additionally calls `Serial.begin(115200)` explicitly. This initialisation does **not** wait for a host: the device therefore stays fully bootable without a connected computer, while boot, audio and benchmark output are reliably available. Removing this explicit call again would make the serial diagnostics unreliable again.

The confirmed device port is currently `/dev/cu.usbmodem101`. In an interactive terminal the standard way is `pio device monitor -e mainframe --port /dev/cu.usbmodem101`; check with `pio device list` first, because the port can differ after a hardware change. Non-interactive environments without a TTY cannot start the PlatformIO monitor; there a simple reading USB CDC client is required instead of a firmware change.

For real renderer measurements, `mainframe-eye-procedural` is flashed. It visually matches the production renderer (48 facets) but only enables diagnostic 10-second reports for frame times, eye memory, audio/sensor cadence and send rejections. The normal `mainframe` build does not contain this benchmark output. Always run tests with audio analysis enabled and flash the normal `mainframe` build again after the measurement.

The emoji atlas is now decoded into a single 32×32 RGB565 cache (2 KiB PSRAM) only when the glyph changes and is blitted afterwards. The confirmed measurement showed four PNG decodes against 366 cache blits; recurring panel frames took about 13.9 ms instead of roughly 22.8 ms with the earlier PNG-per-frame path. The one-time cache fill is still visibly expensive at about 37 ms but is not in the repeat path.

### Product goal, order and next small step

The show works completely offline; internet and AI remain optional enhanced features. Audio and Node communication take priority over display and cloud. Eye/VU, local personality states and reaction texts according to B.2/B.3 are confirmed. IMU events and a first mood-dependent light directive have since been implemented (B.6); TinyML remains experimental, a local language model comes last and would be exclusively the "mouth".

First clarify project state, Git target and hardware, then agree on a manageable independent repo structure. As a lean starting point, `apps/mainframe`, `apps/node` and `shared/include` remain in the new repo and are ported selectively.

The radio separation is implemented in the code and in the flashed Mainframe. The first functional priority now remains the end-to-end test with a flashed V2 Node: Mainframe start, scene change, clock and mutual radio separation. After that audio/BPM, eye/VU, personality/text, further sensors and light reactions; TinyML later, local language model last.

Actual course up to 2026-11-14: eye/VU, personality, IMU gestures, text panel and the new tempo path were implemented before the end-to-end test was completed. The plan-versus-actual comparison is in B.5; the user decided on 2026-11-14 that the end-to-end test with a V2 Node comes next.

### Minimal S3R hardware test

This section describes the isolated bring-up test from 2026-09-25; at that time `pio run -e mainframe` built only `apps/mainframe/src/main.cpp`. Today the same environment compiles the complete Mainframe firmware under `apps/mainframe/src/`, and the isolated test no longer exists as a separate build. Configuration: Espressif32 6.13.0, Arduino, `esp32-s3-devkitc-1` as the base, 8 MB flash, QIO/octal PSRAM, USB CDC/JTAG, M5Unified 0.2.23 and M5GFX 0.2.30. Orientation: [manufacturer PlatformIO example](https://docs.m5stack.com/en/core/AtomS3R), with an explicit flash size and pinned library versions. The library detects the board; its reported ID is not independent proof of the hardware variant.

Completed and still open test sequence on the confirmed S3R:

1. **Done:** Serial output at 115200 confirms chip/revision, flash and PSRAM, board ID, 128×128 and `sample64k=PASS`.
2. **Open:** Check the display visually: colour fields and text must be fully visible without offset. Reported dimensions alone do not confirm the display driver.
3. **Open:** Press the front button: the Mainframe must start the show or advance the scene.
4. **Done:** I²C probe on Voice Base pins SDA38/SCL39 confirms ACKs at `0x18` and `0x43`. It uses the other controller than the internal display/IMU bus. ACK addresses prove only reachability, not yet codec identity or audio function.

The test at that time initialised no microphone capture, speaker output, radio or cloud connection and contained no local secrets. TMOS was left out. Items 2 and 3 are not recorded as a formal acceptance; display and front button have been used in all later steps since (iris VU working according to the user, show start via the button). I²S capture through the Voice Base is now the productive audio path; gain, level thresholds and capture quality are still not calibrated. Manufacturer pin reference: [AtomS3R + Voice Base](https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot).

## A. Verified actual state from the code

### A.1 Architecture and responsibilities

| Area | Actual state | Sources |
| --- | --- | --- |
| Build | Active Mainframe: AtomS3R, Espressif32 6.13.0, M5Unified 0.2.23 and M5GFX 0.2.30. Active Node: M5Atom with FastLED 3.10.3. The Node follows only headers that are actually included (`chain`), because FastLED's deep source scan pulls in optional, unused paths. FastLED 3.10.5 produced a whitish static output on the connected Node; the direct retest with 3.10.3 restored the demo and the Mainframe scenes. The former M5Atom controller is not compiled. | `platformio.ini` |
| Historical M5Atom controller (V1) | Main loop for microphone, audio follow, master clock, scenes, radio management and matrix UI. Not live at boot; a short button press activates the show. A local ignored read-only copy lives under `reference/m5atom_controller`; the historical V1 state remains canonical. | Historical V1 state |
| Audio (V1, historical) | Unit Mini PDM; level, onsets, tempo, confidence, VU and optional recording. | `pdm_microphone.{h,cpp}` |
| Clock follow (V1, historical) | Separate state machine that adopts the audio estimate into the master clock. | `audio_follow.{h,cpp}` |
| Node | One shared firmware for flashlight or RGB; type and role stored in Preferences. UV is only an enum, not a supported renderer. | `apps/node/src/main.cpp`, `node_output.*` |
| Scenes | Shared definitions for the Mainframe and the local Node demo; two scenes: the existing scene 1 and the scene "dynamisch" restored from the Git history. | `shared/include/scene_programs.h`, `apps/node/src/node_programs.h` |
| Transport | ESP-NOW broadcast on channel 1, shared C++ structs, own protocol v13 with five message types. | `shared/include/protocol.h`, `espnow_transport.h` |
| Text (V1, historical) | Matrix scroll text and a separate beat-bound Morse light output on Nodes. The V2 Node can still receive `NodeText`; the V2 Mainframe currently does not send it. | `text_playback.cpp`, `node_text_channel.h`, V1 `main.cpp`, `apps/node/src/main.cpp` |
| Cloud (V1, historical) | One FreeRTOS worker on core 0 for Wi-Fi/HTTPS; Cloudflare mediates AudD and text generation. | `api_client.cpp`, `wifi_manager.cpp`, `ai_mode.cpp`, `workers/decaflash/src/index.js` |

The rows marked "V1, historical" describe the former M5Atom controller. The local read-only copy is ignored and is never built or versioned. Sections A.4 (BPM scanner), A.6 (matrix VU/text) and A.7 (Wi-Fi/cloud) also analyse this V1 state; they are marked as such.

Active S3R Mainframe at state `2786c11` (`apps/mainframe/src/`):

| Area | Actual state | Sources |
| --- | --- | --- |
| Audio capture | Voice Base via M5Unified/I²S, 16 kHz, 256-sample blocks, sample time base and PCM ring in PSRAM. Starts only after the first front-button click, because microphone and speaker share the same I²S path. | `voice_base_input.*` |
| Tempo and show clock | PCM tempo tracker with four-band spectral flux and autocorrelation; the show adopts a tempo after two matching evaluations. Details and open hardware checks in section 0. | `v2_tempo_tracker.*`, `v2_tempo_adoption.h`, `main.cpp` |
| VU and audio features | Iris VU from the block level; silence detection and broadband bass share for the mood. | `iris_vu.h`, `audio_mood_features.*` |
| Personality | Five local mood values, creature battery and sleep before the analysis start, and the Node light directive derived from them. | `personality.*`, `main.cpp` |
| Motion | BMI270 via M5Unified, polled at most every 20 ms; classifier for Tap, MultiTap, Impact, Shake, Rotate, Tilt and Move. | `motion_events.*` |
| Display | Procedural low-poly eye with iris VU, lids, boot animation, debug overlay and text panel with its own pixel font and emoji atlas. | `eye_renderer.*`, `message_panel.*`, `panel_glyphs.*`, `panel_text_sprites.h` (generated) |
| Speaker | Start jingle, mood phrases and snoring through the Voice Base speaker, only before the analysis start. | `sound_engine.*`, `sound_personality.*` |
| Radio | `MainframeHello`, `SceneSelect`, `ClockSync` and `NodeVisualState` as ESP-NOW broadcast. | `main.cpp`, `shared/include` |
| Not wired in | `beat_analyzer.*`, `audio_follower.*`, `bpm_tracker.*` are compiled but not called; `spectral_onset_features.*` only in the diagnostic environment. | Section 0 |

**Not** present in the V2 Mainframe are Wi-Fi, cloud connection, audio recording for AudD, `NodeText` sending, TMOS evaluation, TinyML and a local language model. Frequency-band analysis exists only as a lightweight filter bank in the tempo tracker and as a bass indicator for the mood, not as an FFT or as a bass/mid/high output for light control.

The Mainframe works cooperatively in the Arduino loop with `delay(5)` per pass. Capture is advanced in the M5Unified callback; the evaluation of the buffered blocks runs in the main loop and not in a separate, fixed-rate analysis task. The V2 Mainframe starts no further task of its own.

### A.2 Mainframe ↔ Node: data model and life cycle

The Mainframe sends a scene index, no pixel streams and no role commands. The shared scene definition lives in the Mainframe and the Nodes; each Node derives its local light output from the scene index and its stored role.

| Message | Direction | Content/function |
| --- | --- | --- |
| `SceneSelect` | Mainframe → broadcast | Index of the shared scene |
| `ClockSync` | Mainframe → broadcast | BPM, beats per bar, beat in bar, bar number |
| `MainframeHello` | Mainframe → broadcast | one-time greeting at Mainframe boot; triggers the triple Node blink and `MainframeWaiting` |
| `NodeText` | Mainframe → broadcast | Node type, cancel flag, 48-byte text field; currently not sent by the V2 Mainframe |
| `NodeVisualState` | Mainframe → broadcast | neutral render directive (RGB mode, brightness, colour, overlay, flash behaviour); on change and every three seconds, see B.6 |

Header check: V2 magic `0x44434632` (V1: `0x4443464C`), version `13`, message type; reception also checks `sizeof(...)`. The structs are sent directly as a memory image. There is no explicit portable serialisation, no sender binding on the Node, no encryption, no pairing and no application ACKs for scenes.

Nodes send no status, heartbeat or clock replies. The Mainframe therefore deliberately does not know which Nodes have applied a scene. A successful `esp_now_send()` return still does not prove that a particular Node received the message.

Mainframe start and sending:

1. Setup initialises display, eye, start sound and ESP-NOW; the show is not running yet. The microphone starts only after the first click and the end of the start sound.
2. When the radio is ready it sends `MainframeHello` exactly once. It is only the greeting for Nodes that are already running, not a permission for scene or clock. There is no session ID and no repeated hello.
3. A short button press starts at 120 BPM and four beats per bar and requests the audio analysis. Further short presses select the next scene.
4. At start and at every scene selection the same `SceneSelect` message is sent once. During the show the same broadcast follows every 30 seconds.
5. Clock sync happens at show start, on beat 1 of every bar and additionally immediately on every tempo adoption from the tempo tracker.

Node states:

- `Demo`: local scene from boot, own 120 BPM clock.
- `MainframeWaiting`: only after a received hello; `serviceClock()` executes no beats until the first clock sync.
- `MainframeRunning`: after a valid clock sync, even without a previous hello; between syncs the local clock keeps running.

There is currently no timeout that switches a running Node back to demo after radio loss. It keeps running with the last tempo and command. A waiting Node, by contrast, stays waiting until the next clock sync. This is not an explicit, time-controlled Node holdover state.

### A.3 Scenes, renderers and concrete problem areas

`kScenes` now contains two scenes. Scene 1 is the previous state. Scene 2 is called "dynamisch" and was restored from the earlier shared scene model (commit `07f4003`). Its RGB colours, levels and bar patterns match the definition at that time. The old `Breathe` renderer no longer exists in the current model; the wash therefore uses its present beat-bound counterpart `BarWave`. The earlier flash model with bursts of 2 and 3 is implemented by a corresponding variation profile of Drive, Double and Quad motifs.

Scene 1:

- Flash: deterministic variation profile, window of eight bars, seed 17; weights Drive/Heavy/Double/Quad/Riser = 30/14/24/20/12. The five motifs are not five currently selectable scenes.
- Wash: blue/ice blue, `BarWave` over four bars.
- Pulse: blue beat pulse.
- Accent: red heartbeat over four beats.
- Flicker: beat-related alternating runner.

RGB is output through two GPIOs (26/32) from the same 15-pixel buffer, i.e. mirrored outputs. In addition the renderer produces time-based activity, shadows, colour cast and dark spots. This `activity` is **not an audio measurement**: it comes from `millis()`, hash/noise functions and slow pendulum curves. These modulations are also not tied to a shared Mainframe time; different boot times can produce differences.

**Status of the scene problem (2026-11-14):** The problem was reported for the Brain/Node communication of the old project. A first fix there (`3841e9e`, 2026-04-18, "harden brain scene handoff and node ESP-NOW recovery") was reverted the same day (`f9cabdf`). On 2026-09-23 the old project replaced role-specific command traffic, revisions, sessions, node status, heartbeats and ACKs with the single `SceneSelect` message (`26f4500`). V2 was copied from that state on 2026-09-25; since then the Node's scene selection has changed only by the Brain→Mainframe renaming. According to the user the problem has not occurred for a long time and is fixed. No log, measurement or recorded hardware test in this repository ties the fix to a specific cause; the simplification of 2026-09-23 is the only change to the scene path in that period. Listed as closed BUG-006 in [bugs.md](../bugs.md).

The taken-over communication state contains the following simplifications and remaining limits:

1. **One message per scene.** Foreign role packets can no longer overwrite anything, because role commands are no longer sent over radio.
2. **Idempotent selection.** A Node compares the command derived from the scene index with its active command. A repetition of the same scene does not change the running output.
3. **Single, periodic scene broadcast.** The Mainframe sends `SceneSelect` once at start or change and repeats the current state every 30 seconds.
4. **Hello is only the greeting.** A Node without a hello first runs its local demo but adopts the next scene and clock immediately. After a received hello it blinks three times and waits for the first Mainframe messages.
5. **No global scene barrier.** Nodes adopt a scene on arrival and use their running beat position. A briefly outdated scene on a single Node is acceptable; the installation does not wait for other Nodes.
6. **No Node back-channel.** Status and clock replies have been removed. The Mainframe gets no reception confirmation, but in return there is no simultaneous reply traffic from several Nodes.
7. **Blocking still increases reception and clock latency.** Tracked as BUG-002 in [bugs.md](../bugs.md).
8. **Clock arrival is not a precise beat time.** See A.5 for details. Drift, processing time and jumps can look like scene errors.
9. **Text can deliberately displace the scene.** Morse temporarily replaces the regular Node output. Text packets are filtered by Node type.

Sources: historical V1 controller (`sendSceneSelect`, `selectNextScene`, loop), `apps/node/src/main.cpp` (`stageIncomingSceneSelect`, `processPendingRadio`, `processPendingSceneSelectMessage`, `enterMainframeWaitingMode`), Node renderers. The actual cause of a specific hardware incident remains open without symptom/log correlation; in particular, no specific packet-loss rate has been proven.

### A.4 BPM scanner: complete flow

**Classification (2026-11-14):** This section analyses the V1 scanner (`PdmMicrophone`, `audio_follow`). V2 had ported it to the Voice Base as `beat_analyzer.*` and `audio_follower.*` and used it productively until 2026-10-03. Since commit `30ac9a7` it drives neither the clock nor the mood; the code sits unwired in the source tree and is covered by its host tests. The productive tempo path is described in section 0 ("PCM tempo tracker"). The following formulas are kept as an analysis of the predecessor and as a basis for comparison; whether the ported code matches them line by line was not re-checked in this reconciliation.

The V1 scanner has three different tempo values:

- `PdmMicrophone::detectedBpm()`: chosen tempo representative from the onset analysis.
- `PdmMicrophone::clockBpm()`: clock target chosen from it with a half/double-tempo heuristic.
- `currentBpm` in the Mainframe: the show tempo actually running after `audio_follow`.

`updateClockFromAudio()` explicitly passes `microphone.clockBpm()` to the field `audio_follow::Input::detectedBpm`. The field name is therefore slightly misleading.

#### 1. Capture and preprocessing

Source: `pdm_microphone.cpp`, `begin`, `update`, `accumulateSamples`.

- PDM RX on `I2S_NUM_0`, DATA GPIO26, CLK GPIO32, 16 kHz, 16 bit, mono left, DMA 8 × 128, PDM downsampling `I2S_PDM_DSR_16S`.
- Per loop call up to four non-waiting `i2s_read()` calls of 256 samples each: at most 1024 samples or 64 ms of audio data per call. This is **not a fixed 64 ms analysis interval**.
- Per sample: `dc += (sample - dc) >> 6`; then `centered = sample - dc`. This is a continuous DC estimator with a high-pass effect, not a bass-band filter.
- `blockLevel` is the mean of `abs(centered)` over all samples read in the current update, not RMS. The largest absolute value is also recorded.
- A further sample envelope with weight 1/16 is carried along but is not the basis of the tempo score.
- Per update with new samples exactly one analysis runs, with timestamp `millis()` after reading. The onset time is therefore processing time, not the sample-accurate position of a transient in the DMA buffer.

#### 2. Energy envelopes and music gate

Source: `updateAnalysis`. On zero initialisation the current block level is taken directly; after that integer filters apply:

| Quantity | Rising input | Falling input |
| --- | --- | --- |
| fast analysis envelope F | `(2F + block)/3` | `(5F + block)/6` |
| slow analysis envelope S | `(15S + block)/16` | `(31S + block)/32` |
| analysis floor N | `(255N + S)/256` | `(7N + S)/8` |

`musicPresent` switches on at `S > N + 22` and stays on while `S > N + 12`. This is level hysteresis, not a distinction between speech, music and noise.

Transient `T = max(F-S, 0)`. Onset threshold `H = 18 + max(S-N, 0)/3`.

#### 3. Onsets and history

An onset is accepted if:

- the music gate is active;
- at least 300 ms have passed since the last onset (or there is none yet);
- `T > H`;
- the block peak is at most 18 times the block level (at block level 0 this peak check is skipped).

With an existing tempo and confidence ≥ 50, intervals that are too early are additionally rejected: less than 88 % of the expected beat interval, unless they lie in the 45–65 % range. The absolute 300 ms lockout still applies and limits this half-interval exception.

Accepted onsets receive the current processing timestamp. The last eight onset times and the last eight valid intervals are stored. Intervals of 300–1200 ms trigger the tempo estimate; it starts only with at least three onset timestamps. On a gap of more than 1200 ms only the new timestamp and confidence 0 remain; the interval history is cleared, tempo buckets are not cleared at this point.

After more than 2200 ms without an onset, confidence is set to 0. Only if the music gate is off at the same time are BPM, clock BPM, histories and tempo buckets reset as well. With a persisting level, old BPM values can therefore remain with confidence 0.

#### 4. Pulse history as an additional periodicity signal

On every analysis update, `min(255, max(T-H,0)/4)` is written into a 64-entry ring; without the music gate, 0 is written.

The estimated frame interval starts at 64 ms. Only actually measured update intervals between 20 and 120 ms are blended in with weight 1/8. Nevertheless a pulse value is stored for **every** update. With more frequent updates below 20 ms, the time assumption of the history therefore does not necessarily match its actual sampling. The 64 entries are not a guaranteed time window.

For each candidate tempo a one-beat lag rounded to whole frames is determined. From eight history entries and a valid lag (at least 2, smaller than the history length) the code sums `min(pulse[i], pulse[i-lag])` for non-zero pairs. This is a simple delayed similarity, not an FFT and not a classic product autocorrelation.

#### 5. Candidate search and scores

Source: `updateTempoEstimate`. All **101 integer BPM values from 80 to 180** are evaluated. The following formulas describe the weights actually used; divisions are integer divisions.

For each pair of the at most eight onsets:

1. Distance `d` must be positive and at most 2200 ms.
2. Nearest beat count `m = floor((d*bpm + 30000)/60000)`; allowed 1–8.
3. Expected distance `e = floor((60000*m + bpm/2)/bpm)`.
4. Tolerance `t = max(24 ms, floor(e*9/100))`.
5. With error `a = abs(d-e) <= t`: score increment `(1+newerIndex)*(9-m)*8 + t-a`.

This pair score is called I here. The additional primary score Q counts double the score for `m=1`, a quarter for `m=2` and nothing otherwise. One-beat hits are counted separately.

The up to eight direct onset intervals yield score D: comparison with `floor(60000/bpm)`, tolerance `max(24 ms, 8 % of this interval)`, increment `(2+intervalIndex)*24 + 2*(t-a)`.

With pulse score P, a memory score M is updated per BPM:

`M = M - (M >> 3) + I + Q + floor(P/2)`.

This decay happens per tempo estimate, not per second. A combined candidate score is:

`C = M + 2D + 6Q + 6P + 30 % M_of_the_left_neighbour + 30 % M_of_the_right_neighbour`.

Only candidates with a current pair hit take part in the selection. At a distance ≤ 2 from the previously detected BPM, `(3-distance)*18*6` points are added; otherwise, at confidence ≥ 60 and the same tempo family, `18*4`.

#### 6. Half/double-tempo families and BPM selection

`canonicalTempoFamilyBpm()` considers BPM, integer BPM/2 and BPM×2 within 80–180. Preference goes first to the smallest distance from the 90–170 window, then to the smallest distance from its preferred centre 130.

The combined scores are aggregated under this canonical BPM. The previous family receives 216 bonus points in the ranking. From a previous confidence of 60 it is retained if the new family score reaches less than 112 % of its existing support.

Within the winning family an actual tempo representative is chosen:

`R = 2I + 8Q + D + 8P + floor(M/4) + 30 % I_of_both_neighbours`.

The canonical representative receives 180 extra points. Other representatives lose `abs(bpm-canonical)*18*5` points; this penalty is quartered if their primary score or primary hit count is stronger than that of the family anchor. For the same family as the previous BPM there are further continuity bonuses: up to distance 2 `(3-distance)*18*8`, otherwise up to distance 6 `(7-distance)*18*2`.

Important: `inSameTempoFamily()` does not mean "nearby BPM" in general. It checks for equal canonical values or an approximate doubling of the canonical values with at most 4 BPM error. Some supposed smoothing therefore takes effect only under this narrow condition.

#### 7. Confidence and two analysis BPM outputs

For the chosen representative:

- Precision = `max(0, 100 - floor(mean_pair_error*320/beat_interval))`.
- Coverage = `min(100, floor(pair_hits*100/8))`.
- Primary coverage = `min(100, floor(one_beat_pair_hits*100/4))`.
- Separation = relative distance of the best from the second-best family score in percent; 0 if there is no positive distance.
- Continuity = initially 60, 100 for an unchanged family, otherwise `max(0,100-6*family_distance)`.

`confidence = floor((2*precision + coverage + primary_coverage + separation + continuity)/6)`.

This is a heuristic score of 0–100, not a calibrated probability. Without a valid winner the previous value drops by 6. The declared constant `kAnalysisTempoMinimumMatches=3` is not used: what is actually required at first are three timestamps; the later selection does not explicitly check for three matching pairs. Several other declared family/double-interval constants are currently unused as well.

`detectedBpm` takes the chosen representative, with additional family hysteresis: at a difference of at least 6 BPM and existing support, the new representative must reach at least 125 % of the old support (`6Q+D+P`). With the same family relation and distance ≤ 2 the values are averaged.

`clockBpm` can instead choose the faster canonical family value if primary/interval/pulse support exists there: at least one primary hit or evidence `6Q+D+6P` of at least 35 % of the best representative score. The switch to a slower subdivision within the same family is delayed. Despite the constant "4 hits", the current control flow holds the old tempo at the first candidate and while counting up to 4; only on the **fifth** matching pass is the slower candidate adopted. Small changes are again averaged only with the same family relation.

#### 8. Audio follow: adoption into the show clock

Source: `audio_follow.cpp`; runs only when `mainframeLive`.

- States `Searching`, `Locked`, `Holdover`.
- A fresh onset needs the music gate, a non-zero clock target BPM and a previously unseen onset timestamp.
- For the first lock: three fresh candidates with confidence ≥ 68, BPM within ±4 of the running averaged candidate.
- At lock the tempo is adopted directly, `nextBeatAtMs` is set to the onset timestamp and a clock sync is requested. The bar counter is not re-determined musically.
- In lock, confidence ≥ 62 is required. Onset intervals must normally be 88–112 % or 176–224 % of the running beat interval. When resuming from holdover this interval check is skipped once.
- Two accepted updates with exactly the same differing target tempo allow a change of at most **1 BPM**; after that the count starts again.
- Phase error = distance of the onset from the nearer of the previous/next planned beat time. Small errors shift the next beat by error/3.
- Large errors: threshold `max(20 ms, min(90 ms, beat interval/5))`. First correct error/2; after two large errors with the same sign correct the whole deviation and request a sync.
- After about four beat intervals without an accepted onset, holdover is entered under the respective check conditions; after more than 4 seconds at the latest the follow state is reset. The running master clock is **not** reset to 120 BPM and not stopped.

#### 9. Master clock and limits of the musical statement

Source: historical V1 controller, `setClockBpm`, `updateClockFromAudio`, `onBeat`, loop.

Start at 120 BPM, allowed clock limits 60–180, interval `floor(60000/BPM)` milliseconds. In the loop, due beats are caught up with a `while` loop. Four beats make a bar. The syncs actually sent are created only in `onBeat()`.

The scanner detects **no musical downbeat**, no time signature and no song phrase. "Beat 1" is the installation's counter state. Onsets can arise from rhythmic accents or noise; they are not reliably detected bass drums. There is no bass/mid/high separation and no genre, break or drop model. During long loop pauses several caught-up beats can be processed almost simultaneously.

### A.5 The Node clock is currently not a soft sync

`apps/node/src/main.cpp::applyClockSync()`:

1. A valid clock sync with non-zero BPM is adopted even in demo mode and without a previous hello.
2. BPM and bar length are adopted, the state becomes `MainframeRunning`.
3. If the same beat/bar was already rendered in the last 250 ms, no further beat is output, but `nextBeatAtMs = now + beatIntervalMs` is set.
4. Otherwise beat/bar are adopted, `onBeat()` is executed immediately and afterwards `nextBeatAtMs = now + beatIntervalMs` is set as well.

`now` is the processing time in the Node main loop; the packet contains no sender timestamp and the callback stores no reception timestamp. With blocking flash output the moment can already be in the past after the output. There is no PLL controller and no radio telemetry to the Mainframe. The README statement about a gentle Node phase trim does not match this code.

Missing plausibility limits for received clock BPM are tracked as BUG-001 in [bugs.md](../bugs.md).

### A.6 VU meter and matrix rendering

**Classification:** V1 state of the 5×5 matrix. The V2 implementation as iris VU and text panel is described in section 0 and B.6. The last paragraph on Node text still applies to the V2 Node.

The VU level calculation is separate from the tempo analysis (`updateMeterLevel`): fast envelope with attack 1/4 and release 1/8, slow with 1/8 and 1/16. Adaptive noise floor: downwards 1/8, upwards 1/128. The signal ceiling follows upwards with 1/4 and downwards with 1/16.

Gate = floor + 6 + 10; display start a further 18 above. The base level is scaled to at most 20 units, with a level span of at least 180 and otherwise an additional headroom of 140. Units ≤1 are suppressed. A transient share above 18 yields up to five additional units over a span of 140. Total maximum 25, release two units per analysis update; after four quiet updates it is set to 0. The computed variable `meterDisplayLevel_` is not used for this final scaling.

`matrix_meter.cpp` draws at most every 40 ms, i.e. nominally at most 25 updates/s. Of 25 LEDs, 23 remain for the meter; pixel 0 is the Wi-Fi status and pixel 4 the beat dot. 24/25 measured VU units are capped to 23 visible meter pixels.

The meter is not a classic bar display: a random pixel order decides the active faces and changes every 220 ms. Per draw, local pixel values rise by 18 or fall by 14. Their values run through colour gradients:

- normal: black → blue `(0,105,255)` → pink `(255,40,150)` → violet `(235,0,255)`;
- recording/AI processing: black → green → light green → very light green.

This level, activation, transition and colour idea is transferable to iris polygons; `M5.dis.drawpix()` and the 5×5 framebuffer itself are hardware-bound.

UI priority in the base render path: text, scene display/temporary lock, AI animation, then VU. Text blocks the beat/Wi-Fi overlay. The beat dot lights for 140 ms: **yellow on beat 1**, white on other beats, red as the audio sync correction marker. This differs from the README description "red on beat 1".

Text: 96-byte buffer, normalised upper-case letters/umlauts, 5×5 glyphs, intro flash 90 ms, pause 1000 ms, then one scroll column every 130 ms. There is no eye yet and no text overlay compositor.

Node text: at most 47 payload bytes, A–Z/0–9 as a Morse-like light sequence, start on the next local beat. The unit is half a beat, dot 1 unit, dash 2, symbol gap 1, letter gap 2, word gap 4. This deliberately deviates from the classic Morse timing ratio and is its own beat-bound scheduling function. Matrix text and Node Morse do not necessarily end at the same time.

### A.7 ESP-NOW, Wi-Fi and cloud

**Classification:** The first paragraph also applies to V2. Wi-Fi manager, cloud task and recording/AI path describe the V1 controller; the V2 Mainframe currently contains none of this. The Worker still exists as source code under `workers/decaflash`.

ESP-NOW uses Wi-Fi STA, the fixed offline channel 1 and an unencrypted broadcast peer with `peer.channel=0` (= current local radio channel). The Nodes do not connect to an access point. In V2, `shared/include/espnow_transport.h` still contains `recoverEspNow()`, but neither the Mainframe nor the Node firmware calls it (checked 2026-11-15).

The Wi-Fi manager scans synchronously, tries configured SSIDs in list priority and waits up to 15 seconds per connection attempt, in 250 ms steps. It does not filter for channel 1. Manual `wifi scan`/`wifi connect` run from the shell in the Mainframe main loop and can block audio pickup, clock and UI.

Cloud work runs in a separate task on core 0, priority 1, stack argument 12288. There is one job slot. Every scheduled cloud request activates `managedWifiSessionActive`; `radioPauseActive()` prevents scene, clock and Node text sends meanwhile. This applies **even with an AP on channel 1**. After completion Wi-Fi is disconnected and ESP-NOW including the broadcast peer is restored; the recovery attempt is limited to an interval of one second.

With manually connected Wi-Fi outside channel 1, `serviceEspNowState()` explicitly blocks ESP-NOW. More RAM or an S3R port do not remove the channel problem. Nodes that are already running keep playing autonomously but receive no new scenes or clock corrections during the pause.

Current recording/AI path:

- A long Mainframe button press from 1200 ms toggles AI listening.
- After 1500 ms of continuous music gate a recording is prepared. The trigger prefers BPM present, confidence ≥18 and an onset at most 180 ms old; after a further 1200 ms a fallback without these beat conditions applies.
- Default and maximum are already **12 seconds** at 16 kHz. The recording is µ-law, one byte per sample, target size 192000 bytes. With a RAM shortage it is shortened in 250 ms steps down to at least one second. No explicit PSRAM allocation.
- Onset/VU analysis keeps running during the recording. The whole clip is in RAM before the upload; multipart streaming avoids a second complete upload buffer but does not stream directly from the running microphone to the network.
- The Mainframe sends µ-law to `/api/audd`; the Worker decodes to 16-bit WAV and queries AudD. On a match it additionally generates a short German text from title/artist. No BPM back-channel from AudD.
- Successful processing: cooldown 420 seconds; failure 60 seconds; a Wi-Fi failure disables AI mode.
- IMA ADPCM code exists and is supported on the server side, but the active Mainframe recording path uses µ-law.
- The Worker also offers `/api/chattie`, health and protected debug WAV/JSON endpoints. The last decoded recording and its metadata are overwritten in two KV keys.
- The HTTPS client currently uses `setInsecure()`; certificate verification is therefore not set up. Local secrets are gitignored, templates are versioned. The normal Worker deploy is the Git/Cloudflare integration.

The notes about exclusively short clips and still missing 8–12 s support are therefore outdated. What is not done is a robust quality decision on whether a clip is suitable for recognition; the fallback keeps the gate coarse.

### A.8 Deviations in existing documentation

Before the consolidation, the copied README partly described historical details: only flashlight Nodes, five local programs, old raw-level serial reports and earlier UI colours. The taken-over code supports flashlight and RGB roles as well as two jointly compiled scenes. The README statements on scene index, one-time hello, 30-second repetition and missing sessions/ACKs match the taken-over state. Earlier statements of this context document about separate role packets, only one scene or a hello requirement for clock sync are outdated. The Node resync happens directly as described in A.5.

`docs/archive/v1-outline.md` is historical: statements such as "no radio/microphone/RGB" are not today's actual state. The Node boot message "Node V2" does not mean that the Mainframe v2 planned here already exists. The V1 outline is now explicitly marked as historical; the README was consolidated into a short V2 entry point.

## B. Planned Mainframe v2 – user requirements, not yet implemented

**Implementation status (2026-11-14):** The heading dates from the planning phase. B.1–B.4 remain the user requirements; below each section it says what has been implemented in code. B.5 compares the planned order with the actual course. B.6 describes implemented code.

### B.1 Basic principle and hardware

The actual light show must work completely without an access point, internet or AI services. Wi-Fi/internet provide enhanced features only. No additional ESP/controller in the Mainframe; the existing light Nodes remain part of the installation. The display has lower priority than audio and Node communication.

Target hardware:

- M5Stack AtomS3R, ESP32-S3-PICO-1-N8R8, 8 MB flash, 8 MB PSRAM.
- 128×128 colour display, BMI270 IMU, programmable front button.
- Atomic Voice Base with MEMS microphone and speaker.
- Later: external M5Stack U185 with STHS34PF80 for presence detection; not yet available and not needed for now.
- No camera.

**Status:** AtomS3R and Voice Base are in operation. The Unit TMOS PIR (U185) is available since 2026-11-15; the Mainframe reads it on the Grove port (OpenSpec change `add-presence-sensor-debug`, measured on the device 2026-11-15) and, since `add-presence-mood`, uses it for the attention base, loneliness and presence events with texts; light is not affected. The presence rules are not yet tested on the device. The show runs without Wi-Fi because the V2 Mainframe does not contain Wi-Fi yet.

### B.2 Display and interaction

Normal view: animated low-poly eye in PS1 style on black, few flat-shaded polygons, a deliberately low/choppy frame rate is allowed. Idle behaviour includes blinking and gaze movements. The iris is the VU meter and takes over the existing level/colour logic as a visual principle.

Personality initially expresses itself through text only. Text appears on reactions; an overlay above the still running eye is one possible presentation. Long text may temporarily replace the eye. This does not yet imply a decision on graphics library, polygon count or specific FPS.

Inputs: microphone/music, IMU, front button, TMOS presence; optionally internet. The later IMU evaluation is meant to distinguish bass/vibration, light tapping, double tapping, bump/hit and shaking/moving. A TinyML model is an option for this, not a fixed prerequisite.

**Status:** Eye, iris VU, gaze movement, mood and sleep lids and the text panel above the still running eye are implemented (section 0, B.6). Idle blinking is not implemented; it is noted in `ideas.md`. Microphone, IMU and front button are connected as inputs. TMOS drives the attention base, loneliness and three presence events (see B.6, presence); internet is not connected. The IMU distinguishes Tap, MultiTap, Impact, Shake, Rotate, Tilt and Move heuristically, without TinyML. Bass/vibration is not detected as a separate IMU class. Discrimination on the enclosure has not been accepted.

### B.3 Personality

The personality is driven by local states and rules, **not by a language model**. Intended states, roughly 0–100:

- `energy`
- `annoyance`
- `attention`
- `loneliness`
- `depression` as a deliberately humorous goth-party state

Events change values; some of the values decay over time. Repeated tapping, for example, raises annoyance, missing presence raises loneliness, interaction raises attention; annoyance slowly sinks again.

Initially `state + event → local text packet`. Later optionally `state + event → small local language model → short text`. The model would be exclusively the "mouth"; state evolution and light decisions remain locally deterministic.

States may later influence light, e.g. very high annoyance → temporarily red. Exact thresholds, durations, priorities and the return to the running scene are not specified yet. A white flashlight Node physically cannot output red; this specific colour effect concerns RGB Nodes. For flash, suitable brightness/rhythm reactions have to be decided separately.

**Status:** The five states, their event and time rules, local text variants per motion type and goth verses are implemented (B.6). In addition to text there are creature sounds before the analysis start (section 0). Two rules exist as light effects: annoyance ≥ 90 sends solid red and flash permanently on, loneliness ≥ 90 dims to 15 % and switches flash off. No language model.

### B.4 AudioAnalyzer v2

First understand, measure and stably take over the existing scanner. Possible later outputs: BPM, beat, beat confidence, bass/mid/high energy, total energy, rhythmic regularity, break/drop/transition and possibly coarse music classes. These extensions are not implemented already and not all bindingly decided. TinyML audio classification remains a later option.

**Status:** The existing scanner was first ported, then replaced as the clock source by `V2TempoTracker` on 2026-10-03 (section 0); its hardware acceptance is pending. Available are BPM, confidence, last onset, silence detection and a smoothed bass share. Not available are bass/mid/high outputs for light control, total energy as a separate output, a regularity measure, break/drop/transition and music classes.

### B.5 Binding order of the intended work

The left column is the plan from 2026-09-22 to 2026-09-25, unchanged. The right column is the state on 2026-11-14 as it can be read from the code, the Git history and this document.

| No. | Planned step | Actual state |
| ---: | --- | --- |
| 1 | Check the independent project state, Git target and actual hardware; agree on the repo structure and prepare a minimal hardware test. | Done. The visual display and button check of the bring-up test are not formally recorded. |
| 2 | Stabilise Mainframe/Node communication with mandatory V2 radio separation. | Partly. Radio identity in code, host tests pass, Mainframe flashed. According to A.1 a connected Node has displayed Mainframe scenes. Mutual V1/V2 rejection on hardware and a recorded end-to-end test are pending. |
| 3 | Reproduce the scene problem and narrow it down with measurements; do not anticipate a cause. | Closed. According to the user (2026-11-14) the problem no longer occurs (A.3). It was never reproduced or measured in this repository. |
| 4 | Build the AtomS3R port needed for that in a targeted way. | Done; well beyond what step 3 needed. |
| 5 | Take over audio/BPM stably. | In progress, deviating from the plan. The scanner was ported and then replaced as the clock source by a new PCM tracker. Hardware acceptance of the new path open (section 0). |
| 6 | Low-poly eye with VU. | Implemented and flashed; level calibration and measurement under load open. |
| 7 | Personality state system and simple local text packets. | Implemented (B.6); calibration values provisional. |
| 8 | IMU/button/TMOS. | IMU and button implemented; gesture acceptance on the enclosure open. TMOS measured on the device; since `add-presence-mood` it drives attention, loneliness and presence events with texts; device test of the presence rules open. |
| 9 | Connect personality with the light show. | First rule implemented (`NodeVisualState`, B.6); test with a flashed V2 Node open. |
| 10 | TinyML. | Not started. |
| 11 | Experimental local language model at the very end. | Not started. |

Deviation from the plan: steps 6 to 9 were implemented before 2 and 5 were completed. In addition, unplanned parts were built: creature sounds with battery and sleep cycle, the text panel with emoji atlas and the diagnostic environments.

**User decision of 2026-11-14:** The next step is to complete step 2: the end-to-end test with a flashed V2 Node (start, scene change, clock, mutual V1/V2 rejection). Step 3 is closed, see the table. The hardware acceptance of step 5 follows; it needs a running Node anyway. No further decision on the order of the later steps has been made.

### B.6 Local mood parameters and debug overlay – decision of 2026-09-26

The user made the first step concrete: **no reaction texts**, initially only five local parameters; a later effect on Nodes is a follow-up step. The missing presence sensor stays out of scope. A long press of the front button shows the five parameters as a debug overlay, another long press hides it. This section replaces the previous draft including text selection. Subsequent user clarification: loneliness rises constantly and falls through engagement; tapping/multi-tapping, bump, turning/tilting and shaking must be available as separate events. The missing presence sensor does not prevent this explicitly requested interaction rule.

Implemented in `personality.{h,cpp}` and `motion_events.{h,cpp}`, wired up in `main.cpp`: a hardware-independent, deterministic model with values 0–100, internally milli-points. No persistence, no dynamic allocation, no task of its own and no language model. `depression` remains the device's humorous goth-party state.

| Parameter | Restart | Provisional local rule |
| --- | --- | --- |
| energy | 30 | BPM is the main factor: energy uses the current estimate of `V2TempoTracker` (not the show tempo) as soon as audio is fresh and not silent and a value between 60 and 200 BPM is available. The base is about 13/26/40/60/90 for 80/100/120/140/160 BPM. A current, confident beat adds only 0–10 depending on the confidence. After a lower tempo, energy falls at 20 points/s; when rising, at 4 points/s. Silence lowers the value after 1 s at 20 points/s down to 0; unknown audio inputs freeze the value. |
| annoyance | 0 | Impact, Shake and MultiTap from three taps raise the value. The gesture classifier reports at most one event per second in total; decay 0.1/s towards 0 (one point per ten seconds). |
| attention | 0 | Event impulses see below. While nobody is present it returns at 3/s towards 0; while anyone is present it does not fall and has a floor of 20 farther away and 60 directly in front (`add-presence-mood`). |
| loneliness | 50 | +0.1/s up to 100 while nobody is present; while someone is present it falls instead, 100 to 0 in about 20 minutes (`add-presence-mood`). Gestures and presence events reduce it, see the event tables. |
| depression | 0 | Rises with loneliness above 80 at up to 0.2 points/s and falls with attention above 80 at exactly the same rate. Fresh, non-silent music lowers it at 0.05 points/s; 80–100 BPM are neutral, the tempo share rises to 0.1 points/s at 120 BPM and falls to 0 at 160 BPM. Little bass adds up to 0.12 points/s against it. Without influence the value stays put. Above 50 the panel, if it is free, shows a local goth verse with a grave, coffin or urn emoji every two minutes in every operating state. |

All values are limited to 0–100. The numeric mappings are provisional calibration values, not character traits individually confirmed by the user. The state service runs about every 100 ms and additionally before a motion event. Gaps over 1 s are limited to 1 s of integration and reset the audio confirmation; no long caught-up mood evolution after a blocked loop. Time differences work across `millis()` overflow.

**Audio – refined rule:** Loudness does not drive energy up. Since commit `30ac9a7`, BPM, confidence and the last onset come directly from `voiceBaseInput.v2Tempo()`; the former distinction between the BeatAnalyzer value before the lock and `currentBpm` after it no longer exists. Energy therefore follows the unsmoothed tracker value of each 250 ms evaluation, limited to 80–160 BPM, and can differ from the displayed show tempo. The base runs from about 13 to 90 between 80 and 160 BPM: 120 BPM is at 40, 140 at 60, 150 at 75 and 160 at 90. A current, confident beat adds only 0 to 10 depending on the confidence. A 160 BPM track with maximally clear beats thus reaches 100. The beat bonus and the tempo trust used for depression (three close, fresh estimates, valid for 15 s afterwards) require confidence ≥ 68; this value comes from the BeatAnalyzer scale and has not been recalibrated for the new tracker (section 0, open checks). Half/double tempo remains a limit of the estimator.

Silence is checked independently of the adaptive music gate on the RMS of the high-pass-filtered PCM blocks: entry below 80, exit from 120 (16-bit sample units). After one second of continuous silence, energy falls at 20 points/s down to 0; the return of a signal ends this decay immediately. Capture not ready, or more than 250 ms without a processed block, means Unknown and freezes the audio energy effect; it never counts as silence. These absolute thresholds are hardware calibration values, not already confirmed room-noise limits.

`audio_mood_features.{h,cpp}` additionally analyses the existing raw 16 kHz PCM blocks with a simple high-pass near 40 Hz and two low-pass stages near 180 Hz. Ratio of bass-band power to total power after the high-pass, without an FFT, additional task or audio buffer. It is a band indicator with a broad roll-off, not an exact spectral integration of 40–180 Hz. Powers are smoothed with a time constant of about 15 s; the bass value is released only after 15 s of usable samples. Silence, >1 % clipped samples per block or capture gaps discard the bass history. Existing DC/VU/BeatAnalyzer paths and library versions are retained.

Depression starts at 0 and changes only through active influences: each point of loneliness above 80 raises the rate linearly up to 0.2 points/s at 100; each point of attention above 80 lowers it by exactly the same rate. These social contributions also apply without analysis. Fresh, non-silent music permanently adds −0.05 points/s. The BPM range 80–100 is neutral because of possible half-time detection. Above it the tempo share rises linearly to +0.1 points/s at 120 BPM and falls back to 0 at 160 BPM. Little valid bass adds linearly up to 0.12 points/s at ≤8 % power share; at ≥25 % it adds 0. The positive contributions can outweigh the music decay. Without influence the value stays unchanged; the result stays between 0 and 100. Unknown audio inputs and silence provide no low-bass or slowness contribution. Audio never changes loneliness. The bass weights and thresholds must be checked with the Voice Base and real music material.

**Motion/orientation:** Internal IMU enabled; fresh acceleration and gyroscope data at most every 20 ms. The standalone hardware-independent classifier processes 3D vectors instead of only their magnitudes. The first calm measurement (0.85–1.15 g, <15 degrees/s) defines the reference pose. A low-pass with a 250 ms time constant estimates the gravity direction; the difference from the current sample yields dynamic acceleration. The following limits are explicitly **provisional heuristics**, not gesture recognition validated on the enclosure.

- Observation window of 700 ms from dynamic acceleration ≥0.30 g or rotation rate ≥45 degrees/s. At most one event per window, priority Shake → Impact → Rotate → MultiTap → Tap → Move. In addition, a global event cooldown of one second applies after every reported event. A shake is therefore not counted as several bumps at the same time and produces no new debug output within that second. Gestures at window boundaries can be split; detection latency up to about 700 ms.
- **Tap / MultiTap:** short impulse from 0.45 g, end below 0.20 g, duration at most 120 ms; starts of consecutive impulses at least 100 ms apart. Two or more completed impulses in the window yield MultiTap with a counter. A single impulse counts as Tap only with at most 120 ms of total motion in the window.
- **Impact:** dynamic peak ≥1.8 g, provided no shake sequence was detected.
- **Shake:** at least three direction changes of strong acceleration vectors (magnitude ≥0.55 g, dot product with the previous direction <−0.15 g²). A high level alone is not enough.
- **Rotate:** The rotation rate is integrated for each axis in the observation window, even during slight hand movement. Rotate is triggered only from a total of 35 degrees of rotation; a short gyro spike during a linear movement is not enough. Tilting forwards and sideways during the movement therefore counts as Rotate if it reaches this angle change. If the new pose is then held calmly for at least 500 ms, Tilt can be triggered additionally.
- **Tilt:** the current, calmly held gravity direction deviates more than 25 degrees from the start pose, for at least 500 ms at a magnitude near 1 g and a rotation rate <15 degrees/s. The pose does not use the dynamic motion filtering, so that lifting or bumping does not produce a false tilt. Returning below 15 degrees releases the tilt state. While the device stays tilted, a Tilt event every 2 s. Tilt is reported outside an active motion window.
- **Move:** at least 150 ms of general motion from 0.30 g in the window without a more specific class. Standing still in the reference pose produces no engagement.
- Invalid values or sample gaps >150 ms discard started windows. No caught-up gestures from missing data. Timestamps support `millis()` overflow. Absolute magnetic heading is not an input; rotation and tilt are relative quantities from gyroscope/acceleration.

| Event | attention | annoyance | loneliness |
| --- | ---: | ---: | ---: |
| Move | +8 | 0 | −5 |
| Tap | +12 | 0 | −8 |
| MultiTap | +18 | from 3 taps: +6 | −12 |
| Impact | +20 | +15 | −10 |
| Rotate | +10 | 0 | −6 |
| Tilt, also while held | 0 | 0 | 0 |
| Shake | +25 | +10 | −15 |

**Presence (since `add-presence-mood`):** The Mainframe decides itself whether someone is in front, with its own thresholds on the TMOS presence value, in two zones: near (up to about 1 m) and far (up to about 2 m); the sensor flag alone does not count, because a stale sensor baseline sets it with nobody there. Presence events:

| Presence event | When | attention | annoyance | loneliness |
| --- | --- | ---: | ---: | ---: |
| Kommt davor | about 2 s in the near zone, once per stay; also wakes the sleeping creature | +25 | 0 | −15 |
| Läuft vorbei | present, gone again before the 3 s | +8 | 0 | −5 |
| Geht weg | the person who came leaves the near zone | 0 | 0 | 0 |
| Steht weiter weg | 2 s in the far zone, once per visit, not after Kommt davor | 0 | 0 | 0 |
| Bewegt sich | a present person moves; at once, then at most every 9 s; no text | +5 | 0 | 0 |

Each presence event has its own panel texts, fully equal to the gesture texts. Calibration values are in the change's design.md.

Motion changes neither energy nor depression directly; energy thus stays clearly musical. Engagement can influence depression only through the raised attention value above 80 and through lowering loneliness. All values saturate at 0/100. The time-dependent evolution is updated before an event up to its timestamp. `MotionEvent` keeps type, timestamp, sequence number, number of short impulses and dynamic peak separate from the mood. `MotionEvents::latest()` returns the last event without additional allocation. This is an input usable later for the small language model; a model and an event history are not integrated here yet. Events are classification hypotheses, not a secured interpretation of human intent.

**Node light layer:** The mood values stay exclusively in the Mainframe. After each personality update it derives a neutral `NodeVisualState` message from them and repeats it every three seconds. It describes only the output: RGB scene or solid-colour mode, brightness, a future colour overlay and the flash behaviour. The Node first stores the message in the receive callback and then applies these render directives outside the callback. It knows neither emotional names nor thresholds. V1 does not know the additional message type and discards it.

The current Mainframe rule is: at annoyance ≥90, solid red at 100 % and flash permanently on are sent; otherwise at loneliness ≥90, scene brightness 15 % and flash off are sent; otherwise the normal scene values. Annoyance takes precedence on overlap. Scene, clock and renderer state keep running underneath the render directives. When the Mainframe sends the normal values again, the next renderer frame visibly continues the current scene. NodeText and regular flash pulses are suppressed during a visual directive. The flash driver has no real dimming level; switching off is the simple directive. Colour overlays will later be sent from the Mainframe as colour and opacity; the Node merely applies them to its filtered scene.

Host tests for personality, motion, audio, tempo, iris and protocol including NodeVisualState pass (`sh tests/run_all.sh`). Mainframe and Node build with the pinned dependencies. The Mainframe was flashed to the connected AtomS3R on 2026-09-26; every Node needs the new Node firmware before it can receive the visual directives. A hardware test of the visual directives with a flashed V2 Node is pending. The state is committed.

**Button/display:** Hold threshold 1200 ms, a single toggle while holding. Outside debug mode a short click starts the show or changes the scene on release; since `add-presence-sensor-debug` a short click inside debug mode only switches between the mood and the presence debug screen. Releasing after a long press does not trigger a click. The button itself changes no mood values; physical movement while operating it can still excite the IMU. EyeRenderer draws the mood screen with five labelled numbers and bars in the existing 128×128 canvas before the single display push; since `add-presence-sensor-debug` both debug screens clear the whole display instead of covering part of the running eye. It uses the renderer's 25 ms frame interval. In addition the debug display shows the last event type and the number of short impulses, so that the distinction can be checked on the device.

**Text output:** `MessagePanel` is the only product text surface. It lies almost full-screen as a PS1-like, violet single outline with cut corners above the still running eye; cyan corner marks accent the outline. A 75 percent scanline dither darkens the eye but leaves it visible. Each visible text line gets its own dark, rounded block only behind its content. Any source can send a UTF-8 message via `show()`; motion is the first connected sender. Each motion type has two or three short local variants; the selection rotates deterministically and does not repeat the same variant for immediately identical events. Each local motion text variant ends with a fitting emoji. A message closes after five seconds, even if its triggering event persists. The nine-second cooldown applies per motion type: another tap is slowed down, a bump or shake may show a new message immediately. Four large, left-aligned lines in a fixed pixel font wrap at word boundaries after at most eight text columns. Only with more than four lines does the text scroll upwards line by line after 1.2 seconds and stop as soon as the last four elements are visible. A small white triangle with a gap to the frame blinks at the bottom right while more text follows. The wrapping splits neither UTF-8 sequences nor emoji. An emoji at the end of a message is a further, double-height element of this same line flow: it becomes visible centred only after the text lines and scrolls with them; its small background block appears only in its own line. Ä, Ö, Ü, ä, ö, ü and ß as well as the curated emoji palette are dedicated glyphs; unknown Unicode characters appear as `?`. The entire curated text emoji palette comes from an approved 32-pixel sprite atlas instead of runtime primitives. Emoji faces and hands are porcelain white instead of yellow, have stepped cold-blue shading, strong kohl and violet eye shadow under both eyes; dark circles appear depending on the expression. The debug display remains a separate diagnostic view.

**Verification:** `sh tests/run_personality.sh` checks BPM targets/confidence, independent loneliness, fast silence/reaction to audio return and unknown inputs as well as depression influences. `sh tests/run_audio_mood_features.sh` checks synthetic 100/1000 Hz signals, loudness independence of the bass ratio, warm-up time, silence, DC, capture gaps, clipping and time overflow. `sh tests/run_motion_events.sh` checks synthetic traces for Tap/MultiTap, Impact, Rotate, held Tilt, Shake, standstill, data gaps, invalid samples and time overflow. `sh tests/run_message_panel.sh` checks lifetime, motion wiring and the text run. In addition, all other host tests via `sh tests/run_all.sh` and `pio run -e mainframe`. Hardware acceptance is pending: perform the same gestures several times, compare the last event class in the overlay, check tap strength/mounting/bass vibrations, mind the tilt reference at start, measure audio/clock continuity under IMU and display load. Synthetic tests prove the rules, not their discrimination on the enclosure. On 2026-09-26, at the user's explicit request, flashed to the AtomS3R on `/dev/cu.usbmodem101` (ESP32-S3-PICO-1, MAC `14:c1:9f:d5:6e:ec`): upload successful, flash hash verified, hardware reset triggered. The subsequent BPM/bass/silence version was also flashed successfully to the same device, hash verified and reset triggered (flash 917,901 bytes, static RAM 55,164 bytes according to the build). All seven host tests at that time passed. The annoyance decay subsequently slowed to 0.1 points/s was checked with personality/protocol tests and a Mainframe build and also flashed successfully (hash verified, reset triggered). Energy can stay at the reached value when tempo evidence runs out; the permanently high energy reported by the user has not yet been attributed to a cause without current input measurements. Bass/silence thresholds, detection quality and runtime under real music load remain to be checked on the device. These flash notes concern the state of 2026-09-26; later flashes (creature sounds, tempo work) are not recorded individually here. The code is fully committed; on 2026-11-14 all 15 host tests pass.

## C. Technical reconciliation, open decisions and experiments

### C.1 Hardware reconciliation – manufacturer information, no code verification

Checked on 2026-09-22 against official M5Stack documentation:

- The stated AtomS3R key data is correct. It additionally has a BMM150 magnetometer; its use is not required. Internal BMI270 bus: SDA G45/SCL G0, button G41, Grove G2/G1. The display needs a different driver path from the M5Atom matrix. Source: [AtomS3R](https://docs.m5stack.com/en/core/AtomS3R).
- The Atomic Voice Base uses an ES8311 audio codec and I²S. The previous PDM driver including GPIO26/32 cannot be taken over directly. Source: [Atomic Voice Base](https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base).
- M5Stack documents the combination S3R + Voice Base: codec I²C SDA G38/SCL G39; I²S output from the host G5, LRCK G6, input to the host G7, BCLK G8. Source: [AtomS3R + Voice Base](https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot).
- U185/STHS34PF80 uses I²C address 0x5A, presence/motion detection, no camera image stream. Source: [Unit TMOS PIR](https://docs.m5stack.com/en/unit/UNIT-TMOS%20PIR).

Before the port, board/flash/PSRAM settings, the actual hardware variant, codec gain, PCM format, display initialisation and I²C bus assignment must be checked together. Internal sensors, codec and Grove sit on different pin pairs; the library and bus management must not be assumed to be automatically conflict-free. The overall electrical/mechanical integration was not checked on the device here.

### C.2 Explicit contradictions and requirements not yet met

| Plan/assumption | Finding and consequence |
| --- | --- |
| Enhanced features must not be a prerequisite for the show or impair it | The offline base path exists, but active cloud sessions pause the Mainframe radio control. The strong priority requirement is therefore not met yet. |
| One Mainframe with Wi-Fi and ESP-NOW | A possible goal, but not an independent second channel. With fixed Nodes on channel 1, arbitrary AP channels do not work reliably at the same time. |
| Simply take the existing system over to the S3R | Algorithmic parts are transferable; board, matrix, GPIOs, microphone driver and SDK interfaces need a real port. |
| Cycle through the existing scenes | Two compiled-in scenes (A.3) make takeover errors visible; their final light design is open. |
| Take over audio/BPM stably | The old scanner depends on block/loop timing and uses absolute level thresholds. V2 therefore did not stabilise it but replaced it as the clock source on 2026-10-03 with a level-normalised PCM tracker with a sample time base. "Stable" has not yet been proven on the device for the new path (section 0). |
| Iris takes over the existing VU | Implemented as a level/colour takeover with 36 iris polygons (section 0); level thresholds are provisional. |
| Whole installation red on annoyance | Only colour-capable Nodes can turn red. Implemented as `NodeVisualState` with solid-colour mode and precedence over the scene (B.6); flash Nodes switch permanently on. Test on Node hardware open. |
| Local language model on 8 MB PSRAM | Unproven experiment; available memory capacity is no proof of suitable quality, model size or latency. Not part of the offline core promise. |

Espressif documents the channel binding and that even successful MAC delivery does not guarantee processing at application level. Source: [ESP-NOW for ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html). The current API there says nothing about the SDK version installed in the existing PlatformIO build.

### C.3 Open design decisions – no anticipated implementation

- **Scene protocol:** Scene index and idempotent selection are implemented; repetition every 30 seconds. No Mainframe session and no one-second repetition. The dedicated V2 radio identity is implemented, checked on the host and flashed on the Mainframe; the end-to-end check with a V2 Node is pending. Still open are a possible receive queue for more message types in future, optional state confirmations and the measurement of repetition interval and rejoin behaviour on hardware. No shared scene barrier is intended.
- **Time base:** Reception timestamps, real beat serial vs. sync packet serial, latency-aware phase alignment, handling of late packets and non-blocking Node output. Do not derive an invented musical downbeat from the existing onset detector.
- **Wi-Fi policy:** One possible base policy would be to allow only matching Wi-Fi networks as an enhancement while keeping ESP-NOW channel 1 unchanged, and to stay offline otherwise. Joint channel changes of all Nodes are a different, considerably more complex option. No choice has been made yet; pure holdover playback does not replace the required continuous controllability.
- **Audio port:** Separate capture from analysis; a fixed frame time or a real sample time base as an improvement to be checked. First compare the old signal characteristics and confidence/lock behaviour; do not silently replace the algorithm. *Actual state:* capture ring and sample time base are implemented. The algorithm was replaced on 2026-10-03; a documented comparison of the same material between the old and the new path does not exist. Also open are phase tracking, behaviour during silence/breaks and the handling of half/double tempo in the adoption stage; the user decided on 2026-11-14 to settle these only after the hardware acceptance (section 0).
- **Audio quality:** Assess clip readiness, clipping, level, temporal continuity and actual recording length. The existing 12 s request does not guarantee a gapless 12 s clip when RAM shortening or lost DMA samples occur.
- **Display:** Iris polygon mapping and text layout are implemented (section 0, B.6); the frame budget and frame skipping under load remain open. The speaker is part of the hardware scope but not yet a planned speech output for the personality.
- **Sensors:** TMOS presence needs temporal filtering and is not a person identifier. IMU classes and sampling rate are set heuristically (B.6); mounting, acceptance on the enclosure and training data are open. Bass/vibration and light tapping must be distinguishable on the actual enclosure; speaker operation can influence audio and vibration inputs.
- **Personality:** Event rules, decay curves, text selection/cooldowns and two light overrides are implemented with provisional values (B.6). Still open are confirmation of these calibration values, persistence, the combination of several states and further light effects.
- **TinyML:** first a measurable need and a data set, then the model decision; separate options for IMU and audio.
- **Local language model:** check feasibility against the resource budget only at the end; local text packets remain the working base.

### C.4 Proposed later acceptance, not yet carried out

1. Mutual rejection of all five V2 packet types between V1 and V2 while V2 communication stays valid; then offline start of all Node types/roles; different boot order; Node/Mainframe restart; role change; repeated scene changes and targeted packet losses.
2. Repeated switching between both scenes, also during running beats; no unintended waiting phase on scene changes and measurement of the takeover after packet loss: currently the next periodic repetition follows only after 30 seconds; a triple `SceneSelect` burst is not implemented.
3. Clock measurement under flash/RGB load, without a network and with a Wi-Fi outage and an incompatible AP channel. Display/cloud load must not starve audio and Node control.
4. Comparison of the same audio test material before/after the port: detected BPM, clock target, show BPM, confidence, lock time, phase error, half/double tempo, breaks/silence, speech/tapping and signal overload. For the new tempo path, `mainframe-beat-tracker` outputs tracker BPM, raw value, confidence and show BPM over serial; phase error and lock time are not output, and the old path is no longer wired in parallel.
5. Only after that additional analyzer features and personality/sensors, then experimental models.

All items in C are open decisions, derived verification needs or explicitly labelled experiments. They must not be treated as already existing functionality or as a decided concrete software architecture.
