# text-bank Specification

## Purpose

The text bank: the corpus lines built into the firmware and the rule that picks one line for an event and the mood of the moment, without a model. Terms are those of the README section "Text model and text bank (host)".

## Requirements

### Requirement: Bank content

The Mainframe SHALL hold a text bank of reaction texts built into the firmware. Every line SHALL belong to exactly one of the twelve text events (the eight motion texts with MultiTap split into two taps and three or more, and the four presence events with text) and carry exactly one lead mood value: one of the five mood values at low or high. A line SHALL be one curated emoji followed by German text in capital letters.

#### Scenario: Former fixed text
- **WHEN** the bank is built
- **THEN** it contains `NICHT SCHLAGEN!` with its emoji as an Impact line

#### Scenario: Every event covered
- **WHEN** the bank is built
- **THEN** every one of the twelve text events has lines

### Requirement: Bank in step with the corpus

The bank SHALL contain exactly the lines of the reviewed corpus, today's former fixed texts included. A host check SHALL fail when the built-in bank differs from the corpus.

#### Scenario: Corpus changed
- **WHEN** a corpus line is edited or removed and the built-in bank is not regenerated
- **THEN** the host check fails

### Requirement: Lead mood value

For a mood snapshot the bank SHALL take as lead value the mood value farthest from the middle of its range, at its low side when below the middle and at its high side otherwise. This SHALL hold also when every value lies in the middle third. When several values are equally far from the middle, one of them SHALL be chosen at random.

#### Scenario: Very annoyed
- **WHEN** annoyance is 90 and every other value lies between 40 and 60
- **THEN** the lead value is annoyance high

#### Scenario: Calm mood
- **WHEN** every value lies in the middle third and loneliness is 38, the farthest from the middle
- **THEN** the lead value is loneliness low

#### Scenario: Tie
- **WHEN** energy is 10 and depression is 90 and the other values are closer to the middle
- **THEN** the lead value is energy low or depression high, chosen at random

### Requirement: Line selection without repeats

For an event and a lead value the bank SHALL return a line of that event with that lead value. Within one event and lead value it SHALL return every such line once, in a random order, before any of them repeats, and SHALL then start a new random round. The order SHALL NOT be kept across a restart. If the bank holds no line for that event and lead value, it SHALL return a line of the same event, chosen the same way among all its lines.

#### Scenario: Thirty lines for a lead
- **WHEN** the bank holds 30 Tap lines with lead annoyance high and 30 Tap events with that lead value are answered
- **THEN** each of the 30 lines is shown exactly once

#### Scenario: Next round
- **WHEN** the thirty-first such Tap is answered
- **THEN** a line from a new random round is shown

#### Scenario: Lead value without lines
- **WHEN** no Rotate line has lead loneliness low and a Rotate event has that lead value
- **THEN** the bank returns a Rotate line with another lead value
