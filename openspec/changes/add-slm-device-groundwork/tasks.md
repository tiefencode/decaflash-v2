# Tasks

Device for all device tasks: the Mainframe (AtomS3R with Atomic Voice Base and Unit TMOS PIR). Flashing is approved through the upload command's permission prompt; ask before opening the serial port. Record every device result in `docs/project-context.md` with date, build and state, separate from host results.

## 1. Measurement instrument and baseline

- [ ] 1.1 Ask the user whether branch `claude/loop-load-measurement-f3ac43` may be merged into this branch; after a yes, merge it and verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-loop-load` builds.
- [ ] 1.2 Device test (Mainframe, `mainframe-loop-load`): capture the baseline in the three states of design.md Decision 6 (50 s idle, 80 s show with music, 95 s text phase) and verify the report gives busy share, eye frame time, largest audio and IMU gaps and audio drops per state; store the numbers as "baseline before frame-rate step".

## 2. Reduced frame rate under the message panel

- [ ] 2.1 Make the frame interval in `apps/mainframe/src/eye_renderer.cpp` depend on whether the panel is drawn in this frame (not during sleep or debug screens), with the interval under the panel as one constant; verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-loop-load` builds.
- [ ] 2.2 Device test (Mainframe, `mainframe-loop-load`): measure the text phase with 25 ms, 40 ms and 50 ms under the panel, same music and durations as 1.2; verify a table with busy share, frames/s, largest audio gap and largest IMU gap per candidate exists, and that the state without text is unchanged against 1.2.
- [ ] 2.3 Show the user the table and the look on the device and let them pick the interval in the question dialog; set the constant, record value and reasoning in design.md Decision 2, and verify on the device (`mainframe` build) that a message during sleep and during a debug screen keeps the normal rate. If the user picks none, remove the eye-iris delta and revert 2.1.
- [ ] 2.4 Device test (Mainframe, `mainframe-loop-load`, chosen interval): capture all three states again as "baseline after frame-rate step" and verify 0 audio drops; then flash `mainframe`.

## 3. Arduino core decision (blocks groups 4 to 8)

- [ ] 3.1 Hold the separate session on the Arduino core (design.md Decision 8) and record the user's decision in design.md; verify Decision 8 no longer says "open". If the core is to change, do that as its own change first and repeat 2.4 on the new core before continuing here.

## 4. Partition table and measurement build

- [ ] 4.1 Add the partition table of design.md Decision 3 and the env `mainframe-slm-capture` (extends `mainframe`, `-DDECAFLASH_LOOP_LOAD=1 -DDECAFLASH_SLM=1`) to `platformio.ini`, with a comment that it is measurement only; add the env to `README.md`; verify `pio run -e mainframe-slm-capture` builds, the firmware fits app0, and `pio run -e mainframe` still uses `default_8MB.csv`.
- [ ] 4.2 Device test (Mainframe, `mainframe-slm-capture`, model partition erased, runtime not yet present): verify it boots, the show runs, and the three-state capture stays within the limits of design.md Decision 6 against 2.4 (measures the partition table change alone).

## 5. Runtime kernel (host-tested)

- [ ] 5.1 Port the reference inference kernel into `apps/mainframe/src/slm/` behind `DECAFLASH_SLM`, hardware-independent, with image header check and the licence notice; verify with a new host test `tests/slm_kernel.cpp` + run script, added to `tests/run_all.sh`: a tiny fixed-seed image gives the expected output pieces, and a wrong magic, version or size is rejected.
- [ ] 5.2 Add the host script that writes random-weight images for given dimensions and seed; verify the host test loads an image written by it and that the same seed gives the same file.
- [ ] 5.3 Ask the user for a yes to download the Barista image (state file name, source URL and size); after a yes, store it in the ignored local folder, verify its size fits the model partition, and verify the host test produces sensible English output pieces for a fixed input and record them as the reference output.

## 6. Model task on core 0

- [ ] 6.1 Add partition mapping, PSRAM working memory and the core 0 task as in design.md Decisions 4 and 5, plus the report fields of Decision 9; verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-slm-capture` builds, and that `mainframe` contains no runtime symbols.
- [ ] 6.2 Device test (Mainframe, `mainframe-slm-capture`, erased model partition): verify the serial line reports "no model", no generation starts and the show runs normally.
- [ ] 6.3 Device test (Mainframe, `mainframe-slm-capture`, Barista image at 0x340000): verify the device output pieces equal the host reference output of 5.3; record time per piece on core 0 while idle, PSRAM and internal RAM use, and that RAM use did not grow by the image size.
- [ ] 6.4 Device test (same build and image, model generating continuously): capture the three states for at least 10 minutes each, directly after a fresh baseline with the model task disabled in the same session; verify every limit of design.md Decision 6, including ESP-NOW send failures and clock packet lateness. Report each limit as held or missed with the numbers; do not name a cause without a measurement.
- [ ] 6.5 Device test with a flashed V2 Node (only after the end-to-end Node test required by `CLAUDE.md` has passed): with the model generating, verify start, scene change and beat timing on the Node look the same as without the model, and record what was observed.

## 7. Model sizes and second core

- [ ] 7.1 Device test (Mainframe, `mainframe-slm-capture`, random-weight images of the three sizes in design.md Decision 7, core 0 only): record time per piece, PSRAM use and the Decision 6 limits per size in one table.
- [ ] 7.2 Add the optional worker on core 1 below `loop()` behind a second build flag; verify `sh tests/run_all.sh` passes, both builds succeed, and the host/device output is unchanged.
- [ ] 7.3 Device test (same build with the core 1 worker, reference-size image): repeat 6.4 after a fresh baseline; verify the Decision 6 limits and record the speed-up against core 0 alone. If any limit is missed, leave the worker disabled and say so.

## 8. Wrap-up

- [ ] 8.1 Write the results into `docs/project-context.md`: one table per step with before and after, each number marked as measured, and a list of what was not verified on hardware; verify the README env list matches `platformio.ini`.
- [ ] 8.2 Flash the normal `mainframe` build and verify on the device that it boots with `default_8MB.csv` and the show runs; run `sh tests/run_all.sh`, `pio run -e mainframe -e mainframe-loop-load -e mainframe-slm-capture` and `openspec validate add-slm-device-groundwork --strict`.
- [ ] 8.3 Present the numbers to the user and interview them on the follow-up change (design.md, Open Questions); verify the decisions are recorded in `ideas.md` or a new change.

## Workflow follow-up

- Archive the change after the device tests and the user's review.
- Merge into `main` only with the user's agreement.
