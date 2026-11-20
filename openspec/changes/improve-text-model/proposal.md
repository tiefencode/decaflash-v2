# Proposal

## Why

The user, 2026-11-20: the verse direction did not feel right; we should have stayed with the first text model (word tokens, 6 × 128, classic corpus) and improved it by best practice, taking the learnings from the verse work. The text model work had become disorganised, so this change is the single place for plan, decisions and state of that improvement.

An independent second opinion (Fable 5.2, 2026-11-20; the lead checked the code-level claims) found that the first model was never tuned where it matters:

- Conditioning: 3,632 classic lines spread over 1,990 distinct event and mood conditions (about 1.8 lines each), with the non-lead levels rolled at random, so the mood conditioning is mostly noise.
- Regularisation: no dropout, weight decay 0.01 only, checkpoints chosen by step count; the 1,000-step run ends at train loss 1.03 against validation loss 3.73.
- Vocabulary: an output head of about 8,000 tokens, mostly book words the corpus never uses; 953 of 2,076 corpus word types occur once.
- Writer: temperature 0.5 and top-k 3 after the first token and no repetition handling; about 70 distinct texts per 100 seeds, and the pattern "ICH BIN KEIN DREHTELLER" in 11 of 12 events.
- Evaluation: decisions rested on 12–30 texts, and Claude judges (60–68 % agreement with the user) did not beat the 59 % "always usable" base rate.

## What Changes

Six steps in a fixed order, each measured on the harness before the next starts (details in design.md):

1. A fixed automatic measurement harness, a gold file of the user's ratings (the old ratings are superseded) and a new blind rating set.
2. The writer: repetition block, word mask per event, fewer once-only words, slightly wider sampling.
3. Conditioning on event and lead value only; the device computes the lead value as the text bank does.
4. Training: dropout, early stopping on validation loss, several seeds.
5. A closed word list of corpus words only.
6. Only then more corpus in the classic style.

Not pursued: pretraining on foreign text, word parts. The verse work stays parked. `mainframe` keeps showing the classic text bank without model texts until an improved model is accepted.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `text-model`: the request carries the event and the lead value instead of five independent levels (step 3). The writer changes of step 2 and the word list of step 5 change spec text only once their behaviour is decided; design.md says what is not yet decidable.

## Impact

- Host tools under `apps/mainframe/tools/` (`text_model.py`, `text_model_harness.py`, `text_model_rating_set.py`, `text_model_vocab.py`, `text_model_write.cpp`), training data and models in the ignored `models/text-model/`.
- Firmware: the writer in `apps/mainframe/src/slm/` (step 2), the request built in the Mainframe (step 3), the model image (steps 3–5); `mainframe` build and device test once an improved model is accepted.
- Docs: `docs/text-model-measurements.md` (harness results per step), README text model section.
- No change to radio, Node, sounds, light or the show clock.
