# Proposal

## Why

The creature answers motion and presence events with 32 fixed texts that ignore its mood. The reviewed training corpus of `add-text-model` already holds 3,632 texts in the creature's tone, tagged by event and mood. The first on-device model, 3 layers × 64, wrote mostly broken German (host measurement, 2026-11-18). The user therefore decided on 2026-11-18 to build selection from this text bank first, as the baseline: correct German that answers the mood, with no model on the device. The model stays the research goal and later has to beat the bank in the user's rating.

## What Changes

- Reaction texts for motion and presence events come from a text bank of the reviewed corpus lines instead of the fixed sets of two to four texts per event.
- The panel picks a line by the event and the lead mood value of the moment. The lead value is the mood value farthest from the middle, taken at its low or high side even when all five values are in the middle third. A tie is broken at random.
- Within one event and lead value, lines come in shuffled order without repeats until all have been shown; then a new shuffled round starts.
- Today's 32 fixed texts are part of the bank (they are in the corpus); the separate fixed lists in the firmware go away.
- The bank is a table generated from the corpus files and built into the firmware; a host check keeps it in step with the corpus.
- **BREAKING (behaviour):** the panel's text choice now uses a random source and the mood; the deterministic variant selection by sequence number goes away.
- Ships in the normal `mainframe` build.
- Afterwards `add-text-model` is updated so that the model uses the bank as its fallback and competes against it.

## Capabilities

### New Capabilities

- `text-bank`: the reaction texts of the creature: which lines the bank holds, how the lead mood value is found, how a line is chosen and repeats are avoided, and that the built-in bank matches the reviewed corpus.

### Modified Capabilities

- `message-panel`: texts per motion kind and per presence event come from the text bank; variant selection by sequence number is replaced by the bank's selection.
- `personality`: the mood snapshot gains one reader, the text bank's selection; the mood rules are unchanged.

## Impact

- Mainframe code:
  - `apps/mainframe/src/message_panel.{h,cpp}`: fixed lists and variant selection removed, texts from the bank.
  - `apps/mainframe/src/reaction_texts.{h,cpp}`: the fixed-text path becomes the bank path.
  - `main.cpp`: mood passed with the event.
  - A new generated table of about 80 KB in flash, plus selection code.
- Host tools and data:
  - A generator and check script beside `apps/mainframe/tools/text_model_corpus.py`.
  - The leads of the 32 fixed texts recorded next to the plan files.
- Tests: `tests/message_panel.cpp`, `tests/reaction_texts.cpp` and `tests/text_model_form.cpp` (which today read the fixed texts) change, and a new host test covers the bank.
- Not changed: the mood rules, the radio protocol, the Node firmware, `shared/include`, dependency pins, the poems, cooldowns, display times and layout.
- Docs: `docs/project-context.md` (device result), the Current values of `message-panel` at archive.
- Follow-up: `add-text-model` proposal, design, specs and tasks are revised against this change.
