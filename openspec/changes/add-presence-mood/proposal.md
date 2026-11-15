# Proposal

## Why

The personality should notice people in front of the eye. The measurement prototype (`add-presence-sensor-debug`, archived 2026-11-15) showed what the Unit TMOS PIR reliably reports: a person directly in front (up to about 2 m) and whether they move or stand still. It also showed two false positives that must never make the Mainframe believe someone is there: a warm object in view (presence value about 500) and a stale baseline after a warm object was removed (about 340). The sensor's own threshold (200) lets both through.

## What Changes

- The Mainframe decides itself whether a person is in front, using its own threshold with hysteresis on the presence value, set above the measured false positives. This limits "present" to roughly 1.5 m; people farther away are ignored. The first seconds after the sensor starts are ignored, because the values are not usable then.
- While a person is present, the Mainframe also decides whether they move, from the sensor's motion flag with a short hold time. Motion while nobody is present (for example the swing after someone leaves) is ignored.
- Attention gets a floor: still person in front → a light base value; moving person in front → a higher base value; nobody → 0 as today. Attention never decays below the current base, and rises to it at the existing return rate. IMU gestures keep adding impulses on top.
- Loneliness, annoyance, energy, depression, light directives, sounds and panel texts stay unchanged.
- The presence debug screen gets a `person` row (`away`, `still`, `moving`) with the time since it changed, and the Mainframe's own entering threshold drawn in the presence graph. The `TMOS` serial line gets a `person` field.
- **BREAKING (spec):** the presence-sensing requirement "No effect on behaviour" is replaced; samples now reach the personality, but only through the presence decision and only into attention.

## Capabilities

### New Capabilities
- `presence-mood`: the Mainframe's presence decision (person present, moving) and how it sets the attention base value.

### Modified Capabilities
- `presence-sensing`: "No effect on behaviour" is replaced by a limited-effect requirement; the `TMOS` serial line gains the `person` field.
- `debug-views`: the presence debug screen shows the person decision and the Mainframe's entering threshold.

## Impact

- Code: new hardware-independent presence decision under `apps/mainframe/src` (host-tested), `Personality::update()` takes the presence input and uses a base instead of 0 for attention, wiring in `main.cpp`, drawing in `eye_renderer.cpp`, serial formatter in `presence_debug.cpp`.
- Tests: new host test for the presence decision; personality and presence_debug tests extended.
- No dependency, protocol or Node change; no Node flashing. The CLAUDE.md block on new radio or Node output work does not apply.
- Device test on the AtomS3R with the Unit TMOS PIR: measure 1.5 m to fix the thresholds, confirm a warm object and a stale baseline do not count, and watch attention on the mood screen.
