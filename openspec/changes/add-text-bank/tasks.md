# Tasks

Device for the device task: the Mainframe (AtomS3R with Atomic Voice Base and Unit TMOS PIR). Flashing goes through the upload command's permission prompt. Ask before opening the serial port. Record device results in `docs/project-context.md`, separate from host results.

## 1. Bank data and generator (host)

- [ ] 1.1 Record the lead value of each of the 32 former fixed texts in `apps/mainframe/text_model/plan/fixed.tsv` (design.md Decision 1). Take the values from the corpus session's tagging, otherwise set the best-fitting value and list it for the user. Extend `text_model_corpus.py check` so every fixed text has exactly one entry whose lead value matches the line's level. Verify `python3 -I apps/mainframe/tools/text_model_corpus.py check` reports 0 problems.
- [ ] 1.2 Add `apps/mainframe/tools/text_bank.py`, which writes `apps/mainframe/src/text_bank_data.h` grouped by event and lead value, texts in panel order (Decision 2), with a `--check` mode. Add `tests/run_text_bank.sh` with the header check and a C++ test: every event has lines, every line ends in a curated emoji, no line twice, line count equals the corpus. Verify `sh tests/run_all.sh` passes.

## 2. Selection and panel (host)

- [ ] 2.1 Add `TextBank` in `apps/mainframe/src/text_bank.{h,cpp}`: lead value from a mood snapshot and shuffled rounds per event and lead value with the fallback to the whole event (Decision 3), random source passed in. Host test `tests/text_bank.cpp` covers the scenarios of the `text-bank` delta: extreme value, all middle, tie at random (seeded), every line once per round, new round after the last, group without lines. Verify `sh tests/run_all.sh` passes.
- [ ] 2.2 Wire the bank into `MessagePanel` and `ReactionTexts` (Decision 4). Remove the fixed lists, the variant selection and `fixedReactionText*`. The bank is asked only after the cooldown accepted the event; a model request keeps its acceptance snapshot for the fallback. Update `tests/message_panel.cpp`, `tests/reaction_texts.cpp` and `tests/text_model_form.cpp` (every bank line passes the form check). Verify `sh tests/run_all.sh` passes and `pio run -e mainframe -e mainframe-slm-capture -e mainframe-loop-load -e mainframe-panel-frame-timing` builds; record the flash size of `mainframe` before and after.

## 3. Device test

- [ ] 3.1 Device test (Mainframe, `mainframe`). Open the mood debug screen to read the mood, then close it, because the panel is hidden while it shows. Give a few knocks, one shake and one presence event: in a calm mood, after several knocks (annoyance rises) and when someone leaves.
  - Verify each text comes from the bank, fits its event and the lead value of that moment, and that the same event shows a different line next time.
  - Show the user the list of former fixed texts with the lead values set in 1.1.
  - Record what was observed. Keep it short; no long hand phases.

## 4. Docs and follow-up

- [ ] 4.1 Write the results into `docs/project-context.md` (host and device apart, what was not verified on hardware), run `openspec validate add-text-bank --strict`, and prepare the `message-panel` Current values text for the archive. The fixed-text lists are replaced by a pointer to the corpus and the bank size.
- [ ] 4.2 Revise `add-text-model` with the `openspec-update-change` skill. The fallback becomes the bank, the 30-case rating compares the model with the bank, and its `message-panel` and `personality` deltas are rebased on this change's requirements. Verify `openspec validate add-text-model --strict`.

## Workflow follow-up

- Archive `add-text-bank` after the user's yes, syncing the `text-bank`, `message-panel` and `personality` specs.
- Merge the branch into `main` only after the user agrees.
