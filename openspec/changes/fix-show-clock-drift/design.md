# Design

## Context

See proposal.md for the motivation. `serviceShowClock()` runs once per main-loop pass. A pass takes about 5 ms plus work; an eye frame with a panel text takes up to 61 ms (loop-load measurement, 2026-11-17), so a beat is always noticed some milliseconds after it was due. A Node re-anchors its own clock on every ClockSync: it renders the received position at once and schedules its next beat one interval after processing the packet.

## Goals / Non-Goals

**Goals:**
- The show clock keeps the tempo it sends, whatever the loop load.
- The scheduling rule is host-testable.

**Non-Goals:**
- No change to tempo adoption, to the truncated beat interval or to the Node clock.
- No reduction of the loop load itself; a single beat is still noticed late by the length of the pass in progress. Only the accumulation goes away.
- No timestamp in ClockSync: a Node still anchors on packet arrival, so the lateness of the one pass that sends ClockSync still reaches the Nodes for that bar.

## Decisions

- **Schedule from the due time.** `takeDueShowBeat()` in `show_clock.h` reports whether a beat is due and moves the schedule on by one interval from the due time. `serviceShowClock()` calls it in a loop. Alternative: keep `now + interval` and subtract a measured average lateness; rejected, the lateness depends on what the display is doing.
- **Catch up all missed beats** (user decision). Alternatives offered: count one beat and skip the rest (bar position falls behind), or re-anchor on the late pass (phase moves). Catching up matches the Node's `serviceClock()`, so Mainframe and Nodes agree on the position after a stall.
- **ClockSync only for the beat that starts now.** During a catch-up only the last counted beat is current. Sending ClockSync for an earlier bar start would make the Nodes render beat 1 while the Mainframe is already past it. The Nodes count the missed bar start on their own clock.
- **Beat dot once per pass**, also when several beats were caught up.
- **Interval stays truncated** (user decision): `60000 / BPM` in whole milliseconds, the same on Mainframe and Node. With the accumulation gone the clock is exact at tempos that divide 60000 and up to 1 ms per beat fast otherwise (130 BPM: 461 instead of 461.54 ms, 0.12 %).

## Measurements

Lateness per beat before the fix, measured 2026-11-17 with `mainframe-loop-load-trial`: 6–8 ms average with show and music, 12–14 ms with a visible panel text, 44 ms maximum. The drift before and after the fix is measured on the device as elapsed time over a fixed number of beats against beats × interval; the results are recorded in `bugs.md` under BUG-028.

## Risks / Trade-offs

- [After a stall of more than one beat a bar may get no ClockSync] → Nodes keep running on their own clock at the same interval and are synchronised again one bar later. No such stall has been measured while the show runs (maximum 44 ms).
- [Nodes now see bars at the sent tempo instead of 1–2 % slower] → intended; checked on the device with Nodes where available.
