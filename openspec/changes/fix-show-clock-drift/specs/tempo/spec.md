# Spec Delta

## MODIFIED Requirements

### Requirement: Free-running show clock

While the show runs, the main loop SHALL advance the clock by one beat when the next beat is due and schedule the following beat one beat interval after the time that beat was due, not after the time the loop noticed it. When the loop notices beats one or more whole intervals late, it SHALL count every missed beat at once and keep the beat grid. After the last beat of a bar it SHALL move to beat 1 of the next bar and send a ClockSync as defined in the protocol spec. A beat-in-bar value beyond the bar length SHALL also move to beat 1 of the next bar.

#### Scenario: Bar after the show start
- **WHEN** the show has started on bar 1, beat 1 and four beats have become due
- **THEN** the clock has shown beats 2, 3 and 4 of bar 1 and is on beat 1 of bar 2, and only that beat sent a ClockSync

#### Scenario: Beat noticed late
- **WHEN** the main loop notices a due beat some milliseconds after its due time
- **THEN** the following beat is due one beat interval after the due time of the noticed beat, so the lateness is not passed on to later beats

#### Scenario: Whole beats missed
- **WHEN** the show is on beat 3 of a bar and the main loop next runs after three further beats have become due
- **THEN** the clock is on beat 2 of the next bar, and the following beat is due on the unchanged beat grid
