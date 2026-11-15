# Design

## Context

See proposal.md for the motivation. Constraints from the current code:

- `apps/mainframe/src/main.cpp` runs everything in one cooperative loop with `delay(5)`: audio, tempo, show clock, mood update every 100 ms, IMU every 20 ms, eye frame every 33 ms. The sensor has to fit into that loop without a task of its own.
- The front button uses M5Unified with a hold threshold of 1200 ms. Today `wasHold()` toggles `moodDebug` and `wasClicked()` starts the show or selects the next scene, also while the debug overlay is visible.
- `EyeRenderer::service()` draws the mood overlay with font 1 (6×8 px, 21 columns) into the 128×128 canvas, keeps the beat dot at x = 116 free, and skips the message panel while the overlay is visible.
- I²C on the AtomS3R with M5Unified 0.2.23: `In_I2C` uses controller 1 on G45/G0 (IMU), and the Voice Base codec is reached by temporarily switching controller 1 to G38/G39. `Ex_I2C` is assigned to controller 0 on the Grove pins G2 (SDA) / G1 (SCL) but not started.
- The sensor is the M5Stack Unit TMOS PIR (U185) with the ST STHS34PF80, address `0x5A`, device ID `0xD3`. Despite the product name it is not a pyroelectric PIR: it measures thermal infrared and can report a person standing still.

## Goals / Non-Goals

**Goals:**
- Read every output the sensor offers, at a rate high enough to see short motion, with negligible load on the loop.
- Keep all logic that does not touch hardware (history, flag timing, serial line formatting, debug view state) in host-testable code, like `motion_events`.
- Make the presence screen readable without a legend.

**Non-Goals:**
- No presence rules, filtering into events, or effect on personality (later change, based on measurements).
- No runtime change of sensor thresholds, sample rate or filters (possible later step via serial commands).
- No use of the sensor's interrupt pin; the Grove cable carries only I²C and power.

## Decisions

### Library: SparkFun STHS34PF80 1.0.5, driven over M5Unified's `Ex_I2C`

The SparkFun library wraps ST's reference register driver (`sths34pf80_reg.c`) and is maintained (1.0.5, May 2026). Its base class `STHS34PF80` takes read/write callbacks, so a small subclass can route them through `M5.Ex_I2C` instead of Arduino `Wire`.

- Alternative `m5stack/M5-STHS34PF80` 0.0.1 (2024): single early release, same origin; less maintained.
- Alternative `adafruit/Adafruit STHS34PF80` 1.0.2: also fine, but pulls in Adafruit BusIO.
- Alternative own register driver: avoids the dependency, but the embedded-function registers (thresholds, algorithm reset) need a paged access sequence that ST's driver already implements.
- Why not Arduino `Wire`: `Wire` also uses controller 0. Two drivers on one controller (M5Unified's and Arduino's) risk conflicting state; using `Ex_I2C` keeps one driver per controller. Bus clock 100 kHz (Grove cable length, standard mode).

### Sampling: poll data-ready, burst-read the outputs

- Fixed configuration: object averaging `AVG_TMOS = 32` and ambient averaging `AVG_T = 8` (library defaults), block data update on, sample rate (ODR) 8 Hz. With `AVG_TMOS = 32` ST allows up to 30 Hz; 8 Hz is a starting value that shows hand movement while keeping noise low. To be confirmed by measurement.
- The loop checks the data-ready flag every 25 ms. When set, it reads `FUNC_STATUS` (flags; the read also clears data-ready), then two burst reads: `0x26..0x29` (object raw, ambient) and `0x38..0x3F` (object compensated, presence, motion, ambient shock). About four short transactions per sample, well under 2 ms at 100 kHz, 8 times per second.
- Ambient temperature is converted with 100 LSB/°C (datasheet). The object values are shown raw: the 2000 LSB/°C object sensitivity describes a signal relative to the sensor, not an absolute temperature, so a °C label would mislead.
- The sensor's detection thresholds and hystereses stay at their power-on values. They are read back once after configuration and shown on screen and in the `TMOS_CONFIG` line, so we see the real values instead of assuming datasheet defaults.
- Order matters: in ST's driver every access to the embedded-function registers (thresholds, algorithm reset) powers a running sensor down and resets its algorithm. So the firmware uses its own sequence instead of SparkFun's `begin()` (which starts at 1 Hz): identity check, reboot, averaging, block data update and threshold read-back while the sensor is still powered down, and switching to 8 Hz as the last step.

### Missing sensor and recovery

- Start-up probes `WHO_AM_I`. Missing → state "missing", re-probe every 5 s. A probe of an absent device is one NACKed transaction.
- After a successful start, a failed read marks the sample invalid and increments an error counter. After 8 consecutive failures the state returns to "missing" and the re-probe path takes over (covers a pulled cable).
- Configuration after a re-probe uses the same code as start-up.

### Code structure

- `presence_sensor.{h,cpp}` (device only): the SparkFun subclass on `Ex_I2C`, probe, configuration, polling, and the latest `PresenceSample`.
- `presence_debug.{h,cpp}` (hardware-independent, host-tested): ring history of presence and motion values (15 s at 8 Hz = 120 samples, one per pixel column), time since each flag changed (with `millis()` overflow), min/max for the graph scale, and the formatter for the `TMOS`, `TMOS_CONFIG` and `TMOS_STATE` lines.
- `debug_view.h` (hardware-independent, host-tested): a three-state view (`Off`, `Mood`, `Presence`) with `onHold()` and `onClick()`. `onClick()` reports whether the click belongs to the show (only in `Off`), so `main.cpp` keeps the show logic where it is.
- `EyeRenderer::service()` gets the active view and a pointer to the presence debug data; drawing of the presence screen lives next to the mood overlay.

### Presence screen layout (128×128, font 1)

The presence screen clears the whole display, so the eye does not shine through around the text (seen on the first device test as a white band and line). Three flag rows at the top, each `name  ON/off  time` (e.g. `ambient shock off 42s`, time as `s` below 100 s, then `m`), with ON highlighted in green. Two graphs of 34 px height without titles: the presence graph is cyan like the `presence` label, the motion graph magenta like the `motion` label; the threshold is a dashed yellow line, and the y-range covers 0, the threshold and the window's min/max, so the threshold is always visible. Below them `heat +0.49C`, and a bottom line `sensor 23.5C`, or `sensor error N` after a failed read. `heat` and `sensor` do not fit side by side in 20 columns of font 1, so they keep one line each. A missing sensor gets a full-screen notice with the error count. Labels are English, like the existing mood screen.

First device test (2026-11-15): the first layout was too crowded for the 128 px screen. Removed after the user's review: the `sensor ok` line, the raw `shock` number (still in the serial lines) and the threshold number in the graph titles. The user wants a readable heat signal instead of the raw `object` count, plus the sensor's own temperature. Second review on the device (same day): the graph titles and the error count were removed as not needed (serial still has both), and the mood screen now also clears the whole display; it redraws the beat dot on black.

### Heat signal on screen

- `heat` = object signal / sensitivity, in °C with two decimals: how much warmer the field of view is than the sensor itself, averaged over the whole cone of about 80°. A person at 1 m fills only part of the cone, so the value moves by tenths of a degree, not towards body temperature.
- The sensitivity is read from the sensor's `SENS_DATA` register (value = SENS_DATA × 16 + 2048 counts/°C, per ST's driver; datasheet default 2000) and printed as `sensitivity` in `TMOS_CONFIG`, instead of hard-coding 2000.
- `sensor` is the ambient register, which is the sensor's own temperature; the serial field keeps its name `ambient`.
- Whether the conversion matches reality is unverified; a rough device check is a warm cup or a hand directly in front of the sensor.
- Rejected: `view ≈ sensor + heat` as an absolute scene temperature, because it suggests a precision the sensor does not have.

### Serial lines for live reading

- Format and field meanings are in the presence-sensing spec. `name=value` lines like the existing `BEAT_V2` telemetry instead of CSV: each line describes itself, so a monitor attached at any time can read it and no header line is needed.
- Measurements are read live: the AtomS3R hangs on USB at the Mac, the user performs the steps, Claude reads the port with a plain read-only client and assigns the values to the time windows of each step. Raw readings stay temporary (scratchpad); only a summary of the results goes into this design.md.
- Every line is handed to `Serial.write()`, with the serial transmit timeout lowered from the HWCDC default of 100 ms to 10 ms (`kSerialTxTimeoutMs`), so a host that stops reading stalls the loop for at most about that long; bytes that cannot be queued count as a dropped line in `errors`. About 130 bytes × 8 lines per second.
- First device test (2026-11-15): the original approach, writing only when `Serial.availableForWrite()` had room, delivered one `TMOS` line after boot and then none, while the screen's error counter rose at about the sample rate and the graphs kept moving. In the HWCDC code, host detection and draining of the TX ring buffer are triggered from inside `write()`; skipping writes while the buffer is full therefore keeps it full. Hence the change to always write with a bounded timeout. The fix has to be confirmed on the device.
- The descriptions of the algorithm values (baseline, filters) follow the ST datasheet and application note for the STHS34PF80; how the values actually behave in our room is what the device test measures.

## Risks / Trade-offs

- [The 8 Hz guess is too slow or too noisy] → Read the live lines with real movement; changing ODR is one constant and does not change the specs.
- [`Ex_I2C` and M5Unified's internal use of controller 0] → M5Unified only assigns the port; nothing else in this firmware uses it. Device test: audio, IMU and codec keep working while the sensor samples.
- [Configuration blocks the loop briefly] → ST's ODR setter waits for a pending data-ready while switching; this happens only at start and on re-probe (at most every 5 s while missing). Measure loop gaps in the device test.
- [Short press no longer changes scenes in debug mode] → Intended by the user; leaving debug mode with a long press restores the scene button.
- [Serial output adds USB load] → About 130 bytes × 8 per second; dropped instead of blocking.
- [Sensor mounted behind a window] → Thermal infrared does not pass glass or acrylic; the device test notes the mounting.

## Migration Plan

Mainframe-only firmware change. Rollback is flashing the previous `mainframe` build. No Node or protocol change.

## Open Questions

- Final sample rate, and whether thresholds should be tuned at runtime: answered by the measurements this change enables.
