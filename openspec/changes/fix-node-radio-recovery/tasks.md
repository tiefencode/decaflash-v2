# Tasks

## 1. Silence watchdog (host-tested)

- [ ] 1.1 Add hardware-independent `apps/node/src/radio_watchdog.h` per design.md (unarmed until the first packet, `ReinitEspNow` after one silence period, `RestartWifi` after every further one, reset and "packets returned" report on a packet, attempt and repair counters); verify with a new host test `tests/radio_watchdog.cpp` + `tests/run_radio_watchdog.sh`, added to `tests/run_all.sh`, covering every scenario of "Node radio silence recovery" and a `millis()` overflow during a silence.

## 2. Radio repair and wiring in the Node

- [ ] 2.1 Add `apps/node/src/radio_recovery.{h,cpp}` with the two stages from design.md (stage 1: `recoverEspNow()` plus receive handler registration; stage 2: ESP-NOW and Wi-Fi shut down, then the boot sequence again) returning the step results; verify `pio run -e node` builds.
- [ ] 2.2 In `apps/node/src/main.cpp`: record every valid V2 packet in `onEspNowReceive()` before the Node-kind filter, feed the watchdog from the main loop, run the stage it asks for, update `espNowReady`, and print the `RADIO:` lines from design.md; confirm by reading the diff that no run mode, scene, clock, visual state or overlay variable is written on this path; verify `pio run -e node` builds.
- [ ] 2.3 Add the `RADIO:` line to `status` and to `printHelp()` if the help lists status fields; verify `pio run -e node` builds.

## 3. Host verification

- [ ] 3.1 Run `sh tests/run_all.sh` (all pass), `pio run -e node`, `pio run -e mainframe` (unchanged code, must still build) and `npx openspec validate fix-node-radio-recovery --strict`.

## 4. Device test (M5Atom Node, RGB strip, role wash, on `/dev/cu.usbserial-B956E80C38`, `node` build; Mainframe AtomS3R with the unchanged `mainframe` build; flashing via the upload command)

- [ ] 4.1 Stop the serial logger, flash the Node, start the logger again. With the Mainframe running and the show started, log for at least 10 minutes. Observe: `VISUAL:` lines every 3 s, no `RADIO:` repair line, `status` shows `ready=1 channel=1 repairs=0`.
- [ ] 4.2 Restart the Node while the Mainframe is switched off and let it run for 3 minutes. Observe: no `RADIO:` repair line, `status` shows `last_packet_age=never`.
- [ ] 4.3 With the show running, switch the Mainframe off and back on after 40 s. Observe: one `action=espnow_reinit result=ok` at 30 s, the strip keeps its pattern without a visible stutter, then `packets_back attempts=1`, the hello confirmation and `VISUAL:` lines again.
- [ ] 4.4 With the show running, switch the Mainframe off for at least 6 minutes, then on. Observe: `espnow_reinit` at 30 s, `wifi_restart result=ok` every 30 s after that, at least ten in a row; the strip keeps its pattern; run mode in `status` unchanged; then `packets_back`, the hello confirmation and `VISUAL:` lines again. Record the duration of one repair of each stage and the free heap before and after in design.md.
- [ ] 4.5 Start the show again on the Mainframe after 4.4. Observe: the Node takes scene and clock (`CONTROL: mode=` shows the running mode, scene change by button arrives).

## 5. Documentation

- [ ] 5.1 Update BUG-014 in `bugs.md`: the repair, what the device test proved (both stages leave a working receive path) and what it did not (the real fault was not provoked); the bug stays open until a logged repair of a real stall is seen. Add the README status line if the README lists Node behaviour; verify the text matches the code and the test log.

## Workflow follow-up

- Keep the serial logger on the Node after the device test; a `RADIO:` repair line while the Mainframe runs is the measurement for BUG-014.
- Archive the change after the device test and the user's review; add the silence period to the protocol spec's "Current values"; merge into `main` only with the user's agreement.
