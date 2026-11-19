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
