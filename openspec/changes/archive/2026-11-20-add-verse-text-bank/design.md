# Design

## Decision 1: Verse corpus format

One TSV file per event, named like the current corpus files, in `apps/mainframe/text_model/verse_corpus/`. A line is `LEAD<TAB>FORM<TAB>TEXT`: the lead mood value (for example `ANNOYANCE_HIGH`), the form (`a` free short verse, `b` terse lines, `c` saying) and the text as in the current corpus (`<emoji> <WORDS>`), with ` / ` between verse lines. Unlike the current corpus there are no five-level vectors and no plan files: the lead value is written directly, because the user assigns it while reviewing.

## Decision 2: Line break in the bank

The generator writes ` / ` as `\n` into the C string. `\n` is the only control character a bank line may contain. The emoji still moves to the end of the panel text (panel order), so the panel finds it as the leading element as before.

## Decision 3: Build-time choice

`text_bank.cpp` includes `text_bank_verse_data.h` unless `DECAFLASH_TEXT_BANK_CLASSIC=1` is defined, then `text_bank_data.h`. Both headers declare the same names in different namespaces (`text_bank_data`, `text_bank_verse_data`); `text_bank.cpp` aliases the chosen one. `mainframe-classic` extends `mainframe` and adds the flag. Host tests build the verse bank by default and the classic bank once with the flag.

## Decision 4: Panel layout

`nextSegment()` ends a segment at `\n` and skips it as the separator, like a space at a line break. Two `\n` in a row yield no empty row (no blank verse lines, user decision 2026-11-20). Character counting and display time are unchanged; a row created by a line break counts as one element.

## Decision 5: Length guide

`text_bank.py` counts body characters after the emoji without the ` / ` separators (one space instead) and prints a warning for more than 47. The build continues (user decision 2026-11-20). Verses longer than 47 would need shortening before they could train the text model.
