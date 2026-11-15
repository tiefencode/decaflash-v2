# Tasks

## 1. Presence decision and events (host-tested)

- [x] 1.1 Add hardware-independent `apps/mainframe/src/presence_decision.{h,cpp}` per design.md (present decision with entering/leaving threshold, settling time, missing/invalid handling, movement activity over the window, person state, events `ArrivedInFront`, `WalkedPast`, `Left`, `MovesALot` with dwell time and activity levels); verify with a new host test `tests/presence_decision.cpp` + `tests/run_presence_decision.sh` covering every presence-mood scenario, the stale-baseline level (≈ 340) with the sensor flag set staying away, and `millis()` overflow.

## 2. Personality (host-tested)

- [ ] 2.1 Extend `Personality` with the presence input (attention base 40 + 20 × activity while present, loneliness falling at 0.33 points/s while present) and `onPresence()` (event amounts, wake only on `ArrivedInFront`); verify with new cases in `tests/personality.cpp` and that all existing cases still pass with nobody present.

## 3. Message panel (host-tested)

- [ ] 3.1 Add `MessagePanel::showPresence()` with the four text sets from design.md, variant selection per kind and a 9 s cooldown per kind, fully equal to motion texts; change the Tap and double-tap texts as listed in design.md; verify with new and updated cases in `tests/message_panel.cpp` (texts, variant selection with two Tap variants, cooldown, gesture and presence texts replacing each other) and that all texts render with known glyphs.

## 4. Wiring, screen and serial

- [ ] 4.1 Wire the decision in `apps/mainframe/src/main.cpp`: feed each sample and the sensor state, pass the presence on both `personality.update()` calls, hand each event to `personality.onPresence()` and then `messagePanel.showPresence()`; confirm in code that presence reaches nothing else; verify `pio run -e mainframe` builds.
- [ ] 4.2 Append `person=away|still|moving` to the `TMOS` line; verify exact strings in `tests/presence_debug.cpp`.
- [ ] 4.3 Draw the `person` row and the Mainframe entering threshold, shrink the graphs to 29 px; verify `pio run -e mainframe` builds.

## 5. Documentation

- [ ] 5.1 Update the attention and loneliness rules (B.3/B.6), the event table and B.5 row 8 in `docs/project-context.md`, the README status line and `openspec/config.yaml`; correct the archived measurement summary: the laptop in view was cold (per the user), the false positive was the stale baseline; verify the text matches the code.

## 6. Host verification

- [ ] 6.1 Run `sh tests/run_all.sh` (all pass), `pio run -e mainframe` and the measurement environments that extend it, and `npx openspec validate add-presence-mood --strict`.

## 7. Device test (AtomS3R with the Unit TMOS PIR on the Grove port, `mainframe` build; flashing via the upload command)

- [ ] 7.1 Flash and read the serial lines live. Measure one person standing still at 1.5 m (and 1 m / 2 m for comparison) with the second person behind the sensor; fix the entering and leaving thresholds in design.md. Observe: `person` is present at 1 m and 1.5 m, `away` at 2 m.
- [ ] 7.2 Provoke a stale baseline (change the scene in front, e.g. move an object in and out) and confirm `person=away`; confirm the first 15 s after a restart stay `away`.
- [ ] 7.3 Events: walk past at 1 m and 1.5 m (expect `Läuft vorbei` text, no `Kommt davor`); stand in front for longer than the dwell time (`Kommt davor` text), dance (`Bewegt sich viel` text), leave (`Geht weg` text). Adjust the dwell time if walking past triggers `Kommt davor`.
- [ ] 7.4 Moods on the mood screen: attention settles near 40 while standing still, rises towards 60 while dancing, falls to 0 after leaving; loneliness falls while someone stands in front and rises again afterwards; a sleeping creature wakes on `Kommt davor` and not on walking past.

## Workflow follow-up

- Archive the change after the device test and the user's review; update the message-panel spec's Current values (presence texts, changed Tap and double-tap texts, curated emoji list); merge into `main` only with the user's agreement.
