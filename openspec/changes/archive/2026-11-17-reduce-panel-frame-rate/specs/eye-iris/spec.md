# Spec Delta

## ADDED Requirements

### Requirement: Reduced frame rate under the message panel

In frames in which the message panel is drawn over the eye, the renderer SHALL use a longer frame interval than without the panel. While the panel is not drawn, including while the creature sleeps or a debug screen is shown during a message's display time, the normal frame interval SHALL apply. The change of interval SHALL take effect with the next frame. The intervals are in design.md.

#### Scenario: Message becomes visible
- **WHEN** a message is shown while the creature is awake and no debug screen is on
- **THEN** the eye and the panel are drawn at the reduced frame rate until the message closes

#### Scenario: Message closes
- **WHEN** the display time of the message ends
- **THEN** the next frames are drawn at the normal frame rate

#### Scenario: Message during a debug screen
- **WHEN** a message is active while a debug screen is shown
- **THEN** the debug screen is drawn at the normal frame rate

#### Scenario: Message during sleep
- **WHEN** a message is active while the creature sleeps
- **THEN** the sleeping eye is drawn at the normal frame rate
