# Spec Delta

## ADDED Requirements

### Requirement: Texts per presence event

Each presence event (`Kommt davor`, `Läuft vorbei`, `Bewegt sich viel`, `Geht weg`) SHALL have its own fixed set of two or three German texts, each ending in a curated emoji that no motion text uses, all pairwise different. The panel SHALL pick the variant as for motion texts, per presence event kind. The texts are listed in this change's design.md and move to Current values when the change is archived.

#### Scenario: Someone stays in front
- **WHEN** `Kommt davor` reaches the panel outside its cooldown
- **THEN** the panel shows a text from the `Kommt davor` set, such as `DA BIST DU JA. 🥰`

#### Scenario: Someone leaves
- **WHEN** `Geht weg` reaches the panel outside its cooldown
- **THEN** the panel shows a text from the `Geht weg` set, such as `SCHON WEG? 😢`

### Requirement: Text cooldown per presence event

After a presence text, the panel SHALL ignore further presence events of the same kind until the same cooldown as for motion texts has passed. The cooldown SHALL be kept per presence event kind and separate from the motion kinds. Presence texts SHALL have the same priority as motion texts.

#### Scenario: Two visitors in a row
- **WHEN** a `Kommt davor` text was shown and another `Kommt davor` arrives before the cooldown has passed
- **THEN** the panel ignores the second one

#### Scenario: Gesture right after a presence text
- **WHEN** a `Kommt davor` text is visible and a Tap arrives outside the Tap cooldown
- **THEN** the panel replaces it with a Tap text at once

## MODIFIED Requirements

### Requirement: Content sources

The V2 Mainframe SHALL feed the panel from exactly three sources: motion events from the IMU classifier, presence events and depression poems. No other mood value, the button, the radio or the tempo path SHALL produce panel text. The Mainframe SHALL NOT forward panel text to Nodes.

#### Scenario: Motion event
- **WHEN** the motion classifier reports an event other than None
- **THEN** the Mainframe first updates the personality with it and then offers it to the panel to pick a text

#### Scenario: Presence event
- **WHEN** a presence event is reported
- **THEN** the Mainframe first updates the personality with it and then offers it to the panel to pick a text

#### Scenario: High annoyance alone
- **WHEN** annoyance rises without a motion or presence event
- **THEN** the panel shows no text because of it
