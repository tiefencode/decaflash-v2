# presence-mood Specification

## Purpose
How the Mainframe turns presence sensor samples into a decision about a person in front of the eye (present farther away or directly in front, moving or not), into presence events, and how both change the personality's attention, loneliness and sleep.

## Requirements

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

While a person is present, the Mainframe SHALL consider them directly in front (near zone, up to about 1 m) when the presence value reaches its near entering threshold, and SHALL keep that until the value stays below its near leaving threshold for a hold time (values in design.md). Otherwise a present person is farther away (far zone).

#### Scenario: Person steps directly in front
- **WHEN** a present person comes so close that the presence value reaches the near entering threshold
- **THEN** the person is in the near zone

#### Scenario: Person steps back
- **WHEN** a person in the near zone steps back and the presence value stays below the near leaving threshold for the hold time but above the leaving threshold
- **THEN** the person is present in the far zone

#### Scenario: Value swings while standing still
- **WHEN** a person stands still in the near zone and the presence value dips below the near leaving threshold for less than the hold time
- **THEN** the person stays in the near zone and no event is reported

### Requirement: No decision without usable samples

Nobody SHALL be considered present while the sensor is missing, during a fixed settling time after the sensor was configured (value in design.md), or after an invalid sample until the next valid one.

#### Scenario: Sensor just started
- **WHEN** the sensor was configured less than the settling time ago and the presence value is above the entering threshold
- **THEN** nobody is considered present yet

#### Scenario: Sensor missing
- **WHEN** the sensor is missing
- **THEN** nobody is considered present

### Requirement: Moving person decision

While a person is present, the Mainframe SHALL consider them moving when the motion flag was set within a short window, ignoring the first seconds after arrival while the motion output still rings (values in design.md). The decision is yes or no only and is shown on the debug screen and in the serial lines; it affects moods only through the `Bewegt sich` event. Without a present person nobody SHALL be considered moving.

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

Attention SHALL always return at its existing rate towards a target: 0 while nobody is present, the zone floor while a person is present (values in design: 20 in the far zone, 60 in the near zone). It SHALL fall towards the floor from above and rise to it from below. Gestures and presence events add on top.

#### Scenario: Person arrives farther away
- **WHEN** attention is 0 and a person becomes present in the far zone
- **THEN** attention rises to the far floor at the return rate

#### Scenario: Person steps directly in front
- **WHEN** attention is below the near floor and the person enters the near zone
- **THEN** attention rises to the near floor at the return rate

#### Scenario: Attention above the floor
- **WHEN** a person is present and attention is 50 after an event
- **THEN** attention falls at the return rate until it reaches the zone floor, and stays there

#### Scenario: Person leaves
- **WHEN** nobody is present any more
- **THEN** attention falls towards 0 at the existing return rate

### Requirement: Loneliness holds while someone is present

While a person is present in either zone, loneliness SHALL neither rise nor fall over time. While nobody is present it SHALL rise as before. Gestures and presence events still lower it by their amounts.

#### Scenario: Someone stays in range
- **WHEN** a person stays present in either zone and no gesture or presence event occurs
- **THEN** loneliness keeps its value

#### Scenario: Nobody there
- **WHEN** nobody is present
- **THEN** loneliness rises at its existing rate

### Requirement: Presence events

The Mainframe SHALL report: `Kommt davor` once per near stay after the dwell time; `Geht weg` when that person leaves the near zone; `Steht weiter weg` once per visit after the dwell time in the far zone, not after `Kommt davor`; `Läuft vorbei` when a presence ends within the walk-past time without a greeting; `Bewegt sich` while a present person moves, at most once per cooldown (times in design.md).

#### Scenario: Someone stays directly in front
- **WHEN** a person enters the near zone and stays for the dwell time
- **THEN** `Kommt davor` is reported once, however long the person stays

#### Scenario: Someone stays farther away
- **WHEN** a person stays in the far zone for a long time and then leaves
- **THEN** `Steht weiter weg` is reported once, after the dwell time, and nothing when they leave

#### Scenario: Greeted in front, then steps back
- **WHEN** a person reported with `Kommt davor` steps back into the far zone and stays there
- **THEN** `Geht weg` is reported and no `Steht weiter weg`

#### Scenario: Someone keeps moving
- **WHEN** a present person counts as moving for a long time
- **THEN** `Bewegt sich` is reported at once and then every movement cooldown

#### Scenario: Short step into the near zone
- **WHEN** a person in the far zone steps into the near zone for less than the dwell time and back
- **THEN** no presence event is reported

#### Scenario: Someone walks past
- **WHEN** a person becomes present and is gone again within the walk-past time
- **THEN** `Läuft vorbei` is reported and neither `Kommt davor` nor `Geht weg`

#### Scenario: Value dips during a walk-past
- **WHEN** the presence value dips below the leaving threshold and rises again during one walk-past
- **THEN** `Läuft vorbei` may be reported more than once

#### Scenario: Someone steps back
- **WHEN** a person reported with `Kommt davor` steps back into the far zone
- **THEN** `Geht weg` is reported once, and not again when they then leave the range

### Requirement: Mood effects of presence events

Each presence event SHALL change attention and loneliness once by a fixed amount, chosen with the user (amounts in design.md): `Kommt davor` like Shake, `Läuft vorbei` like Move, `Bewegt sich` attention only. `Geht weg` and `Steht weiter weg` SHALL change no mood value directly. Presence events SHALL NOT change annoyance, energy or depression directly.

#### Scenario: Someone stays in front
- **WHEN** `Kommt davor` is reported
- **THEN** attention rises and loneliness falls by the same amounts as for a Shake, without the Shake's annoyance

#### Scenario: Someone leaves
- **WHEN** `Geht weg` is reported
- **THEN** no mood value changes at that moment

#### Scenario: Someone moves
- **WHEN** `Bewegt sich` is reported
- **THEN** attention rises by the movement amount and loneliness does not change

### Requirement: Waking by presence

`Kommt davor` SHALL wake the sleeping creature under the same conditions as a gesture. The other presence events SHALL NOT wake it.

#### Scenario: Visitor at a sleeping creature
- **WHEN** the creature sleeps with enough energy to wake and `Kommt davor` is reported
- **THEN** the creature wakes as after a gesture

#### Scenario: Passer-by at a sleeping creature
- **WHEN** the creature sleeps and `Läuft vorbei` is reported
- **THEN** the creature keeps sleeping
