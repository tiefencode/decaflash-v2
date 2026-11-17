# Proposal

## Why

BUG-014: a Node stopped following the Mainframe, kept playing its pattern, and did not react to a Mainframe restart or a restarted show; only a reset of the Node helped. The fault is not located and may take a long time to occur again. Today the Node initialises its radio once at boot and has no way to notice or repair a dead receive path, so one such fault costs the Node for the rest of the show.

## What Changes

- A Node watches for radio silence from boot on, whether or not it has ever heard a Mainframe. When no valid V2 packet arrives for the silence period, it repairs its own receive path and repeats the repair at the same period until a packet arrives again. The Nodes hang from the ceiling during a show, so a manual reset is not possible and a fault before the first packet must be covered too.
- The repair is staged: the first attempt of a silence re-initialises ESP-NOW; every later attempt restarts the Wi-Fi driver and ESP-NOW. Each attempt sets channel 1 again. "Wi-Fi" here is the radio driver that ESP-NOW runs on; the Node still never joins a network.
- A repair never changes the run mode, the scene or the clock.
- Last stage: a Node under Mainframe control (`MainframeWaiting` or `MainframeRunning`) that stays silent for the reboot period reboots itself, because a reset is the only repair known to work. A Node in `Demo` never reboots because of silence. After the reboot the Node is in `Demo`, so it cannot reboot again until it has received a hello or a clock, which proves its radio works. Before rebooting it checks once more for a packet and switches its outputs off.
- Every attempt, the return of packets and a reboot are written to the serial port, and `status` shows the radio state. The log is the measurement that BUG-014 lacks: it shows when packets stopped and which stage brought them back.
- The protocol spec's sentence "no radio-loss timeout" is replaced: silence shorter than the reboot period still never changes the run mode, but it triggers the repair; the reboot is the only automatic way back to `Demo`.
- Not part of this change: any repair on the Mainframe, any Node transmission, a watchdog for a hung Node main loop (user decision, 2026-11-17).

The user decided on 2026-11-17 to treat this as the fix for BUG-014, not as new radio feature work, so it is built before the end-to-end test named in `CLAUDE.md`.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `protocol`: the requirement "No automatic return to demo" loses its ban on a radio-loss timeout; new requirements describe the Node's radio silence recovery, the reboot after long silence and the serial reporting.

## Impact

- `apps/node/src`: new silence watchdog (hardware-independent, host-tested) and radio repair; `main.cpp` feeds the watchdog from the receive handler, runs the repair from the main loop and extends `status`.
- `shared/include/espnow_transport.h`: unchanged. The existing, so far unused `recoverEspNow()` becomes the first repair stage.
- `tests/`: new host test for the watchdog, added to `tests/run_all.sh`.
- Mainframe firmware: unchanged, no flashing needed.
- `bugs.md`: BUG-014 gets the fix and its verification state. `openspec/specs/protocol/spec.md`: delta below, plus the silence period under "Current values" at archive.
