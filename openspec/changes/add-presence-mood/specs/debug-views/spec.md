# Spec Delta

## ADDED Requirements

### Requirement: Person decision on the presence debug screen

The presence debug screen SHALL show a `person` row with the Mainframe's presence decision, `away`, `far`, `moving` (farther away and moving) or `near`, and the time since it last changed. The presence history SHALL also show the Mainframe's entering threshold and near-zone threshold as lines, so the sensor flag and the Mainframe's decision can be compared on the device.

#### Scenario: Person decision differs from the sensor flag
- **WHEN** the presence debug screen is visible and the sensor's presence flag is on while the presence value stays below the Mainframe's entering threshold
- **THEN** the presence row shows `ON`, the `person` row shows `away`, and the presence history stays below the entering threshold line

#### Scenario: Present person starts moving
- **WHEN** the presence debug screen is visible, the `person` row shows `far` and the person starts moving
- **THEN** the `person` row shows `moving` and its time since the last change starts again from zero

## MODIFIED Requirements

### Requirement: Presence debug screen contents

The presence debug screen SHALL show, for each of the presence, motion and ambient shock flags, its state and the time since it last changed; the `person` row; a recent history of the presence value and of the motion value, each with the sensor's detection threshold drawn as a line; the heat signal; and the sensor's own temperature in degrees Celsius. A sensor state line SHALL appear only when the sensor is missing or a read failed.

#### Scenario: Person steps in front of the sensor
- **WHEN** the presence debug screen is visible and the sensor sets its presence flag
- **THEN** the presence indicator turns on, its time since the last change starts again from zero, and the presence history shows the rise relative to the threshold line

#### Scenario: Sensor working
- **WHEN** the presence debug screen is visible and the sensor delivers valid samples
- **THEN** no sensor state line and no error count are shown

#### Scenario: Sensor missing
- **WHEN** the presence debug screen is visible and the sensor is missing
- **THEN** the screen states that the sensor was not found and shows the error count instead of values

#### Scenario: Read failed
- **WHEN** the presence debug screen is visible and the last sensor read failed
- **THEN** the flags, the `person` row, the histories and the heat signal are drawn dimmed as not current, and the bottom line shows the sensor error with the error count
