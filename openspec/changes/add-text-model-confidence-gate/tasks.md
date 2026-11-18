# Tasks

## 1. Confidence per token

- [x] 1.1 Return the margin of the two best top-k logits divided by the temperature from the sampling step in `apps/mainframe/src/slm/text_writer.h`, without changing which token is drawn
- [x] 1.2 Carry the margins of each piece's tokens (emoji, word tokens, end) in `TextModelPiece` through `slm_task`

## 2. Gate on the panel

- [x] 2.1 Keep the running mean in `ReactionTexts`; from token 5 on, compare it with the threshold 1.00 after every token (design.md Decisions 2–3)
- [x] 2.2 On a cancel: cancel the request, remove the growing text and its emoji, show a whole text bank line for the request's event and mood snapshot with the text bank frame (`MessagePanel`)
- [x] 2.3 Print one `TEXT gate` line per cancel in `main.cpp` (design.md Decision 6)

## 3. Host tests

- [x] 3.1 Extend `tests/reaction_texts.cpp` / `tests/message_panel.cpp`: cancel below the threshold at token 5 and later, no check before token 5, short texts complete, confident texts complete, bank line shown whole with the bank frame and the acceptance mood
- [x] 3.2 Host check that sampling with the margin returned gives the same tokens as before for fixed seeds
- [x] 3.3 Run `sh tests/run_all.sh` and `pio run -e mainframe`

## 4. Device test

- [x] 4.1 Flash the normal `mainframe` build to the Mainframe (AtomS3R); let the automatic text triggers run about 1 minute with music; read the serial line (after asking). Observe: share of `TEXT gate` lines among model texts (expected about 20 %, host value), cancels never before token 5, the bank line appears at once with the wine-red frame, emoji and first word times unchanged, no resets or audio drops
- [x] 4.2 Record the measured share and timings as **[measured, device]** in design.md and `docs/project-context.md`; report separately what was verified on hardware
