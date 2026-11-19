## MODIFIED Requirements

### Requirement: Loneliness rises over time

Loneliness SHALL rise up to 100 in every state, before and after the analysis is requested, awake or asleep, at a rate that depends on presence: fastest while nobody is present, slower while a person is present in the far zone, and not at all while a person is in the near zone (rates in the presence-mood spec and in design.md). Only motion events and presence events SHALL lower it. Audio SHALL NOT change loneliness.

#### Scenario: Nobody touches the device
- **WHEN** nobody is present and no motion event occurs for ten minutes
- **THEN** loneliness reaches 100 and stays there

#### Scenario: Music does not help
- **WHEN** the same music plays at a slow and at a fast tempo for the same time without motion
- **THEN** loneliness is the same in both cases
