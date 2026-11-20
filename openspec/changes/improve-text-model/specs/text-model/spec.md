# Spec Delta

## MODIFIED Requirements

### Requirement: Request input

A request SHALL carry the event kind and the lead mood value of the snapshot: one of the five mood values at low or high, chosen by the same rule as the text bank's lead value (text-bank spec, requirement "Lead mood value"). The other four mood values SHALL NOT influence the text. The event kinds SHALL be the eight motion texts (Move, Tap, MultiTap with two taps, MultiTap with three or more, Impact, Rotate, Reposition, Shake) and the four presence events with text.

#### Scenario: Levels of a snapshot
- **WHEN** a Tap is requested with energy 10, annoyance 70, attention 50, loneliness 0 and depression 34
- **THEN** the request carries Tap and loneliness low

#### Scenario: Other values ignored
- **WHEN** two Tap requests have the lead value annoyance high and differ only in the other four mood values
- **THEN** both are computed from the same input and, with the same seed, give the same text

#### Scenario: Calm mood
- **WHEN** every mood value lies in the middle third and loneliness is 38, the farthest from the middle
- **THEN** the request carries the event and loneliness low

## ADDED Requirements

### Requirement: Words of the event

After the emoji, the text model SHALL write only words that occur in the corpus lines of the requested event, plus punctuation and the end of the text. Words that occur only once in the whole corpus SHALL be chosen less often than the model alone would choose them; the strength is a calibration value in design.md.

#### Scenario: Word of another event
- **WHEN** a Tap is requested and the model rates a word that occurs only in Shake lines highest
- **THEN** that word is not written and the text continues with a word of the Tap lines

### Requirement: Repetition block

The text model SHALL remember the texts it wrote recently (the number is a calibration value in design.md) and SHALL avoid repeating them: the first word of a remembered text is chosen less often as the first word, and no three consecutive tokens of a remembered text are written again. A text is remembered when it ended by the end token or the length limit; a cancelled text is not.

#### Scenario: Same event twice
- **WHEN** a Tap text "ICH HABE DEN KLOPFER GEHÖRT." was written and the next request is a Tap with the same mood and seed
- **THEN** the next text does not contain "HABE DEN KLOPFER" or any other three-token sequence of the first text
