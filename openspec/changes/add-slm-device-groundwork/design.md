# Design

Every number below is marked: **[measured]** on the device on 2026-11-17 (branch `claude/loop-load-measurement-f3ac43`, env `mainframe-loop-load`), **[reference]** taken from https://github.com/slvDev/esp32-ai and not checked by us, or **[computed]**.

## Context

See proposal.md for the motivation. State that shapes the approach:

- Everything of ours runs in `loop()` on core 1; the Mainframe creates no task of its own. The radio stack runs on core 0.
- Core 1 busy share: 55 % idle, 59 % show with music and no text, 75 % while a text is visible throughout **[measured]**.
- Eye frame: 11.8–12.0 ms without text, 17.6 ms with text, peaks up to 61 ms, about 35 frames/s; eye share of core 1: 41 % / 43 % / 62 % **[measured]**. The frame interval is 25 ms (`kFrameIntervalMs`).
- Largest gap between audio blocks: 54 ms without text, 94 ms with text; between IMU samples: 32 ms idle, 37 ms show, 71 ms with text. No audio drops, backlog at most 6 of 16 blocks **[measured]**.
- Free memory: 8.06 MB PSRAM, about 255 KB internal RAM **[measured]**.
- Flash: `default_8MB.csv` has app0 and app1 of 0x330000 bytes each (3,342,336), SPIFFS 0x180000 (1,572,864), coredump 0x10000. Firmware size 1.07 MB **[measured by build]**. The Mainframe code uses neither OTA, SPIFFS nor NVS.
- Reference model "Barista": 8.9 M parameters, 6 layers, 4 heads, dimension 128, vocabularies 8057 in / 854 out, 4.6 MB image mapped from flash, about 2.5 MB PSRAM, 60 ms per output piece on both cores, 1.75× faster than one core **[reference]**; so about 105 ms per piece on one core **[computed]**.
- The reference builds with Arduino ESP32 3.3.10; we are pinned to `espressif32@6.13.0`, which is Arduino core 2.0.17.

## Goals / Non-Goals

**Goals:**

- Know, from device measurements, whether a model of the reference size can compute on the Mainframe within the limits of Decision 6, and how time per output piece scales with model size.
- Lower the load while a text is visible, for today's firmware already.
- A runtime that can be removed without trace: build flag, own env, own partition table.

**Non-Goals:**

- No German model, training data, training, prompt or state format, text stock, fallback rules or panel integration. Those are later changes and depend on the numbers from this one.
- No change to Wi-Fi, the radio protocol, `shared/include` or the Node firmware.
- No tuning of the eye renderer beyond the frame interval.

## Decisions

### 1. Scope: groundwork only (user decision, 2026-11-17)

Device groundwork and measurements without a trained model. Alternative considered: a first slice of generated text in the same change; rejected by the user because feasibility on the device is not known yet.

### 2. Frame rate under the panel is product behaviour; the value is measured first (user decisions)

The reduced interval applies only in frames where the panel is actually drawn (not during sleep or debug screens) and ships in the normal `mainframe` build. The renderer already knows per frame whether it draws the panel, so the interval is picked there; no change to `MessagePanel`.

Candidates to measure (calibration, not requirement text):

| Interval under the panel | Frames/s | Busy share of core 1 with text |
|---|---|---|
| 25 ms (today) | about 35 **[measured]** | 75 % **[measured]** |
| 40 ms | about 25 **[computed]** | about 65 % **[computed]** |
| 50 ms | about 18–20 **[computed]** | about 45–50 % **[computed]** |

The user picks the value after seeing the numbers and the look on the device. If no candidate is worth it, the eye-iris delta is dropped from this change. The scroll step (0.8 s) and the more-text blink (450 ms) are far slower than any candidate, so the panel itself stays readable **[computed]**; whether the eye under the shade looks acceptable can only be judged on the device.

### 3. Flash layout of the measurement build (user decision: both areas go)

`mainframe-slm-capture` uses its own table:

| Name | Offset | Size | Note |
|---|---|---|---|
| nvs | 0x9000 | 0x5000 | unchanged |
| otadata | 0xE000 | 0x2000 | kept so app0 stays at its offset |
| app0 | 0x10000 | 0x330000 | 3,342,336 bytes, firmware 1.07 MB |
| model | 0x340000 | 0x4B0000 | 4,915,200 bytes, custom data subtype |
| coredump | 0x7F0000 | 0x10000 | unchanged |

4,915,200 bytes is the only layout in which an image of the reference size fits; "4.6 MB" **[reference]** is 4.6–4.83 million bytes depending on the unit, leaving 92–315 KB spare **[computed]**. The exact file size is checked before flashing. Consequence accepted by the user: no over-the-air update with this layout. The normal build keeps `default_8MB.csv`; flashing it again restores the old table. The model image is written with a separate flash command to offset 0x340000 and is not part of the firmware upload.

Alternatives: only the second app slot (3.19 MB) or only SPIFFS (1.5 MB); both too small for the reference image.

### 4. Runtime: port of the reference inference code, memory-mapped, PSRAM working memory

- Port the reference project's inference kernel (MIT, licence notice kept) into `apps/mainframe/src/slm/`, compiled only with `-DDECAFLASH_SLM=1`. Porting instead of writing our own keeps the image format, so the Barista image can prove correctness.
- The kernel is plain C++ behind a small interface (map image, reset, step one piece), with the ESP-specific parts (partition mapping, PSRAM allocation, task) outside it. That makes it host-testable: the host test and the device must produce the same output pieces for a fixed input and seed.
- The image is mapped with the partition mapping API and read in place. KV cache and scratch buffers come from PSRAM (about 2.5 MB **[reference]** of 8.06 MB free **[measured]**); nothing large from internal RAM.
- The image starts with a header (magic, format version, dimensions, sizes). A wrong magic, version or size, or a failed allocation, leaves the runtime off (spec: Missing or invalid model).
- Model image files are not committed; they live in an ignored local folder. The random-weight generator is a small host script in the repo, so images are reproducible from a seed.

### 5. Task model: core 0 first, core 1 worker as a separate measurement (user decision)

- One task pinned to core 0 at the lowest application priority (1), below the radio stack and below `loop()`. It yields with a one-tick delay at least after every layer of every output piece, so the idle task of core 0 runs and the task watchdog stays quiet.
- For measurement the task generates continuously (worst case). Duty cycle and pauses are product questions for a later change.
- The task shares no state with `loop()` except counters read for the report (time per piece, pieces generated, stack high-water mark), exchanged without blocking.
- Step two, measured separately: an additional worker on core 1 at a priority below `loop()`. Since `loop()` only sleeps in `delay(5)`, the worker gets what core 1 leaves free: 25–45 % today **[measured]**. It is kept only if the limits of Decision 6 hold; otherwise core 0 alone is the result.

Alternative: both cores from the start, as the reference does; rejected because it mixes two effects in one measurement.

### 6. Acceptance limit: not worse than today (user decision)

With the placeholder model generating continuously, in each of the three states (idle; show with music; show with music and a text visible):

- 0 audio drops.
- Largest audio block gap and largest IMU sample gap not above the baseline of the same state. Baseline of 2026-11-17: audio 54 ms (show) / 94 ms (text); IMU 32 / 37 / 71 ms **[measured]**.
- No watchdog reset, no crash, in a run of at least 10 minutes per state.
- Nodes stay in time: ESP-NOW send failures on the Mainframe and visible beat timing on a V2 Node not worse than without the model.

Maxima are noisy. Every comparison therefore uses a baseline taken in the same session, with the same build flags apart from the step under test, the same music, and the same durations as the 2026-11-17 run (50 s idle, 80 s show, 95 s text phase), not the table above alone.

### 7. Placeholder models (user decision: both, one after the other)

1. Barista image from the reference repo, for correctness: device output equals host output for fixed inputs, and time per piece is compared with the reference's 60 ms on two cores / about 105 ms on one **[reference / computed]**. Downloading it is a download from the internet and is confirmed with the user at that step (file name, source, size).
2. Random-weight images from the host script, for load: the reference dimensions and two smaller ones (assumption of this design, adjustable: 4 layers × dimension 96 and 3 layers × dimension 64), to get time per piece and PSRAM use against size. Random weights have the same cost per piece as trained ones **[computed: cost depends on dimensions, not values]**.

### 8. Arduino core: open, to be decided in its own session (user decision, 2026-11-17)

The reference uses Arduino ESP32 3.3.10, we are pinned to core 2.0.17. Whether to port to the pinned core or to move the pin first was judged too large for this interview; the user wants a separate session. Consequences for this change:

- Task groups 1 and 2 (baseline, frame rate) do not depend on it and can run now.
- Task groups 3 onward (partition table, runtime, model measurements) start only after that decision.
- If the core changes, it does so as its own step under the Dependencies rule of `CLAUDE.md`, and the baseline is measured again on the new core before any model measurement, because audio driver, radio and timers change with it.

### 9. Measurement method

- Instrument: the loop-load report from `claude/loop-load-measurement-f3ac43` (`-DDECAFLASH_LOOP_LOAD=1`), extended in the measurement build by: time per output piece (average / maximum), pieces per second, PSRAM and internal RAM free / minimum, model task stack high-water mark, and ESP-NOW send results (count, failures, largest lateness of a clock packet against its planned send time).
- `mainframe-slm-capture` extends the normal build with `DECAFLASH_LOOP_LOAD` and `DECAFLASH_SLM`. Per `CLAUDE.md`, measure with audio analysis running and flash the normal `mainframe` build afterwards.
- Reading the serial port restarts the Mainframe and is asked for before each capture.

## Risks / Trade-offs

Risks that only a device measurement can settle:

- [Flash cache and PSRAM contention] Weights read through the flash mapping and the KV cache in PSRAM share cache and memory bus with the firmware's own code and with the eye's off-screen buffer. Cache misses in the model can slow `loop()` on the other core even though it computes on core 0. → Measured in group 5 against the baseline; smaller model sizes in group 7 show how it scales.
- [Radio timing on core 0] The model task is below the radio stack, but each yield point bounds how long lower-level radio housekeeping can be delayed, and cache contention is not solved by priorities. → Send failures and clock packet lateness on the Mainframe plus a V2 Node check; needs the end-to-end test with a flashed V2 Node to have passed (`CLAUDE.md`, Verification).
- [The 94 ms audio gap with text] Already close to trouble today; backlog reached 6 of 16 blocks. Any extra load on core 1 (worker of step two, cache effects) can turn it into drops. → Frame rate reduction first (group 2), then model; the worker on core 1 is last and optional.
- [Watchdog] A long uninterrupted matrix product on core 0 starves the idle task. → Yield per layer; a 10-minute run per state without reset is an acceptance criterion.
- [Speed on one core] About 105 ms per piece is **[computed]** from the reference; on our board, with a busy radio core and an older toolchain, it may be much slower. → Measured in group 5; this number decides how a later change designs the text stock.
- [Port does not build on core 2.0.17] Vector instructions or APIs used by the reference may be missing in the pinned core. → Decision 8 comes first; a failed port is reported, not worked around.
- [Image does not fit] Spare room in the model partition is 92–315 KB **[computed]**. → Check the file size before flashing; a later own model can be sized to fit.
- [Lower frame rate looks wrong] The eye under the panel may look jerky. → The user judges on the device and picks the value, or drops the reduction.
- [Wrong partition table left on the device] After a measurement the device has no second app slot. → Flashing `mainframe` restores `default_8MB.csv`; this is a tracked task.

Trade-off accepted: the runtime ships in no product build yet, so this change adds code that only a measurement env compiles.

## Migration Plan

- Normal build: only the frame interval changes; rollback is reverting that commit.
- Measurement build: flash `mainframe-slm-capture`, write the model image to 0x340000, measure, flash `mainframe` again.

## Open Questions

- Arduino core (Decision 8): blocks groups 3 onward, own session.
- Follow-up changes, each needing its own interview, none decided here: which panel content the model takes over and what stays a fixed list; text stock versus generating after the event and what happens when no text is ready; the state fed to the model; language and tone; source of training data (generating it with a large model is a paid external service and needs the user's explicit yes); whether the model partition and runtime move into the normal build.
