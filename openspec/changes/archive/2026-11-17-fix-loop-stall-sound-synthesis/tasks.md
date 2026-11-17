# Tasks

## 1. Measurement

- [x] 1.1 Time each call between the IMU block and the eye in the loop-load build and capture the stalls on the Mainframe (AtomS3R) in creature mode. Result: the stall is the phrase synthesis inside `playMoodSound`.

## 2. Sound preparation

- [x] 2.1 Move the synthesis into `apps/mainframe/src/sound_synth.{h,cpp}` with a renderer that works in pieces and a bank that holds all ten sounds. Verify: host comparison of old and new output, byte-identical for all sounds and the start sound.
- [x] 2.2 Prepare the bank from `loop()` within a time budget per pass while the analysis is not requested; play phrases and the snore from the bank and drop a sound that is not ready.
- [x] 2.3 Host test `tests/sound_synth.cpp`: lengths, same bytes for any piece size, bank order, readiness and bounds.

## 3. Host verification

- [x] 3.1 Run `sh tests/run_all.sh`; all tests pass.
- [x] 3.2 Build `pio run -e mainframe` and the loop-load measurement environments.

## 4. Device verification

- [x] 4.1 Baseline on the Mainframe with the loop-load build without the fix, creature mode, 360 s.
- [x] 4.2 Same capture with the fix; compare the longest loop pass and the largest IMU gap.
- [x] 4.3 Flash the normal `mainframe` build with the fix and confirm it boots.
- [ ] 4.4 Not checked on 2026-11-17: a snore while sleeping, the phrases other than attention UP and loneliness UP, and a sound that becomes due before it is prepared.

## 5. Records

- [x] 5.1 Update BUG-029 in `bugs.md` with the measured values.
