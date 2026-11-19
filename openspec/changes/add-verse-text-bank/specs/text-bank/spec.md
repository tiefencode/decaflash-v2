# Spec Delta

## MODIFIED Requirements

### Requirement: Bank content

The Mainframe SHALL hold a text bank of reaction texts built into the firmware. Every line SHALL belong to exactly one of the twelve text events (the eight motion texts with MultiTap split into two taps and three or more, and the four presence events with text) and carry exactly one lead mood value: one of the five mood values at low or high. A line SHALL be one curated emoji followed by German text in capital letters. There SHALL be two banks: the classic bank from the current corpus and the verse bank from the verse corpus. A verse line MAY contain line breaks between its verse lines. The firmware SHALL contain exactly one bank, chosen at build time; the default build SHALL contain the verse bank.

#### Scenario: Former fixed text
- **WHEN** the classic bank is built
- **THEN** it contains `NICHT SCHLAGEN!` with its emoji as an Impact line

#### Scenario: Every event covered
- **WHEN** either bank is built
- **THEN** every one of the twelve text events has lines

#### Scenario: Verse bank by default
- **WHEN** the `mainframe` environment is built
- **THEN** the firmware picks reaction texts from the verse bank, and the `mainframe-classic` environment picks them from the classic bank

### Requirement: Bank in step with the corpus

Each bank SHALL contain exactly the lines of its reviewed corpus, the classic bank today's former fixed texts included. A host check SHALL fail when a built-in bank differs from its corpus.

#### Scenario: Corpus changed
- **WHEN** a line of either corpus is edited or removed and its built-in bank is not regenerated
- **THEN** the host check fails

## ADDED Requirements

### Requirement: Verse corpus

The verse corpus SHALL hold 30 verses for each of the twelve text events, mixing free short verse, terse lines and one-sentence sayings, and for every event at least three verses for each of the ten lead values. Every verse SHALL make its event recognisable; the mood SHALL only colour it. Verse body text SHOULD stay within 47 characters without line breaks; the bank generator SHALL warn about longer verses and still build them in.

#### Scenario: Long verse
- **WHEN** a verse body has 52 characters
- **THEN** the generator prints a warning and the verse is in the bank
