# Proposal

## Why

Whoever reads the reaction texts stands directly in front of the creature, and today loneliness then neither rises nor falls while every presence event and gesture lowers it. A visit therefore pushes loneliness to 0 and keeps it there (or freezes a high value, for example 85, when nobody touches the device), and the text bank and the text model then react to "loneliness low" almost every time. The user decided on 2026-11-19 to fix this by retuning how presence moves loneliness, not by a text-only rule.

## What Changes

- Loneliness rises with nobody present at a faster rate than today.
- Loneliness also rises while a person is present in the far zone, at a lower rate than with nobody present. While a person is in the near zone it holds its value, as today.
- `Kommt davor` lowers loneliness much less than today. Shake lowers it by the same smaller amount, so `Kommt davor` stays "like a Shake without annoyance". The other gestures keep their amounts.
- The near zone becomes stricter: higher near entering and leaving thresholds, so a person counts as directly in front only when closer than today. This also moves the attention floor for the near zone, `Kommt davor` and `Geht weg`, which all follow the near zone.
- The eye, the mood sounds, the Node light directive and depression keep their rules and follow the changed loneliness value. The debug screens stay as they are.
- Not part of this change: habituation to lasting extreme values. The user wants it, if at all, as a general rule for all mood values in a change of its own.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `personality`: loneliness no longer rises at one constant rate in every state; its rate depends on presence, and presence events lower it as well as motion events.
- `presence-mood`: loneliness rises at a lower rate in the far zone instead of holding while anyone is present; the near zone reaches less far.

## Impact

- `apps/mainframe/src/personality.cpp`: loneliness rate by presence zone, `ArrivedInFront` and Shake relief.
- `apps/mainframe/src/presence_decision.h`: near entering and leaving thresholds.
- `tests/personality.cpp`, `tests/presence_decision.cpp`: expected values.
- `openspec/specs/personality/spec.md` "Current values" section: loneliness rates and Shake amount.
- Indirect: eye lid and gaze from loneliness 90, Loneliness sound crossings, Node light dimming from 90 and the depression social term from 80 are reached sooner when nobody or only a far person is present. No protocol change; only the `mainframe` environment is affected.
