# Proposal

## Why

BUG-014: a Node stopped following the Mainframe, kept playing its pattern, and did not react to a Mainframe restart or a restarted show; only a reset of the Node helped. The fault is not located and may take a long time to occur again. Today the Node initialises its radio once at boot and has no way to notice or repair a dead receive path, so one such fault costs the Node for the rest of the show.

## What Changes

- A Node that has heard the Mainframe at least once since boot watches for radio silence. When no valid V2 packet arrives for the silence period, it repairs its own receive path and repeats the repair at the same period until a packet arrives again.
- The repair is staged: the first attempt of a silence re-initialises ESP-NOW; every later attempt restarts Wi-Fi and ESP-NOW. Each attempt sets channel 1 again.
- A repair never changes the run mode, the scene or the clock. The Node does not reboot and does not fall back to `Demo`.
- Every attempt and the return of packets are written to the serial port, and `status` shows the radio state. The log is the measurement that BUG-014 lacks: it shows when packets stopped and which stage brought them back.
- The protocol spec's sentence "no radio-loss timeout" is replaced: radio silence still never changes the run mode, but it now triggers the repair.
- Not part of this change: any repair on the Mainframe, any Node transmission, a Node reboot as a last stage (user decision, 2026-11-17).

The user decided on 2026-11-17 to treat this as the fix for BUG-014, not as new radio feature work, so it is built before the end-to-end test named in `CLAUDE.md`.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `protocol`: the requirement "No automatic return to demo" loses its ban on a radio-loss timeout; a new requirement describes the Node's radio silence recovery.

## Impact

- `apps/node/src`: new silence watchdog (hardware-independent, host-tested) and radio repair; `main.cpp` feeds the watchdog from the receive handler, runs the repair from the main loop and extends `status`.
- `shared/include/espnow_transport.h`: unchanged. The existing, so far unused `recoverEspNow()` becomes the first repair stage.
- `tests/`: new host test for the watchdog, added to `tests/run_all.sh`.
- Mainframe firmware: unchanged, no flashing needed.
- `bugs.md`: BUG-014 gets the fix and its verification state. `openspec/specs/protocol/spec.md`: delta below, plus the silence period under "Current values" at archive.
