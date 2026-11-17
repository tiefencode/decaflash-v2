# Language model prototype (Mainframe)

A prototype, not product behaviour, and therefore not an OpenSpec change (user decision, 2026-11-17). It answers one question with device measurements: can a tiny on-device language model compute on the Mainframe without hurting beat detection, the show clock and ESP-NOW? If the numbers say yes and the user wants it in the product, the product behaviour is then specified through `openspec/changes`, with these measurements as its basis. The existing specs in `openspec/specs` stay the reference for how the show must behave meanwhile.

Rules for the prototype:

- The runtime exists only in measurement builds. The normal `mainframe` build contains no model runtime, no model partition and no model task.
- Model output reaches neither the message panel, the Nodes, the personality, the show clock nor the tempo path. The runtime only reports measurements on the serial line.
- A missing, empty or unrecognised model image, or a failed allocation, leaves the runtime off with a reason on the serial line; the show runs as in the normal build.
- No access point, internet or external service; no Wi-Fi association, no channel change.

The reduced frame rate under the message panel is product behaviour and has its own change, `openspec/changes/reduce-panel-frame-rate`.

Every number below is marked: **[measured]** on the device on 2026-11-17 (branch `claude/loop-load-measurement-f3ac43`, env `mainframe-loop-load`), **[reference]** taken from https://github.com/slvDev/esp32-ai and not checked by us, or **[computed]**.

## Context

State that shapes the approach:

- Everything of ours runs in `loop()` on core 1; the Mainframe creates no task of its own. The radio stack runs on core 0.
- Core 1 busy share: 55 % idle, 59 % show with music and no text, 75 % while a text is visible throughout **[measured]**.
- Eye frame: 11.8–12.0 ms without text, 17.6 ms with text, peaks up to 61 ms, about 35 frames/s; eye share of core 1: 41 % / 43 % / 62 % **[measured]**. The frame interval is 25 ms (`kFrameIntervalMs`).
- Largest gap between audio blocks: 54 ms without text, 94 ms with text; between IMU samples: 32 ms idle, 37 ms show, 71 ms with text. No audio drops, backlog at most 6 of 16 blocks **[measured]**.
- Free memory: 8.06 MB PSRAM, about 255 KB internal RAM **[measured]**.
- Flash: `default_8MB.csv` has app0 and app1 of 0x330000 bytes each (3,342,336), SPIFFS 0x180000 (1,572,864), coredump 0x10000. Firmware size 1.07 MB **[measured by build]**. The Mainframe code uses neither OTA, SPIFFS nor NVS.
- Reference model "Barista": 8.9 M parameters, 6 layers, 4 heads, dimension 128, vocabularies 8057 in / 854 out, 4.6 MB image (`model.bin`, 4,600,186 bytes) in flash, about 2.5 MB PSRAM for the layer weights unpacked at start, 60 ms per output piece on both cores, 1.75× faster than one core **[reference]**; so about 105 ms per piece on one core **[computed]**.
- The reference was measured with Arduino ESP32 3.3.10 (GCC 14, `-O3`); we are pinned to `espressif32@6.13.0`, which is Arduino core 2.0.17 (GCC 8.4.0, `-Os`, gnu++11). Decision 8 lists what the reference needs from the core.

## Goals / Non-Goals

**Goals:**

- Know, from device measurements, whether a model of the reference size can compute on the Mainframe within the limits of Decision 6, and how time per output piece scales with model size.
- A runtime that can be removed without trace: build flag, own env, own partition table.

**Non-Goals:**

- No German model, training data, training, prompt or state format, text stock, fallback rules or panel integration. Those are later changes and depend on the numbers from this one.
- No change to Wi-Fi, the radio protocol, `shared/include` or the Node firmware.
- No change to the eye renderer; the frame interval under the panel is handled in `reduce-panel-frame-rate`.

## Decisions

### 1. Scope: groundwork only (user decision, 2026-11-17)

Device groundwork and measurements without a trained model. Alternative considered: a first slice of generated text in the same change; rejected by the user because feasibility on the device is not known yet.

### 2. Frame rate under the panel: moved to its own change

See `openspec/changes/reduce-panel-frame-rate`. The prototype's full-firmware measurements (groups 6 and 7) start from the baseline taken after that change's last device task, because the text phase is the tightest state today.

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

### 4. Runtime: port of the reference inference code, image mapped from flash, layer weights and working memory in PSRAM

- Port the reference project's inference kernel (MIT, licence notice kept) into `apps/mainframe/src/slm/`, compiled only with `-DDECAFLASH_SLM=1`. Porting instead of writing our own keeps the image format, so the Barista image can prove correctness.
- The kernel is plain C++ behind a small interface (map image, reset, step one piece), with the ESP-specific parts (partition mapping, PSRAM allocation, task) outside it. That makes it host-testable: the host test and the device must produce the same output pieces for a fixed input and seed.
- The model partition (4.8 MB) is mapped as a whole with the partition mapping API; the image is checked against the partition size before anything is read. As in the reference, the embedding tables are read in place from flash, and the layer weights (4-bit in the image) are unpacked once at start into 8-bit copies in PSRAM (user decision, 2026-11-17: keep the reference behaviour instead of computing straight from flash, so the kernel stays identical to the reference). Unpacked weights, KV cache and scratch buffers together take about 2.5 MB PSRAM **[reference]** of 8.06 MB free **[measured]**; the reference keeps about 32 KB of scratch and norm data in internal RAM **[reference]**, nothing larger.
- The image starts with a header (magic, format version, dimensions, sizes). A wrong magic, version or size, or a failed allocation, leaves the runtime off (see the rules above).
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

### 8. Arduino core: stay on 2.0.17, compare both cores with a small benchmark firmware (user decision, 2026-11-17)

The normal firmware and `mainframe-slm-capture` stay on `espressif32@6.13.0` (Arduino core 2.0.17). The runtime is ported to that core. Because the user wants the model speed on both cores as measured values, a small benchmark firmware that contains only the runtime kernel is built once per core and measured on the same device with the same image. A move of the whole firmware is not part of this change; the user decides on it after seeing both numbers, and it would be its own OpenSpec change with before/after device measurements.

Facts behind the decision. **[read]** = read in the reference repo (commit 52ebcbb) or in our code and installed packages, **[web]** = from release notes and migration guides, **[assumed]** = not checked. Nothing here was compiled or run.

What the reference needs from the core:

- The kernel (`runtime/llm.h`, 560 lines, and `runtime/bpe_tokenizer.h`, 270 lines) is plain C with only `stdint`, `math` and `string`; it is host-tested in the reference repo **[read]**.
- No vector instructions and no esp-dsp: the hot loop is a scalar 8-bit dot product **[read]**. esp-dsp with ESP32-S3 variants is nevertheless part of core 2.0.17 **[read]**.
- No language feature found that GCC 8.4 with gnu++11 rejects **[read, by search]**.
- ESP-specific calls sit only in the sketch: `esp_partition_find_first`, `esp_partition_mmap`, `heap_caps_malloc` with PSRAM, `xTaskCreatePinnedToCore` with task notifications, `esp_timer_get_time`. All exist in core 2.0.17; only the mapping handle type and enum have other names there (`spi_flash_mmap_handle_t`, `SPI_FLASH_MMAP_DATA`) **[read]**. No watchdog calls, no Wi-Fi **[read]**.
- The reference numbers were taken with GCC 14 at `-O3` and depend on code generation: inlining one function changed 95.0 into 155.2 ms per token **[reference]**. Speed with GCC 8.4 is unknown until measured.

What a newer core would change for us:

- The official PlatformIO platform provides core 3.x from `espressif32@8.0.0` (Arduino 3.3.12, ESP-IDF 5.5.5); no platform release bundles exactly 3.3.10 **[web]**.
- M5Unified 0.2.23 selects the new I2S driver when the core has it, so microphone and speaker would run through different driver code **[read]**. An open ESP-IDF issue (18621) reports silent microphone samples with M5Unified on IDF 5.5 **[web, not checked for our version]**.
- `shared/include/espnow_transport.h` uses only calls that keep their signature **[assumed from the migration guide]**. The receive callback in `apps/node/src/main.cpp` has another signature in core 3.x and would have to change if the Node moved **[read / web]**. The Mainframe registers no ESP-NOW callback **[read]**.
- The Node can stay on the old core, since every env pins its own platform **[read]**. If it moved, FastLED would switch to its other RMT driver **[web]**.
- We use no hardware timer, LEDC or RMT call directly **[read]**.
- Firmware size grows by roughly 10 % (one report for another sketch) **[web]**.

Benchmark firmware:

- A minimal app (`apps/slm-bench/src/`) that maps the image, runs the kernel from `apps/mainframe/src/slm/` on one core for a fixed input and prints time per output piece and the output pieces. No audio, eye, radio or sensors.
- Two envs with the model partition table of Decision 3: `slm-bench-core2` (`espressif32@6.13.0`) and `slm-bench-core3` (`espressif32@8.0.0`). The second pin is a new dependency and is added as its own step under the Dependencies rule of `CLAUDE.md`; it changes no existing env. Installing that platform downloads a new toolchain and is confirmed with the user at that step.
- Each core is measured at `-Os` and at `-O3` for the kernel, with the Barista image, giving four numbers. Output pieces must equal the host reference output on both cores.
- The benchmark gives the pure compute difference. It does not show how audio, radio and display behave on the newer core; only a move of the whole firmware would.

Estimate given to the user before the decision, not a measurement: a measurable gain on the newer core is more likely than not, a gain above about 30 % is unlikely, because the kernel uses no vector instructions on either core and the optimisation level can be raised on the old core too.

Consequences for this change:

- The core question is put to the user again in task 5.6 with the benchmark numbers. If the user then wants the whole firmware moved, that is a separate change, and the baseline (`reduce-panel-frame-rate`, task 2.4) is measured again on the new core before any model measurement in the full firmware.

### 9. Measurement method

- Instrument: the loop-load report from `claude/loop-load-measurement-f3ac43` (`-DDECAFLASH_LOOP_LOAD=1`), extended in the measurement build by: time per output piece (average / maximum), pieces per second, PSRAM and internal RAM free / minimum, model task stack high-water mark, and ESP-NOW send results (count, failures, largest lateness of a clock packet against its planned send time).
- `mainframe-slm-capture` extends the normal build with `DECAFLASH_LOOP_LOAD` and `DECAFLASH_SLM`. Per `CLAUDE.md`, measure with audio analysis running and flash the normal `mainframe` build afterwards.
- Reading the serial port restarts the Mainframe and is asked for before each capture.

## Risks / Trade-offs

Risks that only a device measurement can settle:

- [Flash cache and PSRAM contention] Weights read through the flash mapping and the KV cache in PSRAM share cache and memory bus with the firmware's own code and with the eye's off-screen buffer. Cache misses in the model can slow `loop()` on the other core even though it computes on core 0. → Measured in group 5 against the baseline; smaller model sizes in group 7 show how it scales.
- [Radio timing on core 0] The model task is below the radio stack, but each yield point bounds how long lower-level radio housekeeping can be delayed, and cache contention is not solved by priorities. → Send failures and clock packet lateness on the Mainframe plus a V2 Node check (task 6.5).
- [The 94 ms audio gap with text] Already close to trouble today; backlog reached 6 of 16 blocks. Any extra load on core 1 (worker of step two, cache effects) can turn it into drops. → Frame rate reduction first (`reduce-panel-frame-rate`), then model; the worker on core 1 is last and optional.
- [Watchdog] A long uninterrupted matrix product on core 0 starves the idle task. → Yield per layer; a 10-minute run per state without reset is an acceptance criterion.
- [Speed on one core] About 105 ms per piece is **[computed]** from the reference; on our board, with a busy radio core and an older toolchain, it may be much slower. → Measured in group 5; this number decides how a later change designs the text stock.
- [Port does not build on core 2.0.17] The kernel compiles with the pinned toolchain (see the notes on 5.1). The sketch-side calls (partition mapping, PSRAM, task) are still unbuilt. → The first device build in 5.4 and 6.1 shows it; a failed port is reported, not worked around.
- [Image does not fit] Spare room in the model partition is 92–315 KB **[computed]**. → Check the file size before flashing; a later own model can be sized to fit.
- [Wrong partition table left on the device] After a measurement the device has no second app slot. → Flashing `mainframe` restores `default_8MB.csv`; this is a tracked task.

Trade-off accepted: the runtime ships in no product build yet, so this change adds code that only a measurement env compiles.

## Migration Plan

- Normal build: unchanged by the prototype.
- Measurement build: flash `mainframe-slm-capture`, write the model image to 0x340000, measure, flash `mainframe` again.

## Open Questions

- Arduino core (Decision 8): the firmware stays on 2.0.17; whether it moves later is asked again in task 5.6 with the benchmark numbers of both cores.
- Follow-up changes, each needing its own interview, none decided here: which panel content the model takes over and what stays a fixed list; text stock versus generating after the event and what happens when no text is ready; the state fed to the model; language and tone; source of training data (generating it with a large model is a paid external service and needs the user's explicit yes); whether the model partition and runtime move into the normal build.

## Tasks

Device for all device tasks: the Mainframe (AtomS3R with Atomic Voice Base and Unit TMOS PIR). Flashing is approved through the upload command's permission prompt; ask before opening the serial port. Record every device result in `docs/project-context.md` with date, build and state, separate from host results. Numbering continues from the original plan: groups 1 and 2 are the tasks of `reduce-panel-frame-rate` and come first, group 3 (Arduino core decision) is done.

### 4. Partition table and measurement build

- [x] 4.1 Add the partition table of Decision 3 (`apps/mainframe/partitions_slm.csv`) and the env `mainframe-slm-capture` (extends `mainframe`, `-DDECAFLASH_LOOP_LOAD=1 -DDECAFLASH_SLM=1`) to `platformio.ini`, with a comment that it is measurement only; add the env to `README.md`; verify `pio run -e mainframe-slm-capture` builds, the firmware fits app0, and `pio run -e mainframe` still uses `default_8MB.csv`.
- [ ] 4.2 Device test (Mainframe, `mainframe-slm-capture`, model partition erased, runtime not yet present): verify it boots, the show runs, and the three-state capture stays within the limits of Decision 6 against the baseline of `reduce-panel-frame-rate` task 2.4 (measures the partition table change alone).

### 5. Runtime kernel (host-tested) and core comparison

- [x] 5.1 Port the reference inference kernel into `apps/mainframe/src/slm/` behind `DECAFLASH_SLM`, hardware-independent, with image header check and the licence notice; verify with a new host test `tests/slm_kernel.cpp` + run script, added to `tests/run_all.sh`: a tiny fixed-seed image gives the expected output pieces, and a wrong magic, version or size is rejected.
- [x] 5.2 Add the host script that writes random-weight images for given dimensions and seed; verify the host test loads an image written by it and that the same seed gives the same file.
- [x] 5.3 Ask the user for a yes to download the Barista image (state file name, source URL and size); after a yes, store it in the ignored local folder, verify its size fits the model partition, and verify the host test produces sensible English output pieces for a fixed input and record them as the reference output.
- [x] 5.4 Add the benchmark app `apps/slm-bench/src/` (maps the image, runs the kernel on one core for the fixed input of 5.3, prints output pieces and time per piece) and the env `slm-bench-core2` on `espressif32@6.13.0` with the partition table of 4.1 and a build option for `-Os` or `-O3` on the kernel; add it to `README.md` as measurement only; verify `pio run -e slm-bench-core2` builds and `pio run -e mainframe` is unchanged.
- [x] 5.5 As its own step under the Dependencies rule: ask the user for a yes to install `espressif32@8.0.0` (state source and download size), then add the envs `slm-bench-core3` and `slm-bench-core3-o3` with that pin; verify `pio run -e slm-bench-core3` builds and that `pio run -e mainframe -e node` still build with `espressif32@6.13.0`.
- [ ] 5.6 Device test (Mainframe, Barista image at 0x340000, `slm-bench-core2`, `slm-bench-core2-o3`, `slm-bench-core3`, `slm-bench-core3-o3`): verify the output pieces equal the host reference output of 5.3 in all four builds and record time per piece in one table; flash `mainframe` afterwards. Present the table to the user and ask in the question dialog whether the firmware stays on core 2.0.17 or a separate change moves it; record the answer in Decision 8. If the firmware is to move, do that change first and repeat that baseline on the new core before group 6.

Notes on 5.1 and 5.2 (2026-11-17, host only, nothing on the device):

- `apps/mainframe/src/slm/llm.h` is the reference file unchanged; `slm_kernel.h` wraps it with a bounds-checked image check, allocation through caller-supplied allocators, and reset / step with greedy choice. Host test: `tests/run_slm_kernel.sh`. Its expected output pieces were recorded from this implementation and are a regression value, not an independent reference; correctness against the reference comes with the Barista image in 5.3.
- A translation unit including the kernel compiles without warnings with the pinned device toolchain (xtensa GCC 8.4.0, gnu++11) at `-Os` and `-O3`. It is not linked into any firmware yet. The kernel object and its quantisation buffer take about 12.8 KB of static internal RAM.
- Image tool: `apps/mainframe/tools/slm_make_image.cpp`. A random image of the reference dimensions with group 32 and a separate output head is 5,017,432 bytes and does not fit the model partition (4,915,200). The group size and head layout of the Barista image (4,600,186 bytes) are read from its header in 5.3 and used for the random images of group 7.

Notes on 5.3 (2026-11-17, host only):

- Downloaded with the user's yes from `https://huggingface.co/slvDev/esp32-ai-barista` into the ignored folder `models/slm/barista/`: `model.bin` (4,600,186 bytes), `tokenizer.json`, `vocab.json`, `layout.json`. All four SHA-256 sums equal the values pinned in the reference repo's `scripts/fetch_model.sh`. The image fits the model partition (4,915,200 bytes) with 315,014 bytes spare.
- Image header: format version 1, separate output head (flags 0), 8057 input tokens, 854 output classes, dimension 128, 6 layers, 4 heads, FFN 384, PLE dimension 128, 128 positions, group 128.
- Host check: `python3 apps/mainframe/tools/slm_barista.py models/slm/barista "<question>"` encodes the question, runs the kernel through `apps/mainframe/tools/slm_generate.cpp` and prints the answer. The tokenizer in that script is a small ASCII re-implementation and was not compared with the reference tokenizer; the fixed ids below are what the device comparison uses.
- Fixed input for device comparisons, question "why is my coffee bitter": prompt ids `3127 402 610 2021 1137`, then the input id of `<bos>`, `7339`. Output classes are fed back through `out2in` from `layout.json`; generation stops at class 2 (`<eos>`).
- Reference output of the host (28 pieces): `155 331 645 261 46 4 72 9 310 5 155 164 261 304 4 137 123 160 5 376 215 339 103 386 660 259 403 8`, which reads "if the cup reads bitter, coarsen a step. if it reads sour, go finer instead. which of those does your drink read as?" Two other questions also gave sensible English.
- Random images for group 7 use the same header layout (separate head, 128 positions, group equal to the dimension). With those settings the reference size is 4,600,186 bytes like Barista; 4 layers × dimension 96 gives 2,369,710 bytes and 3 layers × dimension 64 gives 1,232,208 bytes.

Notes on 5.4 (2026-11-17, built only, not flashed):

- `apps/slm-bench/src/main.cpp` with the envs `slm-bench-core2` (`-Os`) and `slm-bench-core2-o3` (everything at `-O3`). Both build on `espressif32@6.13.0`: 281,617 and 295,389 bytes of flash. This is the first build of the sketch-side calls (partition mapping, PSRAM allocation) on the pinned core; whether they work is a device question.
- The fixed prompt, the class table and the host's output classes travel in a prompt block appended to the model image (`apps/mainframe/src/slm/slm_prompt.h`). `python3 apps/mainframe/tools/slm_barista.py models/slm/barista "why is my coffee bitter" models/slm/barista_bitter.bin` writes the file to flash (4,603,762 bytes). The benchmark prints `match=yes` when its output classes equal the host's.
- Writing the model to the device: `python3 ~/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port <PORT> write_flash 0x340000 models/slm/barista_bitter.bin`. This is a flash command of its own and is approved through its permission prompt.
- The benchmark runs the kernel in `loop()` on core 1 with nothing else running, repeats every two seconds and reports milliseconds per forward pass and per output piece.

Device results for both cores (5.6; Mainframe, 2026-11-17, Barista image with prompt block at 0x340000, kernel alone in `loop()` on one core at 240 MHz, three to six runs per build, all runs of a build within 0.2 ms of each other) **[measured]**:

| Build | Core / compiler | Optimisation | ms per forward pass | ms per output piece while generating | ms per piece incl. prompt | Output equals host |
|---|---|---|---|---|---|---|
| `slm-bench-core2` | Arduino 2.0.17 / GCC 8.4.0 | `-Os` | 144.65 | 145.20 | 175.64 | yes |
| `slm-bench-core2-o3` | Arduino 2.0.17 / GCC 8.4.0 | `-O3` | 86.10 | 86.57 | 104.56 | yes |
| `slm-bench-core3` | Arduino 3.3.12 / GCC 14.2.0 | `-Os` | 166.31 | 166.90 | 201.94 | yes |
| `slm-bench-core3-o3` | Arduino 3.3.12 / GCC 14.2.0 | `-O3` | 86.19 | 86.62 | 104.66 | yes |

- The port works on the pinned core: partition mapping, PSRAM allocation and the kernel run, and the 28 output pieces equal the host reference output in every run of both builds.
- Memory with the model loaded (`-O3` build; the `-Os` capture started after that line was printed): 2,521,912 bytes of PSRAM and 31,084 bytes of internal RAM taken; 5.86 MB PSRAM and 326 KB internal RAM free in this firmware, which contains nothing else.
- Comparison with the reference **[reference / computed]**: 49.6 ms per forward pass on two cores, 1.75× faster than one, so about 86.8 ms on one core; 60.2 ms per piece on two cores, so about 105 ms on one. Core 2.0.17 at `-O3` gives 86.10 and 104.56. `-O3` here applies to the whole benchmark build, as in the reference; the Mainframe firmware builds with `-Os`.
- Core comparison: at `-O3` both cores take the same time (86.10 and 86.19 ms per forward pass). At `-Os` the newer core is slower (166.31 against 144.65 ms). The newer core brings no speed gain for this kernel; the optimisation level decides.
- Memory on the newer core: 2,562,232 bytes of PSRAM and 31,732 bytes of internal RAM taken; 300 KB internal RAM free against 326 KB on the old core.
- Installing `espressif32@8.0.0` grew `~/.platformio` from 2.4 GB to 7.7 GB. `mainframe` and `node` still build on `espressif32@6.13.0`.
- Not measured: anything inside the Mainframe firmware, any state with audio, eye or radio running.
- After each measurement session the normal `mainframe` build was flashed; it boots and the partition table on the device is `default_8MB.csv` again. The model image written to 0x340000 lies in the unused second app slot and SPIFFS area of that table.

### 6. Model task on core 0

- [ ] 6.1 Add partition mapping, PSRAM working memory and the core 0 task as in Decisions 4 and 5, plus the report fields of Decision 9; verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-slm-capture` builds, and that `mainframe` contains no runtime symbols.
- [ ] 6.2 Device test (Mainframe, `mainframe-slm-capture`, erased model partition): verify the serial line reports "no model", no generation starts and the show runs normally.
- [ ] 6.3 Device test (Mainframe, `mainframe-slm-capture`, Barista image at 0x340000): verify the device output pieces equal the host reference output of 5.3; record time per piece on core 0 while idle, PSRAM and internal RAM use, and that PSRAM use grew by less than the image size while internal RAM stayed far below it.
- [ ] 6.4 Device test (same build and image, model generating continuously): capture the three states for at least 10 minutes each, directly after a fresh baseline with the model task disabled in the same session; verify every limit of Decision 6, including ESP-NOW send failures and clock packet lateness. Report each limit as held or missed with the numbers; do not name a cause without a measurement.
- [ ] 6.5 Device test with a flashed V2 Node (the user dropped the separate end-to-end Node test as a precondition on 2026-11-17: the Node runs as far as they observed; no measurement recorded): first without the model, verify start, scene change and beat timing on the Node as the comparison; then with the model generating, verify start, scene change and beat timing on the Node look the same as without the model, and record what was observed.

### 7. Model sizes and second core

- [ ] 7.1 Device test (Mainframe, `mainframe-slm-capture`, random-weight images of the three sizes in Decision 7, core 0 only): record time per piece, PSRAM use and the Decision 6 limits per size in one table.
- [ ] 7.2 Add the optional worker on core 1 below `loop()` behind a second build flag; verify `sh tests/run_all.sh` passes, both builds succeed, and the host/device output is unchanged.
- [ ] 7.3 Device test (same build with the core 1 worker, reference-size image): repeat 6.4 after a fresh baseline; verify the Decision 6 limits and record the speed-up against core 0 alone. If any limit is missed, leave the worker disabled and say so.

### 8. Wrap-up

- [ ] 8.1 Write the results into `docs/project-context.md`: one table per step with before and after, each number marked as measured, and a list of what was not verified on hardware; verify the README env list matches `platformio.ini`.
- [ ] 8.2 Flash the normal `mainframe` build and verify on the device that it boots with `default_8MB.csv` and the show runs; run `sh tests/run_all.sh`, `pio run -e mainframe -e mainframe-loop-load -e mainframe-slm-capture -e slm-bench-core2 -e slm-bench-core3`.
- [ ] 8.3 Present the numbers to the user and interview them on the follow-up change (Open Questions above); verify the decisions are recorded in `ideas.md` or, if the model is to enter the product, in a new OpenSpec change.
