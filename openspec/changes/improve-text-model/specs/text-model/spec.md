# Spec Delta

## MODIFIED Requirements

### Requirement: Request input

A request SHALL carry the event kind and the lead mood value of the snapshot: one of the five mood values at low or high, chosen by the same rule as the text bank's lead value (text-bank spec, requirement "Lead mood value"). The other four mood values SHALL NOT influence the text. The event kinds SHALL be the eight motion texts (Move, Tap, MultiTap with two taps, MultiTap with three or more, Impact, Rotate, Reposition, Shake) and the four presence events with text.

#### Scenario: Levels of a snapshot
- **WHEN** a Tap is requested with energy 10, annoyance 70, attention 50, loneliness 0 and depression 34
- **THEN** the request carries Tap and loneliness low

#### Scenario: Other values ignored
- **WHEN** two Tap requests have the lead value annoyance high and differ only in the other four mood values
- **THEN** both are computed from the same input and, with the same seed, give the same text

#### Scenario: Calm mood
- **WHEN** every mood value lies in the middle third and loneliness is 38, the farthest from the middle
- **THEN** the request carries the event and loneliness low
