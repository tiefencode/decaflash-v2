# Tasks

## 1. Silence watchdog (host-tested)

- [x] 1.1 Add hardware-independent `apps/node/src/radio_watchdog.h` per design.md (armed from boot, `ReinitEspNow` after one silence period, `RestartWifi` after every further one, `Reboot` at the reboot period only under Mainframe control, reset and "packets returned" report on a packet, attempt and repair counters); verify with a new host test `tests/radio_watchdog.cpp` + `tests/run_radio_watchdog.sh`, added to `tests/run_all.sh`, covering every scenario of "Node radio silence recovery" and "Node reboot after long silence" that the watchdog decides (including no reboot in `Demo`, restart of the reboot period by one packet) and a `millis()` overflow during a silence.

## 2. Radio repair and wiring in the Node

- [x] 2.1 Add `apps/node/src/radio_recovery.{h,cpp}` with the two repair stages from design.md (stage 1: `recoverEspNow()` plus receive handler registration; stage 2: ESP-NOW and Wi-Fi driver shut down, then the boot sequence again) returning the step results; verify `pio run -e node` builds.
- [x] 2.2 In `apps/node/src/main.cpp`: record every valid V2 packet in `onEspNowReceive()` before the Node-kind filter, start the watchdog in `setup()` also when the radio start-up failed, feed it from the main loop, run the stage it asks for, update `espNowReady`, and print the `RADIO:` lines from design.md; confirm by reading the diff that no run mode, scene, clock, visual state or overlay variable is written on the repair path; verify `pio run -e node` builds.
- [x] 2.3 Implement the reboot with its safeguards from design.md: last packet check under `radioMux`, `RADIO: ... action=reboot` line, `renderer.allOff()`, `ESP.restart()`; print `BOOT: reset_reason=` in `setup()`; verify `pio run -e node` builds.
- [x] 2.4 Add the `RADIO:` line to `status`; verify `pio run -e node` builds.

## 3. Host verification

- [x] 3.1 Run `sh tests/run_all.sh` (all pass), `pio run -e node`, `pio run -e mainframe` (unchanged code, must still build) and `npx openspec validate fix-node-radio-recovery --strict`.

## 4. Device test (M5Atom Node, RGB strip, role wash, on `/dev/cu.usbserial-B956E80C38`, `node` build; Mainframe AtomS3R with the unchanged `mainframe` build; flashing via the upload command)

- [x] 4.1 Stop the serial logger, flash the Node, start the logger again. With the Mainframe running and the show started, observe: `VISUAL:` lines every 3 s, no `RADIO:` repair line, `status` shows `ready=1 channel=1 repairs=0`. Done 2026-11-17: no repair line in any period with the Mainframe sending (12:06 to 12:14 and between the unplug tests).
- [x] 4.2 Node in `Demo` without Mainframe. Observe: `espnow_reinit` at 30 s, `wifi_restart result=ok` every 30 s after that, the demo pattern without a visible stutter, no reboot, `status` shows `last_packet_age=never`; then `packets_back` and the hello confirmation when the Mainframe comes on. Done 2026-11-17 in the 166 s after the Node's own reboot. Shortened by the user's decision: not run for 7 minutes, so "no reboot in `Demo` beyond the reboot period" rests on the host test.
- [x] 4.3 With the show running, switch the Mainframe off and back on after about 40 s. Observe: one `action=espnow_reinit result=ok` at 30 s, the strip keeps its pattern, then `packets_back attempts=1` and the hello. Done 2026-11-17, 12:31 to 12:32.
- [x] 4.4 With the show running, switch the Mainframe off and back on after about 100 s. Observe: `espnow_reinit` at 30 s, `wifi_restart` after that, then `packets_back`, the hello, and after a show start `CONTROL: mode=mainframe` and the changed scene. Done 2026-11-17 twice (attempts=3 at 12:26, attempts=2 at 12:30).
- [x] 4.5 With the show running, switch the Mainframe off for more than 5 minutes. Observe: repairs every 30 s, at 5 minutes `action=reboot`, `BOOT: reset_reason=` for a software reset, the Node in `Demo` with its stored kind and role, then repairs every 30 s; after the Mainframe is back the Node takes hello, scene and clock. Done 2026-11-17, 12:14 to 12:22. Shortened by the user's decision: the Mainframe came back 166 s after the reboot, not after 7 more minutes.

## 5. Documentation

- [x] 5.1 Update BUG-014 in `bugs.md`: the repair and the reboot, what the device test proved (both repair stages leave a working receive path, the reboot rejoins) and what it did not (the real fault was not provoked); the bug stays open until a logged recovery from a real stall is seen. Add the README status line if the README lists Node behaviour; verify the text matches the code and the test log.

## Workflow follow-up

- Keep the serial logger on the Node after the device test; a `RADIO:` repair line while the Mainframe runs is the measurement for BUG-014.
- Archive the change after the device test and the user's review; add the silence period and the reboot period to the protocol spec's "Current values"; merge into `main` only with the user's agreement.
