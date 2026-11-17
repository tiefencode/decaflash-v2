# Tasks

Device for all device tasks: the Mainframe (AtomS3R with Atomic Voice Base and Unit TMOS PIR). Flashing is approved through the upload command's permission prompt; ask before opening the serial port. Record every device result in `docs/project-context.md` with date, build and state, separate from host results.

## 1. Measurement instrument and baseline

- [ ] 1.1 Ask the user whether branch `claude/loop-load-measurement-f3ac43` may be merged into this branch; after a yes, merge it and verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-loop-load` builds.
- [ ] 1.2 Device test (Mainframe, `mainframe-loop-load`): capture the baseline in the three states of design.md Decision 3 (50 s idle, 80 s show with music, 95 s text phase) and verify the report gives busy share, eye frame time, largest audio and IMU gaps and audio drops per state; store the numbers as "baseline before frame-rate step".

## 2. Reduced frame rate under the message panel

- [ ] 2.1 Make the frame interval in `apps/mainframe/src/eye_renderer.cpp` depend on whether the panel is drawn in this frame (not during sleep or debug screens), with the interval under the panel as one constant; verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-loop-load` builds.
- [ ] 2.2 Device test (Mainframe, `mainframe-loop-load`): measure the text phase with 25 ms, 40 ms and 50 ms under the panel, same music and durations as 1.2; verify a table with busy share, frames/s, largest audio gap and largest IMU gap per candidate exists, and that the state without text is unchanged against 1.2.
- [ ] 2.3 Show the user the table and the look on the device and let them pick the interval in the question dialog; set the constant, record value and reasoning in design.md Decision 2, and verify on the device (`mainframe` build) that a message during sleep and during a debug screen keeps the normal rate. If the user picks none, revert 2.1 and drop this change.
- [ ] 2.4 Device test (Mainframe, `mainframe-loop-load`, chosen interval): capture all three states again as "baseline after frame-rate step" and verify 0 audio drops; then flash `mainframe`.
- [ ] 2.5 Record the device results in `docs/project-context.md` and run `sh tests/run_all.sh`, `pio run -e mainframe -e mainframe-loop-load` and `openspec validate reduce-panel-frame-rate --strict`.

## Workflow follow-up

- Archive the change after the device tests and the user's review.
- Merge into `main` only with the user's agreement.
