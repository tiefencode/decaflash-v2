# Spec Delta

## Purpose

On-device debug screens of the Mainframe that show internal values for checking and calibrating on the device, and how the front button opens, switches and closes them.

## ADDED Requirements

### Requirement: Debug screens

The Mainframe SHALL provide two debug screens: the mood debug screen, drawn over the running eye (the five mood values, the last motion event and the tempo), and the presence debug screen, which replaces the eye image completely. At most one debug screen SHALL be visible at a time. While a debug screen is visible, the message panel SHALL NOT be drawn.

#### Scenario: Mood debug screen
- **WHEN** the mood debug screen is visible
- **THEN** it shows energy, annoyance, attention, loneliness and depression with a number and a bar each, the last motion event with its tap count, and the show tempo with the current tempo candidate

#### Scenario: Debug screen hides the panel
- **WHEN** a debug screen is visible and a message is due on the panel
- **THEN** the debug screen stays visible and the panel is not drawn

### Requirement: Opening and closing debug mode

A long press of the front button SHALL toggle debug mode. Entering debug mode SHALL show the mood debug screen. Leaving debug mode SHALL return to the normal eye view. The long press SHALL NOT start the show or select a scene.

#### Scenario: Open debug mode
- **WHEN** no debug screen is visible and the user holds the front button past the hold threshold
- **THEN** the mood debug screen appears

#### Scenario: Close debug mode from either screen
- **WHEN** the mood debug screen or the presence debug screen is visible and the user holds the front button past the hold threshold
- **THEN** the normal eye view returns

### Requirement: Switching debug screens

While debug mode is active, a short press of the front button SHALL switch between the mood debug screen and the presence debug screen. In debug mode a short press SHALL NOT start the show, start the audio analysis or select the next scene.

#### Scenario: Switch to presence
- **WHEN** the mood debug screen is visible and the user presses the front button briefly
- **THEN** the presence debug screen appears and the show and scene stay unchanged

#### Scenario: Switch back to mood
- **WHEN** the presence debug screen is visible and the user presses the front button briefly
- **THEN** the mood debug screen appears

#### Scenario: Short press outside debug mode
- **WHEN** no debug screen is visible and the user presses the front button briefly
- **THEN** the Mainframe starts the show and requests the audio analysis if the show is not running, or selects the next scene if it is

### Requirement: Presence debug screen labels

Every value on the presence debug screen SHALL carry a written-out name, such as `presence`, `motion` or `ambient shock`, not a single-letter abbreviation. The raw ambient shock value SHALL stay off the screen, because the ambient shock flag carries its meaning; it is available in the serial lines.

#### Scenario: Reading the screen without a legend
- **WHEN** the presence debug screen is visible
- **THEN** each flag row, history and number is named in words on the screen itself

### Requirement: Presence debug screen contents

The presence debug screen SHALL show, for each of the presence, motion and ambient shock flags, its state and the time since it last changed; a recent history of the presence value and of the motion value, each titled with its current value and with the sensor's detection threshold drawn as a line; the raw object signal; the error count; and the ambient temperature in degrees Celsius. A sensor state line SHALL appear only when the sensor is missing or a read failed.

#### Scenario: Person steps in front of the sensor
- **WHEN** the presence debug screen is visible and the sensor sets its presence flag
- **THEN** the presence indicator turns on, its time since the last change starts again from zero, and the presence history shows the rise relative to the threshold line

#### Scenario: Sensor working
- **WHEN** the presence debug screen is visible and the sensor delivers valid samples
- **THEN** no sensor state line is shown, and the error count is shown next to the object signal

#### Scenario: Sensor missing
- **WHEN** the presence debug screen is visible and the sensor is missing
- **THEN** the screen states that the sensor was not found and shows the error count instead of values

#### Scenario: Read failed
- **WHEN** the presence debug screen is visible and the last sensor read failed
- **THEN** the flags, histories, titles and object signal are drawn dimmed as not current, and the bottom line shows the sensor error with the error count
