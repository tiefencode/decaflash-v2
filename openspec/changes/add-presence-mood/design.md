# Design

## Context

See proposal.md for the motivation. Current state:

- `presence_sensor.{h,cpp}` polls the Unit TMOS PIR at 8 Hz; `main.cpp::servicePresence()` feeds each sample into `PresenceDebug` (screen history) and prints the `TMOS` line. Nothing reaches the personality.
- `Personality::update(now, MoodAudio)` runs about every 100 ms and before motion events. Attention returns with `attention_ = toward(attention_, 0, elapsed * 3)` (3 points/s towards 0, `personality.cpp:61`); loneliness rises by 0.1 points/s; `onMotion()` adds per-gesture amounts (attention / loneliness): Move +8/−5, Tap +12/−8, MultiTap +18/−12, Impact +20/−10, Rotate +10/−6, Shake +25/−15; gestures can wake the sleeping creature.
- `MessagePanel::showMotion()` picks a text per motion kind with a 9 s cooldown per kind.
- Measurements from 2026-11-15 (archived change `2026-11-15-add-presence-sensor-debug`, design.md "Measurement results"): presence value, one person still: 1 m ≈ 1940, 2 m ≈ 550 (60 s without decay), 3.5 m 75–170; moving at 1 m 950–2900 with the motion flag on 71 %; walking past at 1 m: presence on for 2–3 s; stale baseline after the scene changed ≈ 340 with nobody there (a restart brought it to 0); first 10–15 s after start unusable; the motion flag also reacts to negative swings and stays on after someone leaves.

## Decisions taken with the user (interview 2026-11-15)

| Topic | Decision |
| --- | --- |
| Range of "present" | up to about 1.5 m |
| Attention base, still person | 40 |
| Attention base, a lot of movement | up to 60, graded with movement |
| Base and loneliness start | immediately when present (not only after `Kommt davor`) |
| Loneliness while present | falls from 100 to 0 in about 5 minutes |
| Presence events | `Kommt davor`, `Geht weg`, `Läuft vorbei`, `Bewegt sich viel` |
| `Kommt davor` vs. `Läuft vorbei` | `Kommt davor` only after staying about 3 s; gone earlier = `Läuft vorbei`, no `Geht weg` |
| `Kommt davor` effect | strong, like Shake: attention +25, loneliness −15 |
| `Läuft vorbei` effect | light, like Move: attention +8, loneliness −5 |
| `Bewegt sich viel` effect | medium, like Tap: attention +12, loneliness −8 |
| `Geht weg` effect | none directly (text only) |
| Waking | only `Kommt davor` wakes the sleeping creature, like a gesture |
| Text cooldown | 9 s per presence event kind, like gestures |
| Texts | proposed by Claude, approved by the user (below) |

## Technical values (to fix on the device)

These follow from the measurements, not from a behaviour decision; the device test sets them.

| Value | Starting point | Reasoning |
| --- | --- | --- |
| Entering threshold | 900 | Between a still person at 2 m (≈ 550) and 1 m (≈ 1940), far above the stale baseline (≈ 340); 1.5 m is not measured yet. |
| Leaving threshold | 600 | Hysteresis against flicker at the edge; still above the stale baseline. |
| Settling time after sensor configuration | 15 s | Values unusable for 10–15 s after start. |
| Movement window | 3 s (24 samples at 8 Hz) | Long enough to smooth the flickering motion flag, short enough to react. |
| `Bewegt sich viel` levels | rise at 0.5, re-arm below 0.2 | Dancing measured at 0.71, standing still at 0. |
| Dwell time for `Kommt davor` | 3 s (user) | Walking past measured at 2–3 s, so 3 s is close to the edge; the device test checks it. |
| Loneliness fall while present | 0.33 points/s | 100 → 0 in about 5 minutes, replacing the 0.1 points/s rise. |

## Texts (proposal, for the user's approval)

Uppercase like the gesture texts; each ends in a curated emoji no motion text uses.

- `Kommt davor`: `OH, BESUCH! 😍`, `DA BIST DU JA. 🥰`, `HALLO, DU! 😘`
- `Läuft vorbei`: `HEY, WARTE! ❗`, `GUCK MAL HER! 🥹`, `UND TSCHÜSS. 😌`
- `Bewegt sich viel`: `TANZ MIT MIR! 🎵`, `WILD! 😂`, `JA, WEITER SO! 👌`
- `Geht weg`: `SCHON WEG? 😢`, `BLEIB DOCH... 🥺`, `TSCHÜSS. 😔`

## Decisions on structure

### Own threshold on the presence value instead of the sensor flag

The sensor flag (threshold 200) fires for a stale baseline (≈ 340). A Mainframe-side decision keeps the raw flag visible for diagnosis, needs no embedded-register writes (which power the sensor down and reset its algorithm) and is host-testable. Raising the sensor's register threshold was the alternative.

### PresenceDecision module

New hardware-independent `presence_decision.{h,cpp}`, like `motion_events`:

- Input per sample: valid flag, presence value, motion flag, sample time; plus sensor configured-at time and missing state.
- Output: present yes/no, movement activity 0–1, person state for display (`away`, `still`, `moving` = present with activity above 0), time of the last state change, and at most one `PresenceEvent` per sample (`ArrivedInFront`, `WalkedPast`, `Left`, `MovesALot`) with a sequence number for text variants.
- Rules as in the presence-mood spec; time differences across `millis()` overflow.

### Personality

- `Personality::update()` gets the current presence (present, activity). Attention returns towards `base = present ? 40 + 20 × activity : 0` with the existing 3 points/s; loneliness falls at 0.33 points/s while present and rises at 0.1 points/s otherwise.
- New `Personality::onPresence(PresenceEvent)` applies the event amounts (no annoyance) and, for `ArrivedInFront` only, the same wake logic as `onMotion()`.
- Existing personality test cases run with "nobody present" and must keep their results.

### Message panel

- `MessagePanel::showPresence(now, event)` with its own text sets and per-kind cooldown (9 s, shared constant with motion), same priority as motion texts.

### Debug screen and serial

- `person` row at y 34: label, state text (`away` grey, `still` green, `moving` magenta) from x 46, age right-aligned. Graphs shrink from 34 px to 29 px (46–74 and 78–106); `heat` at 110 and `sensor` at 119 stay.
- Presence graph: Mainframe entering threshold as a white dashed line next to the sensor's yellow one; the graph range covers 0, both thresholds, min and max.
- `TMOS` line: `person=away|still|moving` after `errors`.

## Risks / Trade-offs

- [Dwell time 3 s vs. walking past 2–3 s] → A slow passer-by may count as `Kommt davor` and then `Geht weg`. The device test measures passing at 1 m and 1.5 m; the dwell time is a constant.
- [1.5 m threshold guessed] → Device test measures 1.5 m; thresholds are constants.
- [Stale baseline higher than the one measured value] → Device test provokes a stale baseline again (change the scene in front, e.g. move an object in and out) and checks `person=away`.
- [Loneliness falls fast in a busy spot] → Intended by the user; 5 minutes from 100 to 0 only while someone is within about 1.5 m.
- [Attention base 40–60 makes the gaze lively whenever someone is close] → Intended; below the depression rule (> 80).

## Migration Plan

Mainframe-only firmware change. Rollback is flashing the previous `mainframe` build.

## Open Questions

- Approval of the proposed texts.
- Final thresholds and dwell time: answered by the device test.
