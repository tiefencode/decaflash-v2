# Spec Delta

## MODIFIED Requirements

### Requirement: Mood snapshot

Other parts SHALL read the mood only as a snapshot of whole points, each internal value rounded down, together with the sleep state. The main loop SHALL take one snapshot per pass after the personality updates and pass it to the depression poems, the mood sounds, the Node light directive, the eye, the debug overlay and the text model request of the message panel. No other part, the text model included, SHALL change the mood.

#### Scenario: Fraction below a threshold
- **WHEN** a mood value is internally just below 90
- **THEN** the snapshot reports 89

#### Scenario: One pass, one view
- **WHEN** the eye, the sounds, the light directive and a text model request read the mood in the same loop pass
- **THEN** they all see the same snapshot

#### Scenario: Text after a tap
- **WHEN** a Tap raises annoyance and the panel requests a model text for it in the same loop pass
- **THEN** the request carries the snapshot taken after the Tap was applied to the personality
