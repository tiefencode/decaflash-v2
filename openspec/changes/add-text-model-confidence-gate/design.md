# Design

## Context

The text model (change `add-text-model`) samples every token from the top-k logits at a temperature (`text_writer.h`, `sampleTopK`, temperature 0.8, top-k 8) and streams emoji and words to the panel. The panel shows a whole text bank line at once when the runtime is off, when the panel is hidden, or when no emoji arrives within the 2 s wait time (`message_panel.cpp`, `fallBack`); text bank lines in model builds have the wine-red frame. A text cancelled today because a new event arrives stays on the panel until it expires or is replaced.

## Goals / Non-Goals

**Goals:**
- A failsafe that replaces a growing model text with a text bank line when the model is clearly unsure (user decision: "the threshold will always stay a good failsafe").
- Keep the fast emoji and the word-by-word growth; never wait for the whole text before showing it.

**Non-Goals:**
- An early decision after the emoji and the first words: the measurement shows no signal there.
- Changing the product's sampling. A sampling schedule is examined in a separate thread ("Grammatik 1: Auswahl beim Schreiben").
- A mask for verb forms or new pretraining texts (noted only, see Measurement).

## Measurement

All values **[measured, host, 2026-11-18]**, tool `apps/mainframe/tools/text_model_gate.py`, Python twin of the 4-bit kernel (dequantised 4-bit weights, int8 activations), model image `w6x128-pre-ft-400-3e-3` (SHA-256 41394e6a…), temperature 0.8, top-k 8. Not measured on the device kernel; the twin and the kernel agree on greedy tokens but may differ at close choices (add-text-model design.md Decision 11).

- 1,000 random cases (seed 1118; one of twelve events, five mood values 0–100 mapped to levels as `reaction_texts.cpp` does): 900 pass the form check, 100 are verbatim corpus lines.
- Rated texts: 60 new (20 from each tercile of whole-text mean log-probability) plus the 30 earlier ones, 90 in all; 53 usable (59 %), 37 not. Usable per tercile of the new 60: low 10/20, middle 15/20, high 12/20.
- Separation (AUC, 95 % bootstrap interval; 0.5 = none). Whole text: mean log-probability 0.60 (0.48–0.71), mean margin between the two best tokens 0.69 (0.58–0.80), mean entropy 0.68 (0.57–0.79); text length alone 0.64. After the emoji plus 1, 2 or 3 tokens every measure lies between 0.44 and 0.62.
- Running mean margin, checked after every token from the check token on (emoji = token 1, end of text counted; texts have a median of 8 tokens with the end):

  | Check from | Cancelled texts | Bad caught (of 37) | Good lost (of 53) | Usable share of shown model texts |
  |---|---|---|---|---|
  | token 3 | 20 % | 6 | 8 | 59 % |
  | token 5 | 10 % | 5 | 3 | 61 % |
  | token 5 | 20 % | 9 | 6 | 63 % |
  | token 5 | 30 % | 11 | 8 | 63 % |

  With 90 marks the uncertainty is large: the gain from 59 % to 63 % is within the noise. The gate is a failsafe, not a quality fix.
- Failure types of the 37 not-usable texts (Claude's sorting, not corrected by the user): no sense 17, does not fit the event 8, grammar 9 (verb form for the subject 1, case or gender 0, word order 1, sentence breaks off 2, other 5), does not fit the mood 2, other 1. No grammar kind dominates, so a verb-form mask is not indicated by these data.

Raw data: `models/text-model/gate/` (ignored).

### Second measurement with the sampling schedule

The first measurement used temperature 0.8 / top-k 8 for every token; `main` now samples the emoji and the first text token at 0.8 / 8 and every later token at 0.5 / 3 (add-text-model, task group 11), which raises the margins. All values **[measured, host, 2026-11-18]**, tool `apps/mainframe/tools/text_model_measure.cpp` (the firmware's writer on the 4-bit kernel, the device path) with `text_model_gate_cases.py` and `text_model_gate_analyse.py`, same model image.

- 1,200 random cases (seed 1119): 1,041 pass the form check, 159 are verbatim corpus lines; median 8 tokens with emoji and end, 2 % shorter than 5 tokens.
- Labels: the user's own marks were not used as the reference (user decision: the user lets texts pass that sound good). Claude judges calibrated on the 90 marked texts agreed with the user only at 60–68 % (kappa 0.15–0.37); Sonnet and Opus agreed with each other at 84 % (kappa 0.68). A Sonnet and an Opus judge then labelled 600 model texts and 200 text bank lines of the same cases with fixed criteria (sense, grammar, fits the event, fits the mood), blind to source and confidence; a text counts as not usable only when both reject it. Agreement 83 %. Usable: model texts 387/600 (64.5 %), text bank lines 194/200 (97 %). The judges are stricter than the user.
- Separation (AUC, 95 % bootstrap interval): best mean raw log-probability over the whole text 0.62 (0.57–0.67); mean margin whole 0.57 (0.52–0.61), after token 5 0.57, after token 3 0.50.
- Planned rule (running mean margin, every token from token 5): threshold 0.70 now cancels 6.2 % and gains nothing over blind mixing (panel usable 66.3 % vs 66.5 %); threshold 1.00 cancels 19.4 % of all cases: panel usable 71.8 % against 70.3 % for the same text bank share at random; at a cancel about 72 % of the text (median) is already on the panel.
- The gain over blind mixing is 1–3 points at every text bank share, within the noise. The user decided to try the gate on the device anyway (2026-11-18), as the user judges Opus too strict.

Raw data: `models/text-model/gate2/` (ignored).

## Decisions

### 1. Measure: running mean of the margin (measured choice)

The margin between the two best tokens separated best over the whole text and is cheap on the device: in `sampleTopK` the top-k logits are already sorted, so the value is `(logit[best0] - logit[best1]) / temperature`. It does not depend on which token was drawn. The running mean includes the emoji and the end of text, as measured.

### 2. Check token: 5 (user decision, 2026-11-18)

The first check is after token 5 (emoji plus four text tokens); then after every further token up to and including the end of text. A text with fewer tokens is not checked.

Later checks were compared at 20 % cancels **[measured, host]**: from token 6, 7, 8, 9 or 10 the usable share stays at 63–65 % (differences of one to three texts, within the noise), while the cancel moves later (median at 62 % of the text from token 5, 80 % from token 8, 91 % from token 10) and more texts end before the check (1 % from token 5, 34 % from token 8, 74 % from token 10). The user kept token 5.

### 3. Threshold: about 20 % cancels (user decision, 2026-11-18)

Calibration value: running mean margin **1.00** **[measured, host]** — the value at which about 20 % of the 1,200 cases of the second measurement are cancelled with the sampling schedule (19.4 %; `text_model_write` rejects 80 of the first 400). The first value, 0.70, was calibrated under the old sampling and now cancels only 6 %. The margin of a step is taken at that step's temperature. The device test checks the share; after a new model image or a sampling change, recalibrate with `text_model_measure` and `text_model_gate_analyse.py` so that about 20 % of texts are cancelled.

### 4. Panel on a cancel: whole text bank line at once (user decision, 2026-11-18)

The growing text and its emoji disappear; the text bank line for the same event and the mood snapshot of the event's acceptance appears whole, as every text bank line does today, with its own emoji. The display time starts anew for the bank line. Unlike today's cancel by a new event, the partial text does not stay.

### 5. Frame: wine-red like every text bank line (user decision, 2026-11-18)

### 6. Serial: one line per cancel (user decision, 2026-11-18)

Format: `TEXT gate t=<ms> token=<n> mean=<x.xx> threshold=<x.xx> partial="<emoji words>" bank="<line>"`. The existing `TEXT fallback` line keeps its meaning (emoji wait time exceeded).


In addition, the model task's `TEXT` line of every request carries the gate's decision over the delivered tokens, `gate=kept|rejected gate_token=<n> gate_mean=<x.xx> threshold=<x.xx>`, and the host tool `text_model_write` prints the same fields for a seed, so device and host decisions can be compared per request.
### 7. Where the logic lives

The model task only attaches the margins to each piece (`TextModelPiece`). The running mean, the check and the cancel live in `ReactionTexts`/`MessagePanel` on the main loop, so they are host-testable with the existing test doubles.

## Risks / Trade-offs

- [Small effect, uncertain] → Kept as a failsafe at a moderate rate; the device test only checks that the rate and timing are as measured.
- [A replaced text was half read] → The user chose this over waiting for the whole text; check 5 avoids cancels after one or two words, which carry no signal.
- [Twin vs. device kernel] → The device test compares the cancel share with the host value.
- [Threshold depends on the model image] → Recalibrate per image with the committed tool.

## Noted, not started

- A mask that allows only verb forms fitting the subject: only worth it if one grammar error kind dominates, which the sorting above does not show.
- A pretraining text of short simple German sentences in the model's word list instead of the novels.
