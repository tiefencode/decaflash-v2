# Spec Delta

## MODIFIED Requirements

### Requirement: Mainframe clock sending

While the show runs, the Mainframe SHALL send `ClockSync` with the current show tempo at show start, on beat 1 of every bar, and immediately on every tempo adoption. The show starts at bar 1, beat 1 with four beats per bar. A tempo adoption SHALL move the show clock to beat 1 of the next bar before the ClockSync is sent. Before the show starts the Mainframe SHALL NOT send ClockSync. When several missed beats are counted at once, ClockSync SHALL be sent only if the last one is beat 1.

#### Scenario: Beat 1 of a bar
- **WHEN** the show clock reaches beat 1 of a bar
- **THEN** the Mainframe sends ClockSync with beat in bar 1 and the current bar number

#### Scenario: Other beats
- **WHEN** the show clock reaches beat 2, 3 or 4 of a bar
- **THEN** the Mainframe sends no ClockSync for that beat

#### Scenario: Tempo adoption
- **WHEN** the show is running and the tempo adoption accepts a new BPM
- **THEN** the Mainframe sets the show tempo, advances to beat 1 of the next bar and sends ClockSync immediately

#### Scenario: Bar start among missed beats
- **WHEN** the show clock counts several missed beats at once, passes beat 1 of a bar and ends on beat 2
- **THEN** the Mainframe sends no ClockSync for that bar

#### Scenario: Missed beats ending on a bar start
- **WHEN** the show clock counts several missed beats at once and the last of them is beat 1 of a bar
- **THEN** the Mainframe sends one ClockSync with beat in bar 1 and the current bar number
