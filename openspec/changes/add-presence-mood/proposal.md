# Proposal

## Why

Today loneliness rises all the time and only falls when someone handles the device: every gesture removes a fixed amount, at most one gesture per second. To keep the creature from getting lonely, people would have to play with it constantly. Presence in front of the eye should be the main driver against loneliness, and it should make the creature attentive and talkative.

The measurement prototype (`add-presence-sensor-debug`, archived 2026-11-15) showed what the Unit TMOS PIR reliably reports: a person directly in front (still at 1 m ≈ 1940), movement in front (motion flag on 71 % while dancing, 0 % while standing still), and walking past (presence for 2–3 s with a motion spike). It also showed one false positive that must never count as a person: a stale sensor baseline after the scene changed (presence ≈ 340 with nobody there; a restart brought it back to 0).

## What Changes

- The Mainframe decides itself whether someone is present, with its own threshold and hysteresis on the presence value, above the stale-baseline level. Two zones: directly in front (about 0.5 m) and farther away (up to about 2 m).
- While someone is present:
  - attention has a base value of 60 directly in front, 20 farther away, or 40 farther away while the person moves (movement is yes/no only: the sensor cannot tell little from much movement);
  - loneliness falls continuously, from 100 to 0 in about 5 minutes, instead of rising.
- Three presence events, like the motion gestures, each with its own mood effect and panel texts:
  - **Kommt davor** (someone stays directly in front for about 3 s): strong effect, like Shake; also wakes the sleeping creature like a gesture.
  - **Läuft vorbei** (present, but gone again before those 3 s): light effect, like Move.
  - **Geht weg** (the person who came leaves the near zone): only a text.
- Each presence event has two to four German texts in capital letters, ending in an emoji. Presence and gesture texts are fully equal: same 9 s cooldown per kind, same priority. Two gesture text sets change with it: Tap loses `HUHU 🙂`, double tap loses `DU BIST DAS! 👉👈` and `HUHU 👋` (both move to `Kommt davor`) and gains `WER IST DA? 🙂`. Emoji may now repeat across kinds.
- The presence debug screen gets a `person` row and the Mainframe's entering threshold in the presence graph; the `TMOS` serial line gets a `person` field.

## Capabilities

### New Capabilities
- `presence-mood`: the presence decision, the moving decision, the three presence events and their effect on attention, loneliness and sleep.

### Modified Capabilities
- `presence-sensing`: "No effect on behaviour" is replaced by a requirement that limits the effect to the presence-mood rules; the `TMOS` line gains the `person` field.
- `debug-views`: the presence debug screen shows the person decision and the Mainframe's entering threshold.
- `message-panel`: presence events become a third text source with their own texts and cooldown.

## Impact

- Code: new hardware-independent presence decision and event detection under `apps/mainframe/src` (host-tested); `Personality` gets the presence input for attention base, loneliness and presence events; `MessagePanel` gets presence texts; wiring in `main.cpp`; drawing in `eye_renderer.cpp`; serial formatter in `presence_debug.cpp`.
- Tests: new host test for presence decision and events; personality, message_panel and presence_debug tests extended.
- No dependency, protocol or Node change; no Node flashing.
- Device test on the AtomS3R with the Unit TMOS PIR: fix the 1.5 m threshold, check the stale baseline stays away, check walking past against the 3 s dwell time, watch attention and loneliness on the mood screen and the texts in the panel.
