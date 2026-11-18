# Design

Every number is marked **[measured]** (device, 2026-11-17, see `docs/slm-prototype.md` and `docs/project-context.md` section D), **[computed]** or **[estimated]**. All decisions marked "user decision" were taken in the interview of 2026-11-17.

## Context

- Today the panel picks a reaction text from a fixed list per motion or presence event kind by sequence number; the mood plays no part (`openspec/specs/message-panel/spec.md`). `docs/project-context.md` B planned "state + event → small local language model → short text", with the model as the "mouth" only.
- Prototype results that shape this design:
  - Runtime kernel ported from github.com/slvDev/esp32-ai (MIT), host-tested, output equal to the host on the device; `#pragma GCC optimize("O3")` in `slm_task.cpp` reaches benchmark speed inside the `-Os` firmware **[measured]**.
  - Time per forward pass on core 0: 6 layers × 128 = 98 ms, 2.52 MB PSRAM; 4 layers × 96 = 44 ms, 1.09 MB PSRAM, both with the reference vocabularies of 8,057 / 854 **[measured]**.
  - With continuous generation the core 1 busy share rises by about 10 points, for both sizes alike (idle measured for both); largest audio and IMU gaps exceed the strict limit by 2–11 ms. With the eye at 40 ms without text, everything holds the tolerant limit (+10 ms) **[measured]**. No drops, resets or refused ESP-NOW packets in any run. The cause of the effect on core 1 is not measured.
  - Memory: 5.07 MB PSRAM free with the reference-size model next to the emoji atlas and the pre-rendered sounds **[measured]**.
  - Model partition at 0x340000, 4,915,200 bytes, in `apps/mainframe/partitions_slm.csv`; the image is written with its own flash command.
- Panel facts: 4 rows, 8 columns, 192-byte text capacity, 47 curated emoji, umlauts as own glyphs, ß drawn as `B`.

## Goals / Non-Goals

**Goals:**

- Reaction texts that answer event and mood, written on the device without network.
- The smallest model that passes the quality rule (Decision 9).
- Load on core 1 only while a text is being written, and a show that behaves as today when the model is missing.

**Non-Goals:**

- Poems from the model (later, `ideas.md`), new text sources, texts to the Nodes.
- Any influence of the model on mood, light, sounds, tempo or radio.
- A worker on core 1, a move of the Arduino core, Wi-Fi.

## Decisions

### 1. Scope: reaction texts only (user decision)

The model writes the texts for motion events and for presence events with text (`Kommt davor`, `Läuft vorbei`, `Geht weg`, `Steht weiter weg`). The depression poems stay the fixed list; "poems from the model" goes to `ideas.md` (user decision: later). Alternative considered: poems first (rare, may wait); the user preferred the short, frequent reactions because they should answer the mood.

### 2. Model input: event kind plus all five mood values in three levels (user decision)

- Condition tokens at the start of every request: one event token and five mood tokens, one per value (energy, annoyance, attention, loneliness, depression), each low / middle / high.
- Levels are fixed thirds for all five values (user decision): 0–33 low, 34–66 middle, 67–100 high, taken from the mood snapshot of the loop pass in which the panel accepts the event, after the personality has processed that event.
- Event tokens (12): Move, Tap, MultiTap 2, MultiTap 3+, Impact, Rotate, Reposition, Shake, Kommt davor, Läuft vorbei, Geht weg, Steht weiter weg.
- 243 mood combinations × 12 events cannot be covered one by one in training data; the model has to generalise per value (see Risks).

### 3. Compute on demand (user decision, revised in the interview)

- First choice was a text stock refilled after each use; the user then asked how a stock can match a mood that changes quickly. A stock over all combinations would need about 2,900 slots and about one hour to fill at 4 × 96 **[computed]**, and a gesture changes the mood itself before the text is chosen. Decision: no stock, the model writes when the panel accepts an event.
- The model task stays on core 0 at priority 1 as in the prototype and sleeps while no request is open. Load therefore appears only during writing, not continuously.
- Expected timing at 3 × 64 with word tokens (Decision 6): 7 condition tokens plus about 8–10 output tokens (mean 9.0 per validation text, end included **[measured, host]**); time per forward pass about 15–25 ms **[computed, rough: layer weights about a third of 4 × 96, not measured; the larger output head of word tokens is not included]**, so emoji after about 0.1–0.2 s and the whole text after about 0.2–0.4 s. With character tokens it was about 25–35 tokens and 0.5–1 s.

### 4. Emoji first, words after, panel shows it as it grows (user decisions)

- Training texts are stored as `condition tokens, emoji, text, end`. The first output step is masked to the curated emoji, so every model text has exactly one emoji and it is chosen first; it acts as the intent the words follow. The text body is masked to the text alphabet (Decision 6) and contains no emoji, like today's texts.
- The panel shows the emoji as soon as it arrives and then every finished word (word by word, user decision; character by character was rejected because a growing word jumps to the next line at the column limit).
- The old message stays visible until the emoji of the new one arrives (user decision), then it is replaced.

### 5. Layout: emoji in the first row for all messages (user decision)

- Every message whose last character is a curated emoji (all fixed texts and poems today) and every model text shows that emoji large and centred in the first row, with the text lines below. This replaces "Trailing emoji". Alternatives: text above with the emoji pushed down (keeps today's look), or the new layout for model texts only; the user chose one layout for all.
- The emoji counts as one element for display time and scrolling, as before.

### 6. Vocabulary: one token per word of the corpus or curated emoji (user decision, revised 2026-11-18)

- Text alphabet: space, A–Z, Ä, Ö, Ü, 0–9, `. , ! ? - '`. Lower case and ß are left out, as in today's texts (ß would be drawn as `B`; the corpus writes SS). Every word and every model text is made only of this alphabet.
- Output tokens: an end token, the curated emoji of `panel_glyphs`, the punctuation tokens `. , ! ? -`, and one token per word of the whole corpus (all 3,632 lines, validation lines included, so the shipped model can write every corpus word): 2,048 words **[measured]**. A word is a run of letters, digits and `'` (GEHT'S is one word). The decoder puts a space before every word and none before `. , ! ?`, and no space on either side of `-` (SOLO-ABEND, KLOPF-KLOPF).
- Input vocabulary: the output tokens plus a start token, 12 event and 15 mood tokens; the image format already has separate input and output vocabularies with an output-to-input table (Barista image) **[read]**.
- The tokenizer table is generated from the corpus and the `panel_glyphs` emoji list, so the model can only emit drawable glyphs and words of the corpus; a host test checks this.
- About 2,100 output and 2,130 input tokens: at 3 × 64 the embedding, PLE table and output head make up most of about 0.85 M parameters, roughly 0.5 MB as a 4-bit image **[computed]**.
- Consequence: the model writes only words of the corpus. A new word needs a corpus line and a new training.
- The fixed Shake text `AAH - ICH KOTZE! 🤢` became `AAH, ICH KOTZE! 🤢` in `message_panel.cpp` and in the corpus (user decision, 2026-11-18), because a dash with spaces does not fit the spacing rule. The measurement texts in `main.cpp` keep the old spelling.
- Result of tasks 3.3 and 4.4 (2026-11-18): 2,101 output and 2,129 input tokens; every corpus line passes the form check, which now also checks known words, the length limit and the spacing. A trial run at 3 × 64 (seed 1, 1,000 steps, best validation 3.60 per token at 600 steps) gave a 4-bit image of 495,776 bytes; the kernel equalled the twin in 60 of 60 cases, and 28 of the 30 rating texts passed the form check, the other two were copies of training texts **[measured, host]**. This was a pipeline check, not task 6.1.
- History: the first decision was one token per character (about 110 output tokens). The character model wrote invented words and broken German at 3 × 64 and 4 × 96; the measured comparison that led to word tokens is Decision 14.

### 7. Wait time and fallback (user decisions)

- If the emoji has not arrived 2 s after the panel accepted the event, the panel shows a text from today's fixed list for that event and cancels the request. Once the emoji is visible, the model writes to the end.
- The same fixed list is used, without asking the model, when the runtime is off (no or invalid image, failed allocation) and while the panel is hidden (sleep, debug screen; user decision: no model use while hidden). The variant selection of the fixed list stays as today.
- Length: no row limit (user decision, 2026-11-18: the panel scrolls longer texts, also while they grow, and three of today's fixed texts already need more than four rows). The only limit is the hard length limit: the emoji plus at most 47 text characters, as with the 48 tokens of the character tokens, below the model's position limit and the 192-byte capacity **[computed]** (user decision, 2026-11-18, with the word tokens). Training texts are filtered to it, and the model task ends a text before the word that would cross it.

### 8. A new event while a text is being written (user decision)

The new event wins, as the panel does today: the open request is cancelled, the model starts the new one with the current mood. The cancel is checked by the model task between forward passes, so the old request ends within one forward pass.

### 9. Model sizes and growth rule (user decisions)

- Start at 3 layers × dimension 64. Ladder: 3 × 64 → 4 × 96 → 6 × 128; the last is the largest size measured on the device. Only the next step is trained, and only when the current one fails (user question: not every size has to be done).
- First run (2026-11-18): the character model at 3 × 64 wrote mostly broken German, so the rating was not done; 4 × 96 was trained as a measurement and was only slightly better (Decision 14). The cause was the tokens, not the size: with word tokens the ladder starts again at 3 × 64 (user decision).
- Task 6.1 with word tokens (2026-11-18): 3 × 64, seed 1, 600 steps (lowest validation loss of the 600, 800 and 1,000 step runs: 3.59 per token), image SHA-256 `3c7a9ae86d10883113401994fdd147148d7affafeaa839ce964b482884cd7226`, 495,776 bytes. The kernel equalled the twin in 59 of 60 cases (one late close choice). The 30 rating texts are sampled from the 4-bit twin (temperature 0.8, top-k 8, seed 2026) because the kernel cannot sample before task 7.1; 30 of 30 pass the form check, none is a copy **[measured, host]**. The user rates them in a Claude Doc together with three sampling settings (0.6 / 4, 0.8 / 8, 1.0 / 16).
- Rating of that model (user, 2026-11-18): 16 of 30 usable, 13 not usable, 1 left open (case 14). Below the threshold of 20, so 3 × 64 with word tokens fails the growth rule. The user chose the middle sampling setting.
- Rule: a host check runs first on texts generated by the host kernel from the quantised image (not from the float model): only alphabet glyphs, emoji first and from the curated set, emoji plus text within the hard limit of output tokens, no verbatim copy of a training text. Then the user rates 30 texts for fixed event / mood cases; fewer than 20 usable means one step up. A size that fails the host check also goes up.
- Sampling: the prototype kernel picks greedily; the product samples with temperature and top-k from a per-request seed so the same event and mood give varied texts. Temperature and k are calibration values, chosen during the user's rating; host tests use fixed seeds.

### 10. Training data: written by a Claude Code session, reviewed by the user (user decision)

- A Claude Code session writes the corpus into text files in the repo, each line tagged with event and five mood levels; today's fixed texts are part of it. This runs on the user's Max plan; the Claude API would be billed separately and is not used. The step starts only after the user's explicit yes; the corpus size is agreed with the user at that step.
- The user reviews samples and removes lines. A validation split is held back for the training loss.
- Result (2026-11-18): layout, size and scheme are user decisions of task 5.1. The corpus is one TSV per event in `apps/mainframe/text_model/corpus/` (five levels, tab, emoji and text). Every planned line has one lead value at low or high, and the other four levels are random (`apps/mainframe/text_model/plan/`). Today's 32 fixed texts are included with a lead that fits their content. After the first review the user set the rule that every text must make its event recognizable, and the mood only colours it; 2,022 lines were rewritten for it, together with the user's own edits. The second review round needed no change. Final size: 3,632 lines (300 per event plus the fixed texts); `text_model_corpus.py check` reports no problem.
- Validation split: 10 % of every event file (360 lines), drawn with the fixed seed 52 in `text_model_corpus.split`. The user left this value to Claude.

### 11. Training on the user's Mac (user decision)

- Python training script (PyTorch) and an exporter that writes the kernel's image format (header version 1, 4-bit groups, separate output head). Installing the Python packages is a download and starts only after the user's yes.
- The exporter is verified by a host test (`tests/run_text_model_export.sh`, tool `apps/mainframe/tools/text_model_check.py`): for every event with five mood combinations, the host kernel on the exported image is compared with a Python twin of the kernel's arithmetic (the exported 4-bit weights and int8 activations, as `slm_kernel.h` sets `LLM_INT8_ACT`). Threshold: the kernel equals the twin in greedy output tokens in every case. The comparison with the float model is reported, not required: on the smoke model (3 × 64, seed 1, 600 steps on today's fixed texts) the kernel equalled the twin in 60 of 60 cases and the float model in 54 of 60, the first token included, so quantisation changes close choices **[measured, host, 2026-11-18]**. Text quality is judged on the image itself by the form check and the user's rating (Decision 9). On the first corpus image (3 × 64, seed 1, 2,000 steps, group 32) the kernel equalled the twin in 59 of 60 cases; the one difference starts late in a text at a close choice, which the order of the float sums can flip. The float model and the kernel gave the same text in 3 of 60 cases and the same first token in 49 of 60 **[measured, host, 2026-11-18]**. So the exact threshold holds for the tiny model of the host test; for trained images a late difference at a close choice is reported and not counted as an exporter fault.
- Images are not committed; corpus, scripts, settings and seed are, and the SHA-256 of each image used on the device is recorded in `docs/project-context.md`.

### 12. Builds: own env first, then the normal build (user decision)

- New product env `mainframe-slm` (extends `mainframe`, `-DDECAFLASH_SLM=1`, `partitions_slm.csv`, no measurement flags). `mainframe-slm-capture` gets the request mode with the existing automatic text triggers for load measurements.
- After the device acceptance (Decision 13) and the user's yes, the normal `mainframe` env takes the partition table and the runtime, and `mainframe-slm` is removed. Consequence the user accepted: no second app slot for OTA and no SPIFFS in the normal build; the Mainframe uses neither. With a 3 × 64 image of roughly 0.5 MB with word tokens **[computed, rough]** a smaller model partition would keep the second app slot; the measured table is kept because it fits every size of the ladder.

### 13. Device acceptance

- Same-session comparison with and without image, show with music and texts triggered by the automatic run, method of `docs/slm-prototype.md` Decision 9.
- Limits: 0 audio drops, 0 resets, 0 refused ESP-NOW packets; largest audio gap, largest IMU gap and beat lateness at most 10 ms above the baseline (the tolerant limit of the prototype; user decision, 2026-11-18: it also decides this change).
- Also recorded: time from event to emoji, to first word and to complete text; share of fallback texts.

### 14. Tokens and pretraining: measured comparison (user decisions, 2026-11-18)

The character model at 3 × 64 wrote mostly broken German, for example "ICH BIN KEINE DEN RUNTER AUS." and invented words such as "FINSCHASSEL". The training loss kept falling while the validation loss bottomed at 1.26 per token near 1,400 steps and then rose. The user chose to measure the next size and pretraining on general German, then added word and subword tokens as options.

Method: all runs on the host, float models, seed 1, the corpus split of Decision 10. Measure: nats per validation text (emoji, text and end summed), on the 278 of 360 validation texts whose words all occur in the training lines, so that word tokens are compared fairly. Lower is better. Word and subword runs keep their best validation point; character runs are taken at their last step, so their best point is about 1–1.5 nats lower (estimated from the per-token validation curve). Each variant was run once; differences of about one nat are within what a second seed may change.

| Variant (3 × 64 unless stated) | nats per validation text | Notes |
|---|---|---|
| Characters (2,000 steps) | 33.7, about 32.3 at the best point | invented words |
| Characters, 4 × 96 (1,200 steps) | 32.1 | fewer invented words |
| Characters, pretrained (6,000 steps on the books, then 1,500 on the corpus) | 32.0 | fewer invented words, loops such as "DASS DU GEHST WEH" |
| Subword (BPE on the corpus), 250 / 500 / 1,000 merges | 33.8 / 33.3 / 31.8 | invented words in 13 % / 7 % / 6 % of generated words |
| Subword, pretrained, 500 / 1,000 merges | 30.9 / 30.6 | invented words in 2 % / 4 % |
| Words | 27.7 | no invented words by construction |
| Words, 4 × 96 | 27.2 | |
| Words, pretrained on sentences with at most one word outside the corpus | 28.3–30.7 | 4,590 sentences, 27,160 words |

Invented words are generated words found neither in the corpus nor in the pretraining text, counted over a greedy and a sampled text for each of the 30 rating cases **[measured, host, 2026-11-18]**. Texts: `models/text-model/` and the experiment scripts of that session; the subword and word experiments were not committed.

- Criteria agreed with the user before the subword result: subword tokens without pretraining should come close to the 27.7 of word tokens; pretraining has to bring the error clearly below 27.7; invented words are counted. Neither held, so the user chose word tokens (Decision 6).
- Pretraining text: four public-domain books from Project Gutenberg (Fontane, Effi Briest; Kafka, Die Verwandlung and Der Prozess; Thomas Mann, Buddenbrooks; 2.65 million characters in the text alphabet), downloaded with the user's yes from the official mirror `mirror.csclub.uwaterloo.ca` by `apps/mainframe/tools/text_model_pretrain_data.py` into the ignored `models/text-model/pretrain/` and checked by SHA-256. Only the script and the source list are versioned (user decision). The editions use modern spelling. The user would accept old spelling before 1901 and finds a stray "TH" fitting; 19th-century prose suits the creature's gothic tone.
- Pretraining is kept as a lever, not as a step of the pipeline (user decision): `text_model.py train --pretrain` and `--init`. With word tokens only sentences made of corpus words can be used, which is too little text. A later try could widen the word list beyond the corpus or add gothic 19th-century prose.

## Calibration values

| Value | Initial | Source |
|---|---|---|
| Wait for the emoji | 2 s | user decision |
| Mood levels | 0–33 / 34–66 / 67–100 | user decision |
| Hard length limit | emoji plus 47 text characters | computed; user decision for word tokens |
| Word list | every word of the corpus (2,048) | user decision |
| Start size | 3 layers × 64 | user decision |
| Growth threshold | fewer than 20 of 30 usable | user decision |
| Temperature, top-k | 0.8, 8 | user decision in the rating of 2026-11-18 ("mittel" out of 0.6 / 4, 0.8 / 8, 1.0 / 16) |
| Validation split | 10 % per event, seed 52 | Claude's choice, left to it by the user |
| Display time, cooldown, scroll times | as today (`message-panel` Current values) | unchanged |

## Risks / Trade-offs

- [Quality of a tiny model] 3 × 64 may write broken German or ignore the mood; the character model did (Decision 14). → Word tokens; growth rule; the ladder ends at 6 × 128 (about 3 s per text at 98 ms per step **[computed]**, so the emoji would still arrive well within 2 s).
- [Coverage of 243 mood combinations] Rare combinations may be missing from the corpus. → The corpus is written per value, not per combination; the user's rating cases include rare combinations.
- [Load on core 1 during writing] The prototype's +10 points came from continuous generation; short bursts are not measured, and writing happens while the panel is visible, the tightest state. → Device acceptance per Decision 13; the 40 ms eye reserve from `ideas.md` stays available.
- [Closed word list] The model can only use corpus words, and grammar between words still has to be learned from 3,632 lines. → New words go through the corpus; pretraining (Decision 14) stays available as a lever.
- [Quantisation] 4-bit weights can damage a small model more than a large one. → The form check and the rating run on the quantised image through the host kernel.
- [Layout change for all texts] Every fixed text looks different after this change. → Shipped and looked at on the device first, without a model (tasks group 2).
- [Training reproducibility] Training on the Mac may not be bit-exact between runs. → Images are identified by checksum; the corpus and settings are versioned.

## Migration Plan

- Group 2 ships the layout change in the normal build without any model.
- Until the move (Decision 12) the model exists only in `mainframe-slm` and `mainframe-slm-capture`; flashing `mainframe` restores `default_8MB.csv`.
- After the move, flashing `mainframe` writes the model partition table; the model image is written once with its own flash command (documented in `README.md`). Without an image the device runs with the fixed lists.

## Open Questions

None. Corpus size and file layout are agreed with the user in task 5.1; temperature and top-k are chosen in task 6.2.
