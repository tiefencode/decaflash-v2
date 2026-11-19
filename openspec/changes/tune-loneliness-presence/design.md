# Design

## Context

See proposal.md, Why. Today's rules (code on `main`, 2026-11-19): loneliness +0.1 points/s only while nobody is present, held while anyone is present in either zone; lowered by Shake −15, MultiTap −12, Impact −10, Tap −8, Rotate −6, Move −5, `Kommt davor` −15, `Läuft vorbei` −5. Near zone: entering threshold 2000, leaving threshold 1600 with a 2 s hold (about 1 m); unchanged by this change.

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
- Keep the creature logic: company directly in front slowly lowers loneliness, gestures lower it further, being alone raises it.

**Non-Goals:**
- No resting value or drift to the middle: the user rejected it on 2026-11-19 because a fixed middle would make the value meaningless.
- No habituation and no change to the text bank lead value or the text model levels; habituation comes later as a general rule for all values (`ideas.md`, "Gewöhnung an dauerhafte Extremwerte für die Textbank").
- No change to eye, sound, light or depression rules, and no new debug readout.

## Decisions

All values chosen by the user on 2026-11-19 in the interview.

| Value | Old | New | Basis |
|---|---|---|---|
| Loneliness rise | linear | proportional to the distance to 100; the base rate applies at 50 | user ("stetig gebremst"); doubled base rate at 0, two fifths at 80, one fifth at 90; 100 is approached but practically not reached |
| Base rate, nobody present | +0.1 points/s | +0.2 points/s | user; from 0: 80 after 6.7 min, 90 after 9.6 min, 95 after 12.5 min (computed) |
| Base rate, far zone | 0 (held) | +0.1 points/s | user |
| Rate, near zone | 0 (held) | −0.1 points/s, linear | user: a reader in front slowly heals loneliness; from 95 to 50 in 7.5 min (computed) |
| Loneliness relief (Move / Tap / MultiTap / Impact / Rotate / Shake) | 5 / 8 / 12 / 10 / 6 / 15 | 5 / 3 / 6 / 2 / 4 / 5 | user; attention and annoyance amounts unchanged |
| Loneliness relief `Kommt davor` / `Läuft vorbei` | 15 / 5 | 5 / 5 | user |

- The rise keeps the existing 10 ms integer stepping: per step it adds (factor × (100,000 − loneliness)) / 50,000 milli-points with factor 2 (nobody) or 1 (far zone), carrying the division remainder, so 100 ms and finer update steps give the same whole-point value (host test).
- Purpose (user, 2026-11-19): depression, which loneliness drives only above 80, must stay reachable in reasonable time while loneliness no longer races to 100. The user accepted the times below.
- The near zone thresholds stay at 2000 / 1600 (about 1 m); a trial with 2800 / 2200 on the device on 2026-11-20 was taken back by the user. The user first chose 2800 / 2200 and took it back on 2026-11-19 after it turned out that `Kommt davor` is reported once per stay, not every 9 s.
- Alternatives the user considered and rejected: a resting value in the near zone (drift to 50 or 40), habituation for loneliness only, "loneliness low never as lead value", a quarter rising rate in the near zone, holding in the far zone, a rise with a kink at 80.

Host simulation with the new values (computed with the committed `personality.cpp`, scratch programs; start loneliness 50, depression 0, no gesture), loneliness / depression:

| Case | 5 min | 10 min | 15 min | 20 min | 25 min |
|---|---|---|---|---|---|
| Nobody, no music | 84 / 1 | 95 / 35 | 98 / 87 | 99 / 100 | |
| Far zone, no music | 72 / 0 | 84 / 3 | 91 / 29 | 95 / 70 | 97 / 100 |
| Nobody, 100 BPM, strong bass | 84 / 0 | 95 / 18 | 98 / 55 | 99 / 97 | |
| Nobody, 128 BPM, strong bass | 84 / 10 | 95 / 53 | 98 / 100 | | |
| Nobody, 128 BPM, weak bass (8 %) | 84 / 46 | 95 / 100 | | | |
| Far zone, 100 BPM, strong bass | 72 / 0 | 84 / 0 | 91 / 10 | 95 / 37 | 97 / 71 |
| Far zone, 128 BPM, strong bass | 72 / 8 | 84 / 21 | 91 / 56 | 95 / 100 | |

Depression reaches "high" (above 66) after about 9–17 minutes with nobody present and 10–25 minutes with someone in the far zone, depending on music. A person in the near zone lowers loneliness by 6 points per minute plus the events.

## Risks / Trade-offs

- [A visitor after an empty phase meets loneliness near 100, and the texts then lead with "loneliness high" for the whole stay] → accepted as creature behaviour ("finally someone"); gestures lower it; general habituation is the later remedy.
- [A crowd standing in the far zone makes the creature lonely, with eye lid and dimmed Node light from 90 after about 15 minutes] → checked in the device test; the far rate is calibration and can be retuned without a spec change.
- [Loneliness practically never shows 100 any more, so the Loneliness sound crossing at 90 and the light and eye effects from 90 come later than before] → accepted; they still occur after about 10 minutes alone.
- [If the near zone flickers, every new `Kommt davor` lowers loneliness by 5 and the reader heals it faster than intended] → the device test watches for repeated `Kommt davor` while standing still.

## Device test result

2026-11-20, AtomS3R Mainframe, normal `mainframe` build with these rules (first with the 2800 / 2200 near zone trial, then reflashed with 2000 / 1600). Observed by the user on the mood debug screen, no serial log and no recorded numbers: the behaviour is sensible overall; loneliness rises somewhat strongly, as expected from the computed curve. The user rejected the smaller near zone. Not verified on the device: the exact rates per zone and whether `Kommt davor` repeats while standing still.

## Migration Plan

Firmware only; flash `mainframe`. Rollback: flash the previous build.
