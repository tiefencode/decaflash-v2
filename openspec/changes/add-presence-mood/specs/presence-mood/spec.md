# Spec Delta

## Purpose

How the Mainframe turns presence sensor samples into a decision about a person in front of the eye (present farther away or directly in front, moving or not), into presence events, and how both change the personality's attention, loneliness and sleep.

## ADDED Requirements

### Requirement: Person present decision

The Mainframe SHALL consider a person present when the presence value of a valid sample reaches its own entering threshold, and SHALL keep that decision until the presence value falls below its own leaving threshold. Both thresholds lie above the level measured for a stale sensor baseline; present reaches about 2 m (values in design.md). The sensor's presence flag SHALL NOT decide on its own.

#### Scenario: Person steps into range
- **WHEN** nobody is considered present and the presence value rises to the entering threshold
- **THEN** a person is considered present

#### Scenario: Value between the thresholds
- **WHEN** a person is considered present and the presence value drops below the entering threshold but stays at or above the leaving threshold
- **THEN** the person is still considered present

#### Scenario: Stale baseline
- **WHEN** nobody is in front and the presence value stays at the level measured for a stale baseline, even with the sensor's presence flag set
- **THEN** nobody is considered present

### Requirement: Near zone

While a person is present, the Mainframe SHALL consider them directly in front (near zone, about 0.5 m) when the presence value reaches its near entering threshold, and SHALL keep that until the value falls below its near leaving threshold (values in design.md). Otherwise a present person is farther away (far zone).

#### Scenario: Person steps directly in front
- **WHEN** a present person comes so close that the presence value reaches the near entering threshold
- **THEN** the person is in the near zone

#### Scenario: Person steps back
- **WHEN** a person in the near zone steps back and the presence value falls below the near leaving threshold but stays above the leaving threshold
- **THEN** the person is present in the far zone

### Requirement: No decision without usable samples

Nobody SHALL be considered present while the sensor is missing, during a fixed settling time after the sensor was configured (value in design.md), or after an invalid sample until the next valid one.

#### Scenario: Sensor just started
- **WHEN** the sensor was configured less than the settling time ago and the presence value is above the entering threshold
- **THEN** nobody is considered present yet

#### Scenario: Sensor missing
- **WHEN** the sensor is missing
- **THEN** nobody is considered present

### Requirement: Moving person decision

While a person is present, the Mainframe SHALL consider them moving when the motion flag was set within a short window, ignoring the first seconds after arrival while the motion output still rings (values in design.md). The decision is yes or no only, shown on the debug screen and in the serial lines, and changes no mood value. Without a present person nobody SHALL be considered moving.

#### Scenario: Standing still
- **WHEN** a present person stands still and the motion flag stays off for the whole window
- **THEN** the person counts as still

#### Scenario: Motion ringing after stepping in
- **WHEN** a person has just stepped in and stands still while the motion flag stays on during the ignore time
- **THEN** the person counts as still

#### Scenario: Moving in range
- **WHEN** a present person moves after the ignore time and the motion flag is set within the window
- **THEN** the person counts as moving

#### Scenario: Motion after leaving
- **WHEN** nobody is present and the motion flag is still set from the swing after leaving
- **THEN** nobody counts as moving

### Requirement: Attention while someone is present

While a person is present, in either zone and whether moving or not, attention SHALL NOT fall; below the presence floor (value in design.md: 20) it SHALL rise to the floor at its existing return rate. While nobody is present attention SHALL return towards 0 at its existing rate, as before. Gestures and presence events add on top.

#### Scenario: Person arrives with low attention
- **WHEN** attention is 0 and a person becomes present
- **THEN** attention rises to the presence floor at the return rate

#### Scenario: Attention above the floor
- **WHEN** a person is present and attention is 50 after an event
- **THEN** attention stays at 50 as long as the person is present

#### Scenario: Person leaves
- **WHEN** nobody is present any more
- **THEN** attention falls towards 0 at the existing return rate

### Requirement: Loneliness falls while someone is present

While a person is present in either zone, loneliness SHALL fall continuously at a rate that brings it from 100 to 0 in about 20 minutes, instead of rising. While nobody is present it SHALL rise as before. Gestures and presence events still lower it by their amounts.

#### Scenario: Someone stays in range
- **WHEN** loneliness is 100 and a person stays present for 20 minutes
- **THEN** loneliness is about 0

#### Scenario: Nobody there
- **WHEN** nobody is present
- **THEN** loneliness rises at its existing rate

### Requirement: Presence events

The Mainframe SHALL report three presence events: `Steht davor` after a person has been in the near zone for the dwell time, and again after every repeat interval while they stay; `Geht weg` when that person leaves the near zone; `Läuft vorbei` when a presence ends within the dwell time after it began (times in design.md).

#### Scenario: Someone stays directly in front
- **WHEN** a person enters the near zone and stays for the dwell time
- **THEN** `Steht davor` is reported, and again after every repeat interval while the person stays

#### Scenario: Someone stays farther away
- **WHEN** a person stays in the far zone for a long time and then leaves
- **THEN** no presence event is reported

#### Scenario: Short step into the near zone
- **WHEN** a person in the far zone steps into the near zone for less than the dwell time and back
- **THEN** no presence event is reported

#### Scenario: Someone walks past
- **WHEN** a person becomes present and is gone again before the dwell time
- **THEN** `Läuft vorbei` is reported and neither `Steht davor` nor `Geht weg`

#### Scenario: Value dips during a walk-past
- **WHEN** the presence value dips below the leaving threshold and rises again during one walk-past
- **THEN** `Läuft vorbei` may be reported more than once

#### Scenario: Someone steps back
- **WHEN** a person reported with `Steht davor` steps back into the far zone
- **THEN** `Geht weg` is reported once, and not again when they then leave the range

### Requirement: Mood effects of presence events

Each presence event SHALL change attention and loneliness once by a fixed amount, chosen to match a gesture (amounts in design.md): `Steht davor` like Shake, `Läuft vorbei` like Move. `Geht weg` SHALL change no mood value directly. Presence events SHALL NOT change annoyance, energy or depression directly.

#### Scenario: Someone stays in front
- **WHEN** `Steht davor` is reported
- **THEN** attention rises and loneliness falls by the same amounts as for a Shake, without the Shake's annoyance

#### Scenario: Someone leaves
- **WHEN** `Geht weg` is reported
- **THEN** no mood value changes at that moment

### Requirement: Waking by presence

`Steht davor` SHALL wake the sleeping creature under the same conditions as a gesture. The other presence events SHALL NOT wake it.

#### Scenario: Visitor at a sleeping creature
- **WHEN** the creature sleeps with enough energy to wake and `Steht davor` is reported
- **THEN** the creature wakes as after a gesture

#### Scenario: Passer-by at a sleeping creature
- **WHEN** the creature sleeps and `Läuft vorbei` is reported
- **THEN** the creature keeps sleeping
