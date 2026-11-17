# Tasks

Device for all device tasks: the Mainframe (AtomS3R with Atomic Voice Base and Unit TMOS PIR). Flashing is approved through the upload command's permission prompt; ask before opening the serial port. Record every device result in `docs/project-context.md` with date, build and state, separate from host results.

## 1. Measurement instrument and baseline

- [x] 1.1 Ask the user whether branch `claude/loop-load-measurement-f3ac43` may be merged into this branch; after a yes, merge it and verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-loop-load` builds. (Obsolete: the instrumentation was already on `main`.)
- [x] 1.2 Device test (Mainframe, `mainframe-loop-load`): capture the baseline in the three states of design.md Decision 3 (50 s idle, 80 s show with music, 95 s text phase) and verify the report gives busy share, eye frame time, largest audio and IMU gaps and audio drops per state; store the numbers as "baseline before frame-rate step". (Done inside the sweep capture of 2.2: its 25 ms rows are the same-session baseline, see design.md Decision 3.)

## 2. Reduced frame rate under the message panel

- [x] 2.1 Make the frame interval in `apps/mainframe/src/eye_renderer.cpp` depend on whether the panel is drawn in this frame (not during sleep or debug screens), with the interval under the panel as one constant; verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-loop-load` builds.
- [x] 2.2 Device test (Mainframe, `mainframe-panel-rate-sweep`): measure the text phase with 25, 40, 50 and 67 ms under the panel in one capture with the same music; verify a table with busy share, frames/s, largest audio gap and largest IMU gap per candidate exists, and that the state without text is unchanged against 1.2.
- [x] 2.3 Show the user the table and the look on the device and let them pick the interval in the question dialog; set the constant, record value and reasoning in design.md Decision 2, and verify on the device (`mainframe-loop-load` build) that a message during sleep and during a debug screen keeps the normal rate. (Chosen: 67 ms. Debug screen verified on the device; sleep verified by the host test `tests/frame_interval.cpp` only.) If the user picks none, revert 2.1 and drop this change.
- [x] 2.4 Device test (Mainframe, `mainframe-loop-load`, chosen interval): capture all three states again as "baseline after frame-rate step" and verify 0 audio drops; then flash `mainframe`.
- [x] 2.5 Record the device results in `docs/project-context.md` and run `sh tests/run_all.sh`, `pio run -e mainframe -e mainframe-loop-load` and `openspec validate reduce-panel-frame-rate --strict`.

## Workflow follow-up

- Archive the change after the device tests and the user's review.
- On sync, add the panel interval (67 ms, `kPanelFrameIntervalMs` in `frame_interval.h`) to the calibration line of `openspec/specs/eye-iris/spec.md`, which today names only the 25 ms interval.
- Merge into `main` only with the user's agreement.
