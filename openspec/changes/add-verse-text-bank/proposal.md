# Proposal

## Why

On 2026-11-19 the user chose a new voice for the reaction texts: short unrhymed verse in the tone of Rilke and Hölderlin, in three mixed forms (free short verse of two to four lines, very terse lines of one to three words, one-sentence sayings). See `ideas.md`, entry "Reaktionstexte als reimlose Kurzverse, Vortraining auf Lyrik". The user wants to see the voice on the device while keeping the current texts available to switch back.

## What Changes

- A second corpus, the verse corpus, in `apps/mainframe/text_model/verse_corpus/`: 30 verses for each of the twelve text events, each with one lead mood value, at least three per lead value for tap, shake and left (the 90 approved sample verses plus top-ups). The user reviews every verse and every lead value.
- A verse line break, written ` / ` in the corpus, is kept in the text bank and starts a new panel row. A verse line wider than eight columns still wraps at word boundaries; more rows scroll as today.
- The verse bank becomes the default: `pio run -e mainframe` (and the environments that extend it) builds the verse bank. A new environment `mainframe-classic` builds the current bank. Switching back = build `mainframe-classic` and flash.
- The 47-character body limit becomes a guide for the verse corpus: the generator warns about longer verses but builds them in (line breaks do not count). The current corpus and its bank stay untouched.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `text-bank`: two banks (current and verse), chosen at build time; verse lines may contain line breaks.
- `message-panel`: requirement "Line layout" gains the forced line break.

## Impact

- New: `apps/mainframe/text_model/verse_corpus/*.tsv`, `apps/mainframe/src/text_bank_verse_data.h` (generated).
- Changed: `apps/mainframe/tools/text_bank.py` (second bank, `--verse`), `apps/mainframe/src/text_bank.cpp` (bank chosen by build flag), `apps/mainframe/src/message_panel.cpp` (line break), `tests/message_panel.cpp`, `tests/run_all.sh` (check of the verse header), `platformio.ini` (`mainframe-classic`), README.
- Not changed: the text model, its training and its corpus use; the current corpus and `text_bank_data.h`; nothing under `shared/include`, so the Nodes need no flashing.
