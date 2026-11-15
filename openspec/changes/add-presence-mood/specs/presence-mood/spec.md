# Spec Delta

## Purpose

How the Mainframe turns presence sensor samples into a decision about a person in front of the eye, and how that decision sets a floor for the personality's attention.

## ADDED Requirements

### Requirement: Person present decision

The Mainframe SHALL consider a person present when the presence value of a valid sample reaches its own entering threshold, and SHALL keep that decision until the presence value falls below its own leaving threshold. Both thresholds lie above the false positives measured for warm objects and a stale sensor baseline (values in design.md). The sensor's presence flag SHALL NOT decide on its own.

#### Scenario: Person steps close
- **WHEN** nobody is considered present and the presence value rises to the entering threshold
- **THEN** a person is considered present

#### Scenario: Value between the thresholds
- **WHEN** a person is considered present and the presence value drops below the entering threshold but stays at or above the leaving threshold
- **THEN** the person is still considered present

#### Scenario: Warm object or stale baseline
- **WHEN** the presence value stays at the level measured for a warm object or a stale baseline, even with the sensor's presence flag set
- **THEN** nobody is considered present

#### Scenario: Person farther away
- **WHEN** a person stands at about 2 m or farther and the presence value stays below the entering threshold
- **THEN** nobody is considered present

### Requirement: No decision without usable samples

Nobody SHALL be considered present while the sensor is missing, during a fixed settling time after the sensor was configured (value in design.md), or after an invalid sample until the next valid one.

#### Scenario: Sensor just started
- **WHEN** the sensor was configured less than the settling time ago and the presence value is above the entering threshold
- **THEN** nobody is considered present yet

#### Scenario: Sensor missing
- **WHEN** the sensor is missing
- **THEN** nobody is considered present

### Requirement: Moving person decision

While a person is considered present, the Mainframe SHALL consider them moving when the sensor's motion flag is set, and SHALL keep them moving for a fixed hold time after the last set motion flag (value in design.md). Without a present person, nobody SHALL be considered moving.

#### Scenario: Dancing in front
- **WHEN** a person is present and the motion flag flickers on and off within the hold time
- **THEN** the person is considered moving the whole time

#### Scenario: Standing still
- **WHEN** a person is present and the motion flag stays off longer than the hold time
- **THEN** the person is considered present and still

#### Scenario: Motion after leaving
- **WHEN** the person is no longer considered present and the motion flag is still set from the swing after leaving
- **THEN** nobody is considered moving

### Requirement: Attention base from presence

The personality's attention SHALL have a base value: 0 when nobody is present, a light base value when a still person is present and a higher base value when a moving person is present (values in design.md). Attention SHALL return towards the current base at its existing return rate from above and from below. Motion gestures SHALL keep adding impulses on top.

#### Scenario: Person arrives
- **WHEN** attention is 0 and a still person becomes present
- **THEN** attention rises to the light base value at the return rate and stays there while the person stays still

#### Scenario: Person starts moving
- **WHEN** a still person is present and starts moving
- **THEN** attention rises to the higher base value and falls back to the light base value after the person is still again

#### Scenario: Gesture while someone is present
- **WHEN** a person is present and a tap raises attention above the base
- **THEN** attention returns to the base value, not to 0

#### Scenario: Person leaves
- **WHEN** the person is no longer present
- **THEN** attention returns to 0 at the return rate

### Requirement: Other mood values unchanged by presence

The presence decision SHALL NOT change energy, annoyance, loneliness or depression directly, and SHALL NOT send light directives, play sounds or show panel texts. Depression may still change through its existing attention rule.

#### Scenario: Person stands in front for a long time
- **WHEN** a still person is present for several minutes
- **THEN** loneliness keeps rising at its existing rate and no panel text or light directive is caused by the presence
