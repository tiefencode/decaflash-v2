# Design

## Context

- Today `MessagePanel` holds fixed sets of two to four texts per motion kind and presence event (32 texts, `apps/mainframe/src/message_panel.cpp`). It picks one by the event's sequence number, starts the cooldown per kind, and shows the text with its trailing emoji laid out as the leading element (`add-text-model` group 1, on this branch). `ReactionTexts` (`reaction_texts.{h,cpp}`) routes each accepted event either to a model request or to the fixed path; without a runtime every event takes the fixed path.
- The reviewed corpus lives in `apps/mainframe/text_model/corpus/<event>.tsv`. Each line is five mood levels, a tab, and the text in model order (`<emoji> <WORDS>`). The plan files `apps/mainframe/text_model/plan/<event>.tsv` hold the lead value of every planned line in the same order. The 32 former fixed texts follow the planned lines in each file; their levels were chosen to fit their content, but their lead value is not stored anywhere. Size on 2026-11-18: 3,632 lines (300 per event plus the fixed texts), about 80 KB of text (counted by the parent session).
- `apps/mainframe/tools/text_model_corpus.py check` validates the corpus against the plan, the fixed texts and the form check. `apps/mainframe/tools/text_model_vocab.py` already follows the pattern "Python generator writes a firmware header, `--check` in a host test".

## Goals / Non-Goals

**Goals:**

- Reaction texts from the bank in the normal `mainframe` build, chosen by event and lead mood value, without repeats inside a round.
- One source of truth for the texts: the corpus files.

**Non-Goals:**

- Any change to cooldowns, display times, layout, poems, the panel's hidden-state rules or the mood rules.
- Persisting the shuffle order, or changing the text without a firmware build.
- The model path; `add-text-model` is revised separately to use the bank as its fallback.

## Decisions

### 1. Lead values of the former fixed texts are recorded next to the plan

- A new file `apps/mainframe/text_model/plan/fixed.tsv` holds one line per former fixed text: event file, lead value, text in model order. The lead values come from the choice the corpus session made when it tagged them. If that choice cannot be recovered, the value at low or high that best fits the text is set, and the user sees the list in the device-test step.
- `text_model_corpus.py check` gains a rule: every former fixed text has exactly one entry, and the entry's lead value matches the line's level for that mood value (lead annoyance high requires `ANNOYANCE_HIGH` in the line).
- Alternative considered: deriving the lead value from the five levels. Several levels are often at low or high, so this is ambiguous.

### 2. A generated table in flash

- `apps/mainframe/tools/text_bank.py` reads the corpus, the plan files and `fixed.tsv`, and writes `apps/mainframe/src/text_bank_data.h`. It is grouped by event, then by lead value, so every (event, lead) group is a contiguous range. It holds:
  - an offset table, 12 events × 10 lead values + 1;
  - one `const char*` per line, in panel order (`<WORDS> <emoji>`), so `MessagePanel::show` lays out the emoji as today.
- `--check` compares the header with what it would write. `tests/run_text_bank.sh` runs that check and a C++ test over the header: every event has lines, every line ends in a curated emoji, every line occurs once, the line count matches the corpus.
- Expected flash: about 80 KB of text plus 3,632 pointers (about 15 KB) **[computed]**. The `mainframe` image is about 1.07 MB in a 3.3 MB app slot **[measured, build output 2026-11-17]**.
- Alternative considered: a file in its own flash partition (user chose the generated table, 2026-11-18). It would let texts change without a firmware build, at the cost of a second flash step and a loader.

### 3. Lead value and shuffled rounds

- `apps/mainframe/src/text_bank.{h,cpp}` provides `TextBank`, which has no hardware dependencies.
- Lead value: the mood value with the largest distance from 50, at low below 50 and at high otherwise. Equal distances are broken by the random source (user decision, 2026-11-18).
- Rounds: one index array of all lines (uint16, about 7.3 KB) and one cursor per (event, lead) group.
  - When a group's cursor is at the start, its range is shuffled (Fisher–Yates), then the cursor walks the range.
  - At the end of the range the cursor resets, and the next pick reshuffles.
  - A group without lines falls back to the event's whole range, with its own cursor.
- Random source: a function the caller passes in. The device uses `esp_random()`; host tests use a seeded generator.
- Alternatives considered (user chose shuffled rounds, 2026-11-18): blocking only the last few lines, or plain random picks.

### 4. Panel and router integration

- `MessagePanel` keeps cooldowns and display. Its fixed lists, the variant selection, `fixedReactionText` and `fixedReactionTextCount` are removed.
- `showMotion`, `showPresence` and `fallBack` take the mood snapshot and ask the bank only after the cooldown accepted the event. An ignored event therefore does not advance a round.
- An open model request stores the mood snapshot of its acceptance, so a late fallback uses the same lead value.
- `ReactionTexts::onMotion` and `onPresence` already receive the snapshot; they pass it on. `main.cpp` keeps passing `personality.snapshot()` right after `onMotion` / `onPresence`.
- `tests/text_model_form.cpp` checked the fixed texts; it reads the bank lines instead, so the form check covers every bank line.

## Risks / Trade-offs

- [The corpus changes and the header is stale] → `tests/run_text_bank.sh` fails in `sh tests/run_all.sh`.
- [Lines that fit the lead value but not the rest of the mood] The non-lead levels of a line are random. → Accepted; this is the baseline the user chose. Their device check covers it.
- [Loop time] A pick is a table lookup and, once per round, a shuffle of at most about 30 entries **[computed]**. No measurable effect is expected; the device test watches the text state as before.
- [Internal RAM] About 7.5 KB for the index array and cursors **[computed]**. If internal RAM is short, it goes to PSRAM.

## Migration Plan

- Flash the `mainframe` build with the bank. Rolling back means flashing the previous build; no stored state is involved.
- At archive, the fixed-text lists under `message-panel` Current values are replaced by a pointer to the corpus and the bank size.
- `add-text-model` is revised afterwards (proposal, design, the `message-panel` delta, tasks): its fallback becomes the bank and the model's rating compares against the bank.
