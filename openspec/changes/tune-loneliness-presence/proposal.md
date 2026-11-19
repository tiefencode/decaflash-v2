# Proposal

## Why

Whoever reads the reaction texts stands directly in front of the creature, and today loneliness then neither rises nor falls while every presence event and gesture lowers it. A visit therefore pushes loneliness to 0 and keeps it there (or freezes a high value, for example 85, when nobody touches the device), and the text bank and the text model then react to "loneliness low" almost every time. The user decided on 2026-11-19 to fix this by retuning how presence moves loneliness, not by a text-only rule.

## What Changes

- Loneliness rises with nobody present at a faster base rate than today, and it rises ever more slowly the closer it is to 100 (a saturating curve instead of a straight line), so it lingers in the range where depression starts instead of jumping to 100.
- Loneliness also rises while a person is present in the far zone, at half the base rate. While a person is in the near zone it falls slowly instead of holding.
- Every gesture and `Kommt davor` lowers loneliness much less than today. Shake and `Kommt davor` keep the same amount, so `Kommt davor` stays "like a Shake without annoyance".
- The near zone becomes stricter for a trial (user, 2026-11-20): higher near entering and leaving thresholds, so a person counts as directly in front only when closer than today. This also moves the attention floor for the near zone, `Kommt davor` and `Geht weg`, which all follow the near zone.
- The eye, the mood sounds, the Node light directive and depression keep their rules and follow the changed loneliness value. The debug screens stay as they are.
- Not part of this change: habituation to lasting extreme values. The user wants it, if at all, as a general rule for all mood values in a change of its own.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `personality`: loneliness no longer rises at one constant rate in every state; its rate depends on presence and slows down towards 100, and presence events lower it as well as motion events.
- `presence-mood`: loneliness rises at a lower rate in the far zone and falls slowly in the near zone, instead of holding while anyone is present; the near zone reaches less far.

## Impact

- `apps/mainframe/src/personality.cpp`: saturating loneliness rise by presence zone, loneliness relief of all gestures and `ArrivedInFront`.
- `apps/mainframe/src/presence_decision.h`: near entering and leaving thresholds.
- `tests/personality.cpp`, `tests/presence_decision.cpp`: expected values.
- `openspec/specs/personality/spec.md` "Current values" section: loneliness rise and gesture amounts.
- Indirect: eye lid and gaze from loneliness 90, Loneliness sound crossings, Node light dimming from 90 and the depression social term from 80 are reached sooner when nobody or only a far person is present. No protocol change; only the `mainframe` environment is affected.
