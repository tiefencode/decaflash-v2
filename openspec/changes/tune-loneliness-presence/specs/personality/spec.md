## MODIFIED Requirements

### Requirement: Loneliness rises over time

Loneliness SHALL rise towards 100 in every state, before and after the analysis is requested, awake or asleep, ever more slowly the closer it is to 100, at a base rate that depends on presence: fastest while nobody is present and slower while a person is present in the far zone (rates in the presence-mood spec and in design.md). While a person is in the near zone it SHALL fall slowly instead. Otherwise only motion events and presence events SHALL lower it. Audio SHALL NOT change loneliness.

#### Scenario: Nobody touches the device
- **WHEN** nobody is present and no motion event occurs for ten minutes
- **THEN** loneliness is above 90 and keeps rising, ever more slowly, without passing 100

#### Scenario: Rise slows down
- **WHEN** nobody is present and loneliness rises from 50 to 80
- **THEN** the rise per second at 80 is smaller than at 50

#### Scenario: Music does not help
- **WHEN** the same music plays at a slow and at a fast tempo for the same time without motion
- **THEN** loneliness is the same in both cases
