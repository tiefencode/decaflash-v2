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
| Zones (changed 2026-11-16) | near (up to about 1 m) and far (up to about 2 m); a first try with near = 0.5 m flickered |
| Attention while present | no decay; floor 20 in the far zone and 60 directly in front (rises to it at 3 points/s if lower); nobody present: falls at 3 points/s to 0 as before; changed 2026-11-16 |
| Movement | yes/no; `Bewegt sich` adds attention +5, at once and then at most every 9 s, in both zones, no text, from 15 s after arrival |
| Base and loneliness start | immediately when present (not only after `Kommt davor`) |
| Loneliness while present | falls from 100 to 0 in about 20 minutes |
| Presence events | `Kommt davor`, `Geht weg`, `Läuft vorbei` (`Bewegt sich viel` dropped 2026-11-16) |
| `Kommt davor` | only in the near zone, after 2 s above the near leaving threshold (changed from 3 s); once per stay |
| `Läuft vorbei` | a presence that ends within about 3 s after it began |
| `Kommt davor` effect | strong, like Shake: attention +25, loneliness −15 |
| `Läuft vorbei` effect | light, like Move: attention +8, loneliness −5 |
| `Geht weg` | when the person who came leaves the near zone (into the far zone or out of range); no mood effect, text only |
| Zone-change events | none besides `Kommt davor` and `Geht weg`; base values come from the state, events add fixed amounts |
| Waking | only `Kommt davor` wakes the sleeping creature, like a gesture |
| Text cooldown | 9 s per presence event kind, like gestures |
| Texts | written with the user (below); capital letters like all texts |
| Gesture texts | Tap loses `HUHU 🙂`; double tap loses `DU BIST DAS! 👉👈` and `HUHU 👋` and gains `WER IST DA? 🙂` |
| Text rules | emoji may repeat across kinds; two to four variants per kind |
| `Steht weiter weg` | after 2 s in the far zone, once per visit, not after `Kommt davor`; text only (attention is covered by the floor, loneliness by its constant fall) |
| Text priority | presence and gesture texts are fully equal (cooldown, priority, replacing each other) |
| Entering threshold (device test 2026-11-16) | keep 900, accepting a reach of about 2 m on that day |
| Several events per walk-past | accepted (presence can dip below the leaving threshold while walking) |
| Motion ringing after stepping in | ignore movement for about 15 s after arrival |
| Event output | presence events as `TMOS_EVENT` serial lines, for testing |
| Amount of movement | not measurable with this sensor: movement is yes/no, `Bewegt sich viel` and its texts are dropped, attention 40 still / 60 moving |

## Technical values (to fix on the device)

These follow from the measurements, not from a behaviour decision; the device test sets them.

| Value | Starting point | Reasoning |
| --- | --- | --- |
| Entering threshold | 900 | Between a still person at 2 m (≈ 550) and 1 m (≈ 1940), far above the stale baseline (≈ 340); 1.5 m is not measured yet. |
| Leaving threshold | 600 | Hysteresis against flicker at the edge; still above the stale baseline. |
| Near entering / leaving threshold | 2000 / 1600, leaving only after 2 s below | Device test 2026-11-16, standing still: 0.5 m 2800–4500, 1 m 1930–3355, 1.5 m about 1500. The first placeholder 4000 / 3000 sat inside the 0.5 m swing: near flickered, `Kommt davor` came about 14 s late and `Geht weg` 4 s after it. On 2026-11-15 1 m gave about 1940, close to 2000. |
| Settling time after sensor configuration | 15 s | Values unusable for 10–15 s after start. |
| Movement window | 3 s (24 samples at 8 Hz) | Long enough to smooth the flickering motion flag, short enough to react. |
| Movement ignore time after arrival | 15 s | Device test 2026-11-16: after stepping in and standing still the motion flag stayed on for about 13 s at 1 m, 10 s at 1.5 m, 7 s at 2 m (activity 100 %), which triggered `Bewegt sich viel` while standing still. |
| Dwell time for `Kommt davor` | 2 s above the near leaving threshold (user) | Walking past at 1 m stays about 1 s in the near zone; the dwell counts only time above the leaving threshold, so the 2 s hold does not turn a passer-by into `Kommt davor`. |
| Walk-past time | 3 s | `Läuft vorbei` = a presence ending within 3 s without `Kommt davor`; walk-pasts measured 0.6–2.8 s. |
| Loneliness fall while present | 0.083 points/s | 100 → 0 in about 20 minutes, replacing the 0.1 points/s rise. |

## Texts (agreed with the user)

Presence texts:

- `Kommt davor`: `DU BIST DAS! 👉👈`, `BIST DU AUCH GRUFTI? ⚰️`, `HUHU 👋`, `HUHU 🙂`
- `Läuft vorbei`: `HEY, WARTE! ❗`, `GUCK MAL HER! 🥹`, `UND TSCHÜSS. 😌`
- `Geht weg`: `SCHON WEG? 😢`, `BLEIB DOCH... 🥺`, `TSCHÜSS. 😔`
- `Steht weiter weg`: `IST DA WER? 🫢`, `ICH KANN DICH SEHEN! 👁`, `KOMM NUR HER, HEHE! 🔪` (texts by the user, emoji chosen by Claude)

Changed gesture texts (all others stay):

- Tap: `KLOPF 💬`, `WER KLOPFT SO SPÄT? 🫢` (`HUHU 🙂` removed)
- MultiTap, 2 taps: `KLOPF KLOPF 🤗`, `WER IST DA? 🙂` (`HUHU 👋` and `DU BIST DAS! 👉👈` removed; both now used by `Kommt davor`)

All emoji are in the curated palette (⚰️ is the coffin from the poems).

## Decisions on structure

### Own threshold on the presence value instead of the sensor flag

The sensor flag (threshold 200) fires for a stale baseline (≈ 340). A Mainframe-side decision keeps the raw flag visible for diagnosis, needs no embedded-register writes (which power the sensor down and reset its algorithm) and is host-testable. Raising the sensor's register threshold was the alternative.

### PresenceDecision module

New hardware-independent `presence_decision.{h,cpp}`, like `motion_events`:

- Input per sample: valid flag, presence value, motion flag, sample time; plus sensor configured-at time and missing state.
- Output: present yes/no, moving yes/no (motion flag set within the window, after the ignore time), person state for display (`away`, `far`, `far_moving`, `near`), time of the last state change, and at most one `PresenceEvent` per sample (`ArrivedInFront`, `WalkedPast`, `Left`) with a sequence number for text variants.
- Rules as in the presence-mood spec; time differences across `millis()` overflow.

### Personality

- `Personality::update()` gets the current presence (present, near, moving). While present, attention only rises to a floor (20 far, 60 near) and never falls; without presence it returns to 0 with the existing 3 points/s; loneliness falls at 0.083 points/s while present and rises at 0.1 points/s otherwise.
- New `Personality::onPresence(PresenceEvent)` applies the event amounts (no annoyance) and, for `ArrivedInFront` only, the same wake logic as `onMotion()`.
- Existing personality test cases run with "nobody present" and must keep their results.

### Message panel

- `MessagePanel::showPresence(now, event)` with its own text sets and per-kind cooldown (9 s, shared constant with motion), fully equal to motion texts. The motion text tables lose and gain the texts listed above; the emoji uniqueness check in the panel test is dropped.

### Debug screen and serial

- `person` row at y 34: label, state text (`away` grey, `still` green, `moving` magenta) from x 46, age right-aligned. Graphs shrink from 34 px to 29 px (46–74 and 78–106); `heat` at 110 and `sensor` at 119 stay.
- Presence graph: Mainframe entering threshold as a white dashed line next to the sensor's yellow one; the graph range covers 0, both thresholds, min and max.
- `TMOS` line: `person=away|still|moving` after `errors`.

## Device test results (2026-11-16)

AtomS3R Mainframe with the Unit TMOS PIR, `mainframe` build of this branch, one person, read live over serial.

| Case | presence value | person | Notes |
| --- | --- | --- | --- |
| Nobody in front | about 0 | away | |
| Still at 1 m | about 2700 | present | motion flag rang on for about 13 s after stepping in |
| Still at 1.5 m | about 1500 | present | ringing about 10 s |
| Still at 2 m | about 1000 | present | ringing about 7 s; on 2026-11-15 2 m gave about 550, so the value for one distance varies from day to day |
| Walking past at 1 m | peaks 2100-3700 | three short present stretches (0.6 s, 2.8 s, 1.8 s) | the value dipped to 510-560 between them |

Findings that changed the design: movement is ignored for 15 s after arrival (ringing), and events are printed as `TMOS_EVENT` lines. With the 15 s ignore time, small movements close to the sensor (presence ≈ 6000; motion 200–280, just above the sensor's motion threshold) still put the motion flag on 30–60 % of the time and triggered `Bewegt sich viel`. ST's material lists presence (also of still people), motion yes/no, passing-by, room occupancy and, with two sensors, people counting; nothing on the amount of movement (AN5867 itself could not be retrieved). The motion value grows with closeness, so its size says more about distance than about how much someone moves. The user therefore chose yes/no movement and dropped `Bewegt sich viel`. The user kept the entering threshold at 900 and accepted several events per walk-past. The panel showed `BLEIB DOCH...` (a `Geht weg` text) after the walk-past test; without event output it could not be told whether it came from the walk-past or from leaving the 2 m position before.

## Device test with announced steps (2026-11-16, build 6767b86)

One person, read live over serial with `TMOS_EVENT` lines. Near zone 2000 / 1600 with 2 s hold, `Kommt davor` after 2 s.

| Step | presence value | person | Events | Result |
| --- | --- | --- | --- | --- |
| Empty room after start | −33 to 61 | away, no change | none | as specified |
| Still at 1 m, about 25 s | about 2300–2500 | near, no change | one `arrived_in_front` 2 s after stepping in, `left` when walking away | as specified |
| Still at 1.5 m, about 60 s | about 1200–1400 | far, no change | none | as specified |
| Two walk-pasts at 1 m | about 1 s above 2000 each | near for about 1 s | two `walked_past`, no `arrived_in_front` | as specified |
| 1.5 m → 1 m (5 s) → 1.5 m (10 s) → away | far, near, far | far → near → far → away | `arrived_in_front` 2 s after stepping forward, `left` when stepping back, nothing when leaving from 1.5 m | as specified |

Notes: the way back to the keyboard passes through the cone and can add a `walked_past`. A first free test with much movement close to the sensor gave values between 1300 and 8800 and two left/arrived pairs; with standing still the zones held. The 0.5 m first try (4000 / 3000) is described in the decision table above. Not tested yet: the mood screen (task 7.4) and a provoked stale baseline (task 7.2).

## Device check of the new events (2026-11-16, build 5a8d5ef)

Free test by the user, read over serial: `arrived_in_front`, then `moves` about every 9 s (278.5, 287.6, 296.6, 305.6, 314.6 s), `left`, later `stands_far`, `arrived_in_front`, `left`. The user confirmed the panel texts. The user confirmed the mood screen (task 7.4). The stale baseline was measured on 2026-11-15 (≈ 340) and lies well below the 900 entering threshold (task 7.2); it was not provoked again on this build.

## Risks / Trade-offs

- [Dwell time vs. walking past] → Dwell 2 s counts only time above the near leaving threshold; the device test checks walking past at 1 m.
- [1.5 m threshold guessed] → Device test measures 1.5 m; thresholds are constants.
- [Stale baseline higher than the one measured value] → Device test provokes a stale baseline again (change the scene in front, e.g. move an object in and out) and checks `person=away`.
- [Loneliness falls fast in a busy spot] → Intended by the user; 20 minutes from 100 to 0 only while someone is within about 1.5 m.
- [Attention base 40–60 makes the gaze lively whenever someone is close] → Intended; below the depression rule (> 80).

## Migration Plan

Mainframe-only firmware change. Rollback is flashing the previous `mainframe` build.

## Open Questions

- Final thresholds and dwell time: answered by the device test.
