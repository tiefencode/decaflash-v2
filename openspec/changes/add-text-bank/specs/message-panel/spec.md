# Spec Delta

## MODIFIED Requirements

### Requirement: Texts per motion kind

The panel SHALL take the text for a motion event from the text bank's lines for that motion kind. MultiTap SHALL use the double-tap lines for two taps and the three-or-more lines for three or more taps. Reposition SHALL produce a text although it changes no mood value. A bank line is stored with its emoji first; the panel SHALL show it with the emoji as its leading element and the text below, as for every other message.

#### Scenario: Triple tap
- **WHEN** a MultiTap event with a count of 3 reaches the panel outside its cooldown
- **THEN** the panel shows a line of the three-or-more lines of the text bank

#### Scenario: Double tap
- **WHEN** a MultiTap event with a count of 2 reaches the panel outside its cooldown
- **THEN** the panel shows a line of the double-tap lines of the text bank

## ADDED Requirements

### Requirement: Text selection from the bank

When the panel takes a text for a motion or presence event, it SHALL ask the text bank for a line of that event, with the lead mood value of the mood snapshot taken after the personality has applied the event.

#### Scenario: Annoyed tap
- **WHEN** a Tap raises annoyance to 90 and every other value lies between 40 and 60
- **THEN** the panel shows a Tap line with lead annoyance high

#### Scenario: Two taps in a row
- **WHEN** two Taps with the same lead value are answered one cooldown apart
- **THEN** the panel shows two different Tap lines

### Requirement: Presence texts from the bank

The panel SHALL take the text for a presence event with text (`Kommt davor`, `Läuft vorbei`, `Geht weg`, `Steht weiter weg`) from the text bank's lines for that event, chosen as for motion texts. `Bewegt sich` SHALL show no text.

#### Scenario: Someone leaves while lonely
- **WHEN** `Geht weg` reaches the panel outside its cooldown and loneliness is the value farthest from the middle, at its high side
- **THEN** the panel shows a `Geht weg` line with lead loneliness high

#### Scenario: Someone moves
- **WHEN** `Bewegt sich` is reported
- **THEN** the panel shows nothing because of it

## REMOVED Requirements

### Requirement: Text variant selection

**Reason**: Replaced by "Text selection from the bank" (user decision, 2026-11-18): texts are chosen by event and lead mood value, without repeats, using a random source instead of the event's sequence number.

**Migration**: None; the panel keeps no stored selection state across a restart.

### Requirement: Texts per presence event

**Reason**: Replaced by "Presence texts from the bank": the fixed sets of two to four texts per presence event are part of the text bank now.

**Migration**: The former fixed texts are lines of the corpus and therefore of the bank.
