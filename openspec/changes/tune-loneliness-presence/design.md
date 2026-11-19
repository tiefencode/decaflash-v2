# Design

## Context

See proposal.md, Why. Today's rules (code on `main`, 2026-11-19): loneliness +0.1 points/s only while nobody is present, held while anyone is present in either zone; lowered by Shake −15, MultiTap −12, Impact −10, Tap −8, Rotate −6, Move −5, `Kommt davor` −15, `Läuft vorbei` −5. Near zone: entering threshold 2000, leaving threshold 1600 with a 2 s hold (about 1 m).

Host simulation with the current `personality.cpp`, 100 ms steps, music running, values per minute over 15 minutes (computed, scratch program, not committed):

| Case | Start 50 | Start 100 |
|---|---|---|
| Stands in front, no gesture | 35 and holds | 85 and holds |
| Stands in front, one Tap per minute | 0 after 5 min | 0 after 11 min |
| Steps back and in again every 2 min | 0 after 7 min | 0 after 13 min |
| Passer-by every 2 min, nobody between | 98 | 100 |
| Nobody | 100 after 9 min | 100 |

Other readers of loneliness: eye lid and lowered gaze from 90, Loneliness sound on rising crossings of 10/30/50/70/90 (no falling phrase), Node light directive dimmed from 90, depression social term above 80, mood debug screen, text bank lead value and text model mood level. Sleep does not read it.

## Goals / Non-Goals

**Goals:**
- A visitor no longer drives loneliness to a lasting 0 just by arriving, and a far-away crowd does not freeze it.
- Keep the creature logic: company directly in front holds loneliness, gestures lower it, being alone raises it.

**Non-Goals:**
- No resting value or drift to the middle: the user rejected it on 2026-11-19 because a fixed middle would make the value meaningless.
- No habituation and no change to the text bank lead value or the text model levels; habituation comes later as a general rule for all values (`ideas.md`, "Gewöhnung an dauerhafte Extremwerte für die Textbank").
- No change to eye, sound, light or depression rules, and no new debug readout.

## Decisions

All values chosen by the user on 2026-11-19 in the interview.

| Value | Old | New | Basis |
|---|---|---|---|
| Loneliness rate, nobody present | +0.1 points/s | +0.2 points/s | user; 0 → 100 in about 8.3 min (computed) |
| Loneliness rate, far zone | 0 (held) | +0.1 points/s (half the rate) | user |
| Loneliness rate, near zone | 0 (held) | 0 (held) | user: a reader in front must not make the creature lonelier |
| `Kommt davor` loneliness relief | −15 | −5 | user |
| Shake loneliness relief | −15 | −5 | user; attention +25 and annoyance +10 unchanged |
| Near entering / leaving threshold | 2000 / 1600 | 2800 / 2200 | user; distance about 0.7 m is estimated from the 2026-11-16 measurement (0.5 m 2800–4500, 1 m 1930–3355, 1.5 m about 1500) |
| Near leaving hold time | 2 s | 2 s | unchanged |

- The rate keeps the existing integer stepping (whole milli-points per 10 ms) so 100 ms and finer update steps give the same result; the far zone uses the old step, nobody present twice that step.
- The 4000 / 3000 near thresholds tried on 2026-11-16 flickered because they sat inside the 0.5 m swing; 2800 / 2200 stays below that and keeps the 2 s leaving hold.
- Alternatives the user considered and rejected: a resting value in the near zone (drift to 50 or 40), habituation for loneliness only, "loneliness low never as lead value", a quarter rate in the near zone, holding in the far zone.

Host simulation with the new values (computed, same scratch program; near and far are set directly, not through the presence decision):

| Case | Start 50 |
|---|---|
| Stands in front, no gesture | 45 and holds |
| Stands in front, one Tap per minute | 0 after 6 min |
| Steps back to the far zone for 5 s and in again every 2 min | falls slowly, 13 after 15 min |
| Passer-by every 2 min, nobody between | 100 after 6 min |
| Nobody | 100 after about 4 min |

## Risks / Trade-offs

- [A visitor after an empty phase meets loneliness near 100, and the texts then lead with "loneliness high" for the whole stay] → accepted as creature behaviour ("finally someone"); gestures lower it; general habituation is the later remedy.
- [A crowd standing in the far zone makes the creature lonely in about 8 minutes, with eye lid and dimmed Node light from 90] → checked in the device test; the far rate is calibration and can be retuned without a spec change.
- [Gestures still drive loneliness to 0 while someone stays in front] → accepted for now; the device test shows whether gesture amounts need retuning.
- [2800 / 2200 may flicker or miss a reader at panel distance] → device test with the presence debug screen before the change is finished.

## Migration Plan

Firmware only; flash `mainframe`. Rollback: flash the previous build.
