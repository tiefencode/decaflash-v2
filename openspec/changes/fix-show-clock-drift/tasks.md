# Tasks

## 1. Clock scheduling

- [ ] 1.1 Add `takeDueShowBeat()` to `apps/mainframe/src/show_clock.h` and use it in a loop in `serviceShowClock()`; ClockSync only when the last counted beat is beat 1. Verify: `pio run -e mainframe` succeeds.
- [ ] 1.2 Extend `tests/show_clock.cpp`: late passes do not shift later beats, missed beats are all counted, ClockSync rule after a catch-up, millisecond wrap. Verify: the test fails with `now + interval` scheduling and passes with the fix.

## 2. Host verification

- [ ] 2.1 Run `sh tests/run_all.sh`; all tests pass.
- [ ] 2.2 Build `pio run -e mainframe`.

## 3. Device verification

- [ ] 3.1 Baseline on the Mainframe (AtomS3R) with the loop-load measurement build without the fix: record `beat_late_ms` and elapsed time over counted beats against beats × interval, show running without music, with and without a visible panel text.
- [ ] 3.2 Repeat 3.1 on the Mainframe with the measurement build plus the fix; the elapsed time over the counted beats matches beats × interval within the lateness of a single beat.
- [ ] 3.3 With flashed V2 Nodes, if available: observe that they follow show start, scene change and the clock with the fixed Mainframe. Record plainly if this was not checked.
- [ ] 3.4 Flash the normal `mainframe` build with the fix to the Mainframe and confirm it boots.

## 4. Records

- [ ] 4.1 Update BUG-028 in `bugs.md` with the measured before and after values; move it to "Closed" only if 3.2 shows the drift is gone.
