# Tasks

## 1. Dependency

- [x] 1.1 Add `sparkfun/SparkFun STHS34PF80 Arduino Library@1.0.5` to `lib_deps` of `[env:mainframe]` in `platformio.ini` as its own commit; verify `pio run -e mainframe` builds.

## 2. Debug view state

- [x] 2.1 Add hardware-independent `apps/mainframe/src/debug_view.h` with views `Off`, `Mood`, `Presence`, `onHold()` and `onClick()` (click belongs to the show only in `Off`); verify with a new host test `tests/debug_view.cpp` + `tests/run_debug_view.sh` covering every transition from the debug-views spec.
- [x] 2.2 Wire the view into `main.cpp`: hold and click go through the view; show start and scene selection run only when the click belongs to the show; `EyeRenderer` gets the active view; verify `pio run -e mainframe` builds and the mood screen still appears for `Mood`.

## 3. Presence history and serial lines (host-tested)

- [x] 3.1 Add hardware-independent `apps/mainframe/src/presence_debug.{h,cpp}`: `PresenceSample`, a 120-entry history of presence and motion values, time since each flag changed (across `millis()` overflow), graph range covering 0, threshold, min and max, and invalid samples kept out of the history; verify with `tests/presence_debug.cpp` + `tests/run_presence_debug.sh`.
- [x] 3.2 Add the formatter for `TMOS`, `TMOS_CONFIG` and `TMOS_STATE` lines with the field order from the presence-sensing spec; verify exact strings, including `ambient` with one decimal and negative values, in the same host test.

## 4. Sensor driver (device only)

- [x] 4.1 Add `apps/mainframe/src/presence_sensor.{h,cpp}`: SparkFun subclass with read/write callbacks on `M5.Ex_I2C` (100 kHz), probe via `WHO_AM_I`, fixed configuration (AVG_TMOS 32, AVG_T 8, block data update, 8 Hz), read-back of thresholds and hystereses, data-ready poll every 25 ms with burst reads, error counting, return to "missing" after 8 consecutive failures, re-probe every 5 s; verify `pio run -e mainframe` builds.
- [x] 4.2 Call the sensor from `main.cpp` (begin in `setup()`, service in `loop()`), feed samples into the presence history and print `TMOS` lines only when `Serial.availableForWrite()` has room; confirm in code that no sample reaches personality, eye animation, light state or panel; verify `pio run -e mainframe` builds.

## 5. Presence debug screen

- [x] 5.1 Draw the presence screen in `eye_renderer.cpp` per design.md: sensor state line, three written-out flag rows with state and time, two graphs with threshold lines and titled values, bottom line with ambient shock value, raw object value and ambient °C; missing/failing sensor shows state and error count and no current values; verify `pio run -e mainframe` builds.

## 6. Documentation

- [x] 6.1 Update `openspec/config.yaml` (Unit TMOS PIR present), the hardware status and B.2/B.5 rows in `docs/project-context.md`, and the README status line; verify the text matches the code.

## 7. Host verification

- [ ] 7.1 Run `sh tests/run_all.sh` (all tests pass) and `pio run -e mainframe`; build the measurement environments that extend `mainframe` (`pio run -e mainframe-beat-tracker` and the other `mainframe-*` envs) to confirm the new dependency does not break them; run `npx openspec validate add-presence-sensor-debug --strict`.

## 8. Device test (needs the user's approval to flash)

- [ ] 8.1 Flash the `mainframe` build to the AtomS3R with the Unit TMOS PIR on the Grove port. Observe: serial shows `TMOS_CONFIG` with read-back thresholds and `TMOS` lines at about 8 per second; long press opens mood debug, short press switches to presence debug and back without scene change, long press returns to the eye; outside debug mode a short press still starts the show and changes scenes.
- [ ] 8.2 With audio analysis running, check that tempo, iris VU, IMU gestures and Node scene/clock keep working while the sensor samples; unplug and replug the sensor and confirm `TMOS_STATE sensor=missing`, "sensor missing" on screen and automatic recovery with `TMOS_STATE sensor=ok` without restart.
- [ ] 8.3 Read the serial lines live while the user performs the measurement cases step by step: nobody in front, one person standing still at 0.5/1/1.5 m for two minutes, moving or dancing in front, walking past sideways, arriving and leaving, waving a hand, people in the background at 3–4 m, a heat source in view. Note mounting and room. Keep raw readings only temporarily; summarise per case in design.md what the flags and values reliably distinguish. This summary is the input for the later presence-rules change.

## Workflow follow-up

- Archive the change after the device test and the user's review.
- Propose the presence-rules change (attention base value and further cases) based on the 8.3 measurements.
