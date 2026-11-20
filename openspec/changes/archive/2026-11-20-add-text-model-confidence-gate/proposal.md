# Proposal

## Why

The text model writes the creature's reaction texts on the Mainframe panel, and its emoji arrives after 0.6–0.7 s, so the text bank fallback practically never appears. In the user's ratings only 53 of 90 text model texts were usable (59 %), against 28 of 30 text bank lines. The user wants a safety net: when the model is clearly unsure while a text grows, the panel replaces it with a text bank line. The fast emoji and the word-by-word growth stay; the panel never waits for the whole text before showing it (user decisions, 2026-11-18).

A host measurement on 1,000 random cases and 90 rated texts showed that confidence carries no signal in the first two or three words, and only a moderate signal over the whole text (design.md, Measurement). The gate is therefore a late cancel with a modest effect, kept as a failsafe, not a fix of the text quality.

## What Changes

- The text model reports a confidence value for each token it chooses: the margin between the two best tokens of the sampling distribution.
- While a model text grows, the panel keeps the running mean of these values. From a fixed token on, when the running mean falls below a threshold, the request is cancelled, the growing text and its emoji disappear, and a text bank line for the same event and mood snapshot appears at once, whole, with the text bank frame colour.
- The Mainframe prints one serial line per cancel.
- A host test covers the gate logic; a short device test checks the share of cancels and the timing.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `message-panel`: a growing model text is cancelled and replaced by a text bank line when the model's running confidence falls below the threshold.
- `text-model`: the model delivers a confidence value with each piece. This capability is introduced by the change `add-text-model`, which must be archived before this one.

## Impact

- Firmware: `apps/mainframe/src/slm/text_writer.h` (margin per token), `slm_task` piece hand-over, `reaction_texts.cpp`, `message_panel.cpp`, serial report in `main.cpp`. Normal `mainframe` build only; no change under `shared/include`, so the Nodes are not affected.
- Tests: `tests/reaction_texts.cpp` and `tests/message_panel.cpp` extended.
- Tools: `apps/mainframe/tools/text_model_gate.py` (measurement, already committed) for recalibration after a new model image.
