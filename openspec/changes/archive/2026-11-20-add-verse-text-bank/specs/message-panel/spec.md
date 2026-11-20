# Spec Delta

## MODIFIED Requirements

### Requirement: Line layout

The panel SHALL lay text out in left-aligned lines of at most eight columns. ASCII characters and the German letters Ä, Ö, Ü, ä, ö, ü and ß SHALL take one column, curated emoji two. Lines SHALL break at spaces, and spaces at a line break SHALL be dropped. A line break character in the text SHALL always end the line; it takes no column, and several in a row SHALL yield no empty line. A word wider than a line SHALL be split at the column limit. No break SHALL split a UTF-8 sequence or a curated emoji.

#### Scenario: Word wrap
- **WHEN** the text `NEXT MESSAGE` is shown
- **THEN** the first line is `NEXT` and the second `MESSAGE`

#### Scenario: Inline emoji
- **WHEN** the text `HÜHÜ 🙂 MUSIK 🎵` is shown
- **THEN** the first line is `HÜHÜ 🙂`, filling seven of eight columns

#### Scenario: Verse line break
- **WHEN** the text `KLOPF,` line break `KLOPF,` line break `HERZ. 🙂` is shown
- **THEN** the emoji is element 0 and the lines are `KLOPF,`, `KLOPF,` and `HERZ.`, although `KLOPF, HERZ.` would fit into one line

#### Scenario: Long verse line
- **WHEN** a verse line `WER BLEIBEN WILL.` follows a line break
- **THEN** it starts a new line and wraps into `WER` `BLEIBEN` `WILL.`
