# Tasks

## 1. Firmware

- [x] 1.1 In `apps/mainframe/src/personality.cpp`, raise loneliness in proportion to its distance to 100 with base rate 0.2 points/s (nobody) or 0.1 points/s (far zone) at 50, and lower it linearly at 0.1 points/s in the near zone, keeping the remainder stepping; verify with task 2.1.
- [x] 1.2 In `personality.cpp`, set the loneliness relief to Move 5, Tap 3, MultiTap 6, Impact 2, Rotate 4, Shake 5 and `ArrivedInFront` 5 (attention and annoyance unchanged); verify with task 2.1.

## 2. Host tests

- [x] 2.1 Update `tests/personality.cpp`: from 50, ten minutes in the far zone give 85, ten minutes alone give 95, one minute in the near zone lowers 50 to 44; each gesture and `ArrivedInFront` lowers loneliness by its new amount; 100 ms and finer steps give the same value. `sh tests/run_personality.sh` passes.
- [x] 2.2 Keep `tests/presence_decision.cpp` unchanged (near zone thresholds stay 2000 / 1600); `sh tests/run_presence_decision.sh` passes.
- [x] 2.3 Run `sh tests/run_all.sh` and `pio run -e mainframe`; both pass.

## 3. Specs and docs

- [x] 3.1 Update the "Current values" section of `openspec/specs/personality/spec.md` (loneliness rates by zone, Shake loneliness −5); `openspec validate tune-loneliness-presence --strict` passes.

## 4. Device test

- [ ] 4.1 Coordinate the single Mainframe with the user (another session may be using it), flash the normal `mainframe` build to the AtomS3R Mainframe, then about 3 minutes with the mood debug screen and the presence debug screen, one step at a time: (a) nobody in range for 1 minute, loneliness rises by about 12 near 50 (less when higher); (b) stand at about 1.5 m for 1 minute, the presence screen shows far and loneliness rises by about half as much; (c) step to panel reading distance, the presence screen shows near, `Kommt davor` comes once and lowers loneliness by 5, and it then falls by about 6 in 1 minute. Report what was observed on the device separately from the host results.
