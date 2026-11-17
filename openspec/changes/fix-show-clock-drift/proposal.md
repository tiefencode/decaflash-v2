# Proposal

## Why

The Mainframe show clock schedules each beat one interval after the loop pass that noticed the previous beat, so the lateness of every beat is added to all later ones and the clock runs slower than the BPM it sends in ClockSync (BUG-028 in `bugs.md`). The Nodes are pulled onto this slower bar on every ClockSync, so the whole installation drifts against the tempo between two tempo adoptions.

## What Changes

- The show clock schedules the following beat one beat interval after the time the current beat was due, not after the loop pass that noticed it. Late loop passes no longer shift later beats.
- When a loop pass comes one or more whole beats late, the clock counts every missed beat in that pass and stays on its beat grid (user decision, 2026-11-17). The Node clock already behaves this way.
- After such a catch-up the Mainframe sends ClockSync only if the beat that starts now is beat 1 of a bar. A bar start that lies among the missed beats sends no ClockSync, because a Node renders the ClockSync position when the packet arrives.
- Unchanged (user decisions, 2026-11-17): a tempo adoption still re-anchors the clock on the last onset, and the beat interval stays `60000 / BPM` truncated to whole milliseconds on Mainframe and Node.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `tempo`: Requirement "Free-running show clock" states that beats are scheduled from the previous beat's due time and that missed beats are caught up.
- `protocol`: Requirement "Mainframe clock sending" states which beat sends ClockSync after missed beats were caught up.

## Impact

- `apps/mainframe/src/show_clock.h`, `serviceShowClock()` in `apps/mainframe/src/main.cpp`, host test `tests/show_clock.cpp`.
- Timing seen by the Nodes: bars arrive at the sent BPM instead of 1–2 % slower. No packet layout, message type or Node code changes; no change under `shared/include`.
- `bugs.md`: BUG-028.
