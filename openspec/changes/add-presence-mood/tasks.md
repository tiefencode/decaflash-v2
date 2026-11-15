# Tasks

## 1. Presence decision (host-tested)

- [ ] 1.1 Add hardware-independent `apps/mainframe/src/presence_decision.{h,cpp}` with `PersonState { Away, Still, Moving }`, entering/leaving thresholds, settling time, motion hold and change time per design.md; verify with a new host test `tests/presence_decision.cpp` + `tests/run_presence_decision.sh` covering: enter at threshold, stay between thresholds, leave below, warm-object level (≈ 500) and stale-baseline level (≈ 340) with the sensor flag set stay `Away`, settling time, missing sensor, invalid sample, motion hold bridging flicker, motion after leaving ignored, `millis()` overflow.

## 2. Attention base (host-tested)

- [ ] 2.1 Extend `Personality::update()` with the presence input and return attention towards the base (0 / still / moving) instead of 0; verify with new cases in `tests/personality.cpp` (arrival rises to the still base at 3 points/s, moving raises to the moving base and falls back, a tap returns to the base not to 0, leaving returns to 0, loneliness/annoyance/energy/depression unchanged by presence) and that all existing personality cases still pass with `Away`.
- [ ] 2.2 Wire it in `apps/mainframe/src/main.cpp`: feed each sensor sample and the sensor state into the decision, pass the current `PersonState` on both `personality.update()` calls; confirm in code that presence reaches nothing but the attention base; verify `pio run -e mainframe` builds.

## 3. Debug screen and serial line

- [ ] 3.1 Append `person=away|still|moving` to the `TMOS` line formatter; verify exact strings in `tests/presence_debug.cpp`.
- [ ] 3.2 Draw the `person` row (y 34, label, coloured state, age) and the Mainframe entering threshold in the presence graph; shrink the graphs to 29 px; the graph range covers both thresholds; verify `pio run -e mainframe` builds.

## 4. Documentation

- [ ] 4.1 Update the B.3/B.6 attention rule and the B.5 row 8 in `docs/project-context.md`, the README status line and `openspec/config.yaml` (sensor now drives the attention base); verify the text matches the code.

## 5. Host verification

- [ ] 5.1 Run `sh tests/run_all.sh` (all pass), `pio run -e mainframe` and the measurement environments that extend it, and `npx openspec validate add-presence-mood --strict`.

## 6. Device test (AtomS3R with the Unit TMOS PIR on the Grove port, `mainframe` build; flashing via the upload command)

- [ ] 6.1 Flash and read the serial lines live. Measure one person standing still at 1.5 m (and 1 m / 2 m for comparison) with the second person behind the sensor; fix the entering and leaving thresholds in design.md from the values. Observe: `person` is `still` at 1 m, `away` at 2 m and farther.
- [ ] 6.2 False positives: put a warm laptop into the field of view and confirm `person=away` while the sensor flag may be on; remove it and confirm the stale baseline also stays `away`. Observe the first 15 s after a restart stay `away`.
- [ ] 6.3 Attention on the mood screen: still person in front → attention settles at the still base; moving → higher base; leaving → back to 0; a tap while present returns to the base. Loneliness keeps rising while someone just stands there.

## Workflow follow-up

- Archive the change after the device test and the user's review; merge into `main` only with the user's agreement.
