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
- Keep all logic that does not touch hardware (history, flag timing, CSV line, debug view state) in host-testable code, like `motion_events`.
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
- The sensor's detection thresholds and hystereses stay at their power-on values. They are read back once after configuration and shown on screen and in the CSV header, so we see the real values instead of assuming datasheet defaults.

### Missing sensor and recovery

- Start-up probes `WHO_AM_I`. Missing → state "missing", re-probe every 5 s. A probe of an absent device is one NACKed transaction.
- After a successful start, a failed read marks the sample invalid and increments an error counter. After 8 consecutive failures the state returns to "missing" and the re-probe path takes over (covers a pulled cable).
- Configuration after a re-probe uses the same code as start-up.

### Code structure

- `presence_sensor.{h,cpp}` (device only): the SparkFun subclass on `Ex_I2C`, probe, configuration, polling, and the latest `PresenceSample`.
- `presence_debug.{h,cpp}` (hardware-independent, host-tested): ring history of presence and motion values (15 s at 8 Hz = 120 samples, one per pixel column), time since each flag changed (with `millis()` overflow), min/max for the graph scale, and the CSV line formatter.
- `debug_view.h` (hardware-independent, host-tested): a three-state view (`Off`, `Mood`, `Presence`) with `onHold()` and `onClick()`. `onClick()` reports whether the click belongs to the show (only in `Off`), so `main.cpp` keeps the show logic where it is.
- `EyeRenderer::service()` gets the active view and a pointer to the presence debug data; drawing of the presence screen lives next to the mood overlay.

### Presence screen layout (128×128, font 1)

Top line: sensor state (`presence sensor ok`, or `sensor missing`/`sensor error` with error count); beat dot stays free. Three flag rows, each `name  ON/off  time` (e.g. `ambient shock off 42s`, time as `s` below 100 s, then `m`), with the state highlighted in colour. Two graphs of about 26 px height, each with a title line `presence 123 thr 200` / `motion 45 thr 200`, the threshold as a dashed line; the y-range covers 0, the threshold and the window's min/max, so the threshold is always visible. Bottom line: `shock 3  obj 1234  23.4C`. Labels are English, like the existing mood screen. Exact pixel positions are an implementation detail.

### Serial CSV

- Columns: `TMOS,ms,valid,presence_flag,motion_flag,shock_flag,presence,motion,shock,object_raw,object_comp,ambient_c,errors`.
- `TMOS_HEADER,...` at start, every 10 s, and once more with the read-back thresholds (`TMOS_CONFIG,odr_hz,avg_tmos,presence_ths,presence_hyst,motion_ths,motion_hyst,shock_ths,shock_hyst`) after each successful configuration.
- A line is written only if `Serial.availableForWrite()` has room for it; otherwise it is dropped and the drop counter appears in the next line's `errors` column. The firmware never waits for a host.

## Risks / Trade-offs

- [The 8 Hz guess is too slow or too noisy] → Read the CSV with real movement; changing ODR is one constant and does not change the specs.
- [`Ex_I2C` and M5Unified's internal use of controller 0] → M5Unified only assigns the port; nothing else in this firmware uses it. Device test: audio, IMU and codec keep working while the sensor samples.
- [Configuration blocks the loop briefly] → ST's ODR setter waits for a pending data-ready while switching; this happens only at start and on re-probe (at most every 5 s while missing). Measure loop gaps in the device test.
- [Short press no longer changes scenes in debug mode] → Intended by the user; leaving debug mode with a long press restores the scene button.
- [CSV output adds USB load] → About 100 bytes × 8 per second; dropped instead of blocking.
- [Sensor mounted behind a window] → Thermal infrared does not pass glass or acrylic; the device test notes the mounting.

## Migration Plan

Mainframe-only firmware change. Rollback is flashing the previous `mainframe` build. No Node or protocol change.

## Open Questions

- Final sample rate, and whether thresholds should be tuned at runtime: answered by the measurements this change enables.
