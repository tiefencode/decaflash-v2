# Proposal

## Why

The M5Stack Unit TMOS PIR presence sensor (U185, STHS34PF80) is now available, and the personality is supposed to react to people in front of the eye. Before any presence rule is designed, we need to see on the device what the sensor reliably reports: standing still, moving, walking past, distance, background people, heat sources. Today the firmware does not read the sensor at all, so there is nothing to measure with.

## What Changes

- The Mainframe reads the Unit TMOS PIR on the AtomS3R Grove port at a fixed rate: the three flags (presence, motion, ambient temperature shock), the three algorithm outputs (presence, motion, ambient shock value), the object temperature (raw and compensated) and the ambient temperature.
- A missing or failing sensor never blocks start-up, audio, radio or the show; the Mainframe keeps running and reports the sensor as missing.
- A new presence debug screen shows these values live with labels written out in full: one indicator per flag with the time since its last change, a recent history of the presence and motion values with the sensor thresholds drawn in, and the current numbers.
- Button behaviour in debug mode changes: a long press still opens the mood debug screen; while a debug screen is open, a short press switches between the mood debug screen and the presence debug screen instead of starting the show or selecting the next scene; a long press returns to the normal eye.
- Every sensor sample is printed over USB serial as one self-describing `TMOS name=value ...` line, so measurements can be read live in a serial monitor. The meaning of every field is part of the spec.
- Measurement prototype only: the sensor has no effect on mood values, eye, light directives or texts. A later change will define presence rules based on the measurements.

## Capabilities

### New Capabilities
- `presence-sensing`: reading the STHS34PF80 presence sensor on the Mainframe, its fixed configuration, failure handling, and the serial output lines with the meaning of each field.
- `debug-views`: the Mainframe's on-device debug screens (mood and presence) and how the front button opens, switches and closes them.

### Modified Capabilities
None. No existing spec covers the button or the sensors.

## Impact

- Code: new sensor module and presence debug drawing under `apps/mainframe/src`, button handling and wiring in `apps/mainframe/src/main.cpp`, debug drawing in `eye_renderer.{h,cpp}`.
- Dependency: new pinned library `sparkfun/SparkFun STHS34PF80 Arduino Library@1.0.5` in `platformio.ini` for the `mainframe` environment (inherited by the measurement environments that extend it). Added as its own step according to CLAUDE.md.
- I²C: the sensor uses the Grove port (G2/G1, I²C controller 0 via M5Unified `Ex_I2C`), separate from the internal bus and the codec (controller 1).
- Node firmware, protocol and shared headers are unchanged; no Node flashing needed.
- `openspec/config.yaml` still lists the Unit TMOS PIR (U185) as "planned, not present"; the change updates that line.
- Device test needs the AtomS3R with the U185 on the Grove port; flashing needs the user's approval.
