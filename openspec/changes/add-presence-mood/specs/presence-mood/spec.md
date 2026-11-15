# Spec Delta

## Purpose

How the Mainframe turns presence sensor samples into a decision about a person in front of the eye (present, moving), into presence events, and how both change the personality's attention, loneliness and sleep.

## ADDED Requirements

### Requirement: Person present decision

The Mainframe SHALL consider a person present when the presence value of a valid sample reaches its own entering threshold, and SHALL keep that decision until the presence value falls below its own leaving threshold. Both thresholds lie above the level measured for a stale sensor baseline and limit "present" to about 1.5 m (values in design.md). The sensor's presence flag SHALL NOT decide on its own.

#### Scenario: Person steps close
- **WHEN** nobody is considered present and the presence value rises to the entering threshold
- **THEN** a person is considered present

#### Scenario: Value between the thresholds
- **WHEN** a person is considered present and the presence value drops below the entering threshold but stays at or above the leaving threshold
- **THEN** the person is still considered present

#### Scenario: Stale baseline
- **WHEN** nobody is in front and the presence value stays at the level measured for a stale baseline, even with the sensor's presence flag set
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

While a person is present, the Mainframe SHALL consider them moving when the sensor's motion flag was set in at least one valid sample within a short window. Samples from the first seconds after the person became present SHALL NOT count, because the motion output keeps ringing after someone steps in (window and ignore time in design.md). The decision is yes or no only; the sensor cannot tell little from much movement. Without a present person nobody SHALL be considered moving.

#### Scenario: Standing still
- **WHEN** a present person stands still and the motion flag stays off for the whole window
- **THEN** the person counts as still

#### Scenario: Motion ringing after stepping in
- **WHEN** a person has just stepped in and stands still while the motion flag stays on during the ignore time
- **THEN** the person counts as still

#### Scenario: Moving in front
- **WHEN** a present person moves after the ignore time and the motion flag is set within the window
- **THEN** the person counts as moving

#### Scenario: Motion after leaving
- **WHEN** nobody is present and the motion flag is still set from the swing after leaving
- **THEN** nobody counts as moving

### Requirement: Attention base from presence

While nobody is present the attention base SHALL be 0. While a still person is present it SHALL be the still base value and while a moving person is present the moving base value (values in design.md: 40 and 60). The base SHALL apply from the first sample a person is present. Attention SHALL return towards the current base at its existing return rate from above and from below; gestures and presence events add on top.

#### Scenario: Person arrives
- **WHEN** attention is 0 and a still person becomes present
- **THEN** attention rises to the still base value at the return rate

#### Scenario: Person moves
- **WHEN** a present person counts as moving
- **THEN** attention returns towards the moving base value, and back to the still base value when they stand still again

#### Scenario: Gesture while someone is present
- **WHEN** a person is present and a tap raises attention above the base
- **THEN** attention returns to the base value, not to 0

#### Scenario: Person leaves
- **WHEN** the person is no longer present
- **THEN** attention returns to 0 at the return rate

### Requirement: Loneliness falls while someone is present

While a person is present, loneliness SHALL fall continuously at a rate that brings it from 100 to 0 in about five minutes, instead of rising. While nobody is present it SHALL rise as before. Gestures and presence events still lower it by their amounts.

#### Scenario: Someone stands in front
- **WHEN** loneliness is 100 and a person stays present for five minutes
- **THEN** loneliness is about 0

#### Scenario: Nobody there
- **WHEN** nobody is present
- **THEN** loneliness rises at its existing rate

### Requirement: Presence events

The Mainframe SHALL report three presence events. `Kommt davor` when a person has been present without interruption for the dwell time; `Läuft vorbei` when a person stops being present before the dwell time; `Geht weg` when a person stops being present after `Kommt davor` (dwell time in design.md). A walk-past whose presence value dips below the leaving threshold MAY produce several `Läuft vorbei` events.

#### Scenario: Someone stays
- **WHEN** a person becomes present and stays for the dwell time
- **THEN** `Kommt davor` is reported once

#### Scenario: Someone walks past
- **WHEN** a person becomes present and is gone again before the dwell time
- **THEN** `Läuft vorbei` is reported and neither `Kommt davor` nor `Geht weg`

#### Scenario: Someone leaves after staying
- **WHEN** a person reported with `Kommt davor` stops being present
- **THEN** `Geht weg` is reported once

### Requirement: Mood effects of presence events

Each presence event SHALL change attention and loneliness once by a fixed amount, chosen to match a gesture (amounts in design.md): `Kommt davor` like Shake, `Läuft vorbei` like Move. `Geht weg` SHALL change no mood value directly. Presence events SHALL NOT change annoyance, energy or depression directly.

#### Scenario: Someone stays in front
- **WHEN** `Kommt davor` is reported
- **THEN** attention rises and loneliness falls by the same amounts as for a Shake, without the Shake's annoyance

#### Scenario: Someone leaves
- **WHEN** `Geht weg` is reported
- **THEN** no mood value changes at that moment

### Requirement: Waking by presence

`Kommt davor` SHALL wake the sleeping creature under the same conditions as a gesture. The other presence events SHALL NOT wake it.

#### Scenario: Visitor at a sleeping creature
- **WHEN** the creature sleeps with enough energy to wake and `Kommt davor` is reported
- **THEN** the creature wakes as after a gesture

#### Scenario: Passer-by at a sleeping creature
- **WHEN** the creature sleeps and `Läuft vorbei` is reported
- **THEN** the creature keeps sleeping
