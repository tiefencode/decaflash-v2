# Spec Delta

## ADDED Requirements

### Requirement: Texts per presence event

Each presence event (`Kommt davor`, `Läuft vorbei`, `Bewegt sich viel`, `Geht weg`) SHALL have its own fixed set of two to four German texts in capital letters, each ending in a curated emoji. The panel SHALL pick the variant as for motion texts, per presence event kind. The texts are listed in this change's design.md and move to Current values when the change is archived.

#### Scenario: Someone stays in front
- **WHEN** `Kommt davor` reaches the panel outside its cooldown
- **THEN** the panel shows a text from the `Kommt davor` set, such as `BIST DU AUCH GRUFTI? ⚰️`

#### Scenario: Someone leaves
- **WHEN** `Geht weg` reaches the panel outside its cooldown
- **THEN** the panel shows a text from the `Geht weg` set, such as `TSCHÜSS. 😔`

### Requirement: Presence texts equal to motion texts

Presence texts SHALL be treated exactly like motion texts: the same cooldown, kept per presence event kind, and the same priority. A presence text SHALL replace a visible poem or a visible motion or presence text of another kind, and a motion text SHALL replace a visible presence text the same way.

#### Scenario: Two visitors in a row
- **WHEN** a `Kommt davor` text was shown and another `Kommt davor` arrives before the cooldown has passed
- **THEN** the panel ignores the second one

#### Scenario: Gesture right after a presence text
- **WHEN** a `Kommt davor` text is visible and a Tap arrives outside the Tap cooldown
- **THEN** the panel replaces it with a Tap text at once

#### Scenario: Presence right after a gesture text
- **WHEN** a Tap text is visible and `Läuft vorbei` arrives outside its cooldown
- **THEN** the panel replaces it with a `Läuft vorbei` text at once

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

### Requirement: Texts per motion kind

Each motion kind SHALL have its own fixed set of two to four German texts in capital letters, each ending in a curated emoji. MultiTap SHALL use a double-tap set for two taps and a separate set for three or more. The same emoji MAY end texts of different kinds. Tilt SHALL produce a text although it changes no mood value. The texts are listed under Current values.

#### Scenario: Triple tap
- **WHEN** a MultiTap event with a count of 3 reaches the panel
- **THEN** the panel shows a text from the three-or-more set, such as `NICHT SO WILD. 😤`

#### Scenario: Double tap
- **WHEN** a MultiTap event with a count of 2 reaches the panel
- **THEN** the panel shows a text from the double-tap set, such as `WER IST DA? 🙂`

### Requirement: Text variant selection

The panel SHALL pick the text deterministically from the event's sequence number modulo the number of variants. If the event has the same motion kind as the last text shown and the pick equals that text's variant, the panel SHALL take the next variant instead, wrapping around. The panel SHALL use no random source.

#### Scenario: Same variant twice in a row
- **WHEN** a Tap with sequence 0 was shown and a later Tap with sequence 2 arrives after its cooldown
- **THEN** the pick 2 mod 2 = 0 equals the last variant, so the panel shows variant 1, `WER KLOPFT SO SPÄT? 🫢`
