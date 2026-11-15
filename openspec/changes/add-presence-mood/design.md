# Design

## Context

See proposal.md for the motivation. Current state:

- `presence_sensor.{h,cpp}` polls the Unit TMOS PIR at 8 Hz; `main.cpp::servicePresence()` feeds each sample into `PresenceDebug` (screen history) and prints the `TMOS` line. Nothing reaches the personality.
- `Personality::update(now, MoodAudio)` runs about every 100 ms and before motion events. Attention returns with `attention_ = toward(attention_, 0, elapsed * 3)` (`personality.cpp:61`), i.e. 3 points/s towards 0; gestures add impulses in `onMotion()`.
- Measurements from 2026-11-15 (archived change `2026-11-15-add-presence-sensor-debug`, design.md "Measurement results"): presence value, one person still: 1 m ≈ 1940, 2 m ≈ 550 (60 s without decay), 3.5 m 75–170; moving at 1 m 950–2900 with the motion flag on 71 %; warm laptop ≈ 500; stale baseline after removing a warm object ≈ 340; first 10–15 s after start unusable; the motion flag also reacts to negative swings and stays on after someone leaves.

## Goals / Non-Goals

**Goals:**
- A presence decision that warm objects and a stale baseline cannot trigger.
- Attention reflects a person in front: light floor when still, higher floor when moving, 0 when nobody is there.
- Everything decision-related is hardware-independent and host-tested.

**Non-Goals:**
- No change to loneliness, annoyance, energy or depression rules (user decision: loneliness stays an interaction state).
- No light directives, sounds or panel texts from presence (light controllers wait for the V2 Node end-to-end test per CLAUDE.md).
- No group detection, no "approach" or "arrival after N s absence" events (not reliably measurable).
- No automatic re-learning of the sensor baseline; the Mainframe threshold makes a stale baseline harmless instead.

## Decisions

### Own threshold on the presence value instead of the sensor flag

The sensor flag (threshold 200) fires for a warm laptop (≈ 500) and a stale baseline (≈ 340). Raising the sensor's own register threshold would also work, but a Mainframe-side decision keeps the raw flag visible for diagnosis, needs no embedded-register writes (which power the sensor down and reset its algorithm) and is host-testable.

Provisional calibration values:

| Value | Provisional | Reasoning |
| --- | --- | --- |
| Entering threshold | 900 | Clearly above laptop (≈ 500) and stale baseline (≈ 340), clearly below a still person at 1 m (≈ 1940). |
| Leaving threshold | 600 | Hysteresis against flicker of a person near the edge; still above both false positives. |
| Settling time after sensor configuration | 15 s | Measured: values unusable for 10–15 s after start. |
| Motion hold | 1.5 s | The motion flag flickered while dancing (on 71 %); the hold bridges the gaps. |
| Attention base, still | 15 | "Light" base (user); below every existing threshold (depression uses attention > 80). |
| Attention base, moving | 30 | "Higher" base (user decision: moving raises the base, no impulse). |

Expected range with these values: up to about 1.5 m. People at 2 m (≈ 550) or farther stay `away`, which the user chose ("ignore people farther away"). The 1.5 m value is not measured yet; the device test fixes the thresholds.

- Alternative considered: a lower base for 1.5–3 m. Rejected by the user; it would overlap the false-positive range (340–550).

### PresenceDecision module

New hardware-independent `presence_decision.{h,cpp}`, like `motion_events`:

- Input per sample: valid flag, presence value, motion flag, sample time; plus "sensor configured at" time and "sensor missing".
- Output: `PersonState { Away, Still, Moving }` and the time of the last change (for the `person` row).
- Rules: Away while missing, during settling or after an invalid sample until the next valid one; Away → present at presence ≥ enter; present → Away below leave; Moving while present and the motion flag was set within the hold time; motion while Away is ignored (covers the negative swing after leaving).
- Time differences across `millis()` overflow, as in the other modules.

### Personality input

- New `MoodPresence { PersonState person; }` (or the enum directly) as a second argument of `Personality::update()`. Gesture calls (`onMotion`) stay unchanged; `main.cpp` passes the current decision on both `update()` calls.
- `attention_ = toward(attention_, base, elapsed * 3)` with base 0 / 15 / 30 (in milli-points 0 / 15000 / 30000). Rising to the base uses the same 3 points/s, so a person arriving brings attention from 0 to 15 in 5 s — gradual, matching the gaze's own smoothing.
- The existing host test for personality gets cases for arrival, moving, leaving and gesture-on-top; old cases pass `Away` and must keep their results.

### Debug screen and serial

- `person` row at y 34: `person` label, state text (`away` grey, `still` green, `moving` magenta) from x 46, age right-aligned. Graphs shrink from 34 px to 29 px (46–74 and 78–106); `heat` at 110 and `sensor` at 119 stay.
- Presence graph: Mainframe entering threshold as a white dashed line next to the sensor's yellow one; the graph range covers 0, both thresholds, min and max.
- `TMOS` line: `person=away|still|moving` appended after `errors`; the formatter in `presence_debug.cpp` takes the state.

## Risks / Trade-offs

- [1.5 m guess is wrong for the installation] → Device test measures 1.5 m; thresholds are constants and do not change the specs.
- [A person standing still at the edge of range flickers in and out] → Hysteresis 900/600; if it still flickers, widen it.
- [A very warm object close to the sensor exceeds 900] → Not measured; mounting note: nothing warm in the field of view. The debug screen shows `person` vs. raw flag, so it is visible on site.
- [Attention floor keeps the gaze lively all evening in a busy spot] → Only up to about 1.5 m counts; base values are low.
- [Stale baseline larger than measured] → Only one stale case was measured (≈ 340). The device test repeats it with a warm object removed.

## Migration Plan

Mainframe-only firmware change. Rollback is flashing the previous `mainframe` build.

## Open Questions

- Final thresholds and whether 1.5 m is the right range: answered by the device test.
