# Proposal

## Why

Before the show starts, single passes of the Mainframe main loop take 0.4 s or 2.1 s (BUG-029 in `bugs.md`). Measured on the device on 2026-11-17: the whole time is the PCM synthesis of a mood phrase inside the loop pass that starts it (attention UP 343 ms, loneliness UP 2,067 ms; starting the speaker takes 5 ms). During such a pass no IMU sample is read, so the motion classifier clears its window and drops a gesture in progress, and the eye stands still.

## What Changes

- The nine mood phrases and the snore are synthesised once after boot, a small piece per main loop pass, and kept in PSRAM until reboot (user decision, 2026-11-17: prepare in pieces after boot, not blocking at boot and not in a background task). Playing a sound no longer synthesises anything.
- A phrase or snore that becomes due before it is prepared is dropped and not played later (user decision, 2026-11-17). This can only happen in the first time after boot.
- The prepared sounds stay in memory after the show has started (user decision, 2026-11-17), about 0.42 MB of PSRAM. Preparation stops when the audio analysis is requested, because no creature sound plays after that.
- The PCM output of every sound is unchanged.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `sound`: Requirement "Locally synthesised sounds" states when phrases and the snore are synthesised; "Mood phrases" and "Sleep snoring" state what happens to a sound that is not prepared yet; "Playback does not block the loop" bounds the preparation per loop pass.

## Impact

- New `apps/mainframe/src/sound_synth.{h,cpp}` (pure synthesis, host-testable), `apps/mainframe/src/sound_engine.{h,cpp}`, one call in `loop()` in `apps/mainframe/src/main.cpp`, host test `tests/sound_synth.cpp`.
- PSRAM: 417,627 bytes for the prepared sounds instead of the 66,150-byte shared buffer.
- The loop-load measurement build prints a `LOOP_STALL` line naming the duration of each call between the IMU block and the eye.
- `bugs.md`: BUG-029.
