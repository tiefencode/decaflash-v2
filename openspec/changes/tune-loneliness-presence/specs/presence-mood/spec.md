## RENAMED Requirements

- FROM: `### Requirement: Loneliness holds while someone is present`
- TO: `### Requirement: Loneliness by presence zone`

## MODIFIED Requirements

### Requirement: Loneliness by presence zone

While a person is in the near zone, loneliness SHALL fall slowly over time. While a person is present only in the far zone, it SHALL rise at a lower rate than while nobody is present at the same loneliness. While nobody is present it SHALL rise at its full rate (rates and the slowing towards 100 in design.md). Gestures and presence events still lower it by their amounts.

#### Scenario: Someone stays in range
- **WHEN** a person stays in range in the near zone and no gesture or presence event occurs
- **THEN** loneliness falls slowly at the near-zone rate

#### Scenario: Someone stays farther away
- **WHEN** a person stays in the far zone and no gesture or presence event occurs
- **THEN** loneliness rises at the far-zone rate, which is lower than the rate with nobody present

#### Scenario: Nobody there
- **WHEN** nobody is present
- **THEN** loneliness rises at its full rate

### Requirement: Near zone

While a person is present, the Mainframe SHALL consider them directly in front (near zone, closer than about 0.7 m) when the presence value reaches its near entering threshold, and SHALL keep that until the value stays below its near leaving threshold for a hold time (values in design.md). Otherwise a present person is farther away (far zone).

#### Scenario: Person steps directly in front
- **WHEN** a present person comes so close that the presence value reaches the near entering threshold
- **THEN** the person is in the near zone

#### Scenario: Person steps back
- **WHEN** a person in the near zone steps back and the presence value stays below the near leaving threshold for the hold time but above the leaving threshold
- **THEN** the person is present in the far zone

#### Scenario: Value swings while standing still
- **WHEN** a person stands still in the near zone and the presence value dips below the near leaving threshold for less than the hold time
- **THEN** the person stays in the near zone and no event is reported
