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
- Expected timing at 3 × 64 with character tokens: 6 condition tokens plus about 25–35 output pieces **[estimated]**; time per forward pass about 15–25 ms **[computed, rough: layer weights about a third of 4 × 96, not measured]**, so emoji after about 0.1–0.2 s and the whole text after about 0.5–1 s.

### 4. Emoji first, words after, panel shows it as it grows (user decisions)

- Training texts are stored as `condition tokens, emoji, text, end`. The first output step is masked to the curated emoji, so every model text has exactly one emoji and it is chosen first; it acts as the intent the words follow. The text body is masked to the text alphabet (Decision 6) and contains no emoji, like today's texts.
- The panel shows the emoji as soon as it arrives and then every finished word (word by word, user decision; character by character was rejected because a growing word jumps to the next line at the column limit).
- The old message stays visible until the emoji of the new one arrives (user decision), then it is replaced.

### 5. Layout: emoji in the first row for all messages (user decision)

- Every message whose last character is a curated emoji (all fixed texts and poems today) and every model text shows that emoji large and centred in the first row, with the text lines below. This replaces "Trailing emoji". Alternatives: text above with the emoji pushed down (keeps today's look), or the new layout for model texts only; the user chose one layout for all.
- The emoji counts as one element for display time and scrolling, as before.

### 6. Vocabulary: one token per character or curated emoji (user decision)

- Output alphabet: space, A–Z, Ä, Ö, Ü, 0–9, `. , ! ? - '` and the curated emoji of `panel_glyphs`, plus an end token. Lower case and ß are left out, as in today's texts (ß would be drawn as `B`). The exact punctuation set is taken from today's texts and the corpus.
- Input vocabulary: the output alphabet plus 12 event and 15 mood tokens and a start token; the image format already has separate input and output vocabularies with an output-to-input table (Barista image) **[read]**.
- The tokenizer table is generated from the `panel_glyphs` emoji list, so the model can only emit drawable glyphs; a host test checks this.
- About 110 output and 140 input tokens instead of 854 / 8,057: the embedding tables and the output head shrink accordingly **[computed]**.

### 7. Wait time and fallback (user decisions)

- If the emoji has not arrived 2 s after the panel accepted the event, the panel shows a text from today's fixed list for that event and cancels the request. Once the emoji is visible, the model writes to the end.
- The same fixed list is used, without asking the model, when the runtime is off (no or invalid image, failed allocation) and while the panel is hidden (sleep, debug screen; user decision: no model use while hidden). The variant selection of the fixed list stays as today.
- Length: no row limit (user decision, 2026-11-18: the panel scrolls longer texts, also while they grow, and three of today's fixed texts already need more than four rows). The only limit is the hard limit of output pieces: emoji plus text at most 48 pieces, below the model's position limit and the 192-byte capacity **[computed]**; training texts are filtered to it, and the model task ends a longer text there.

### 8. A new event while a text is being written (user decision)

The new event wins, as the panel does today: the open request is cancelled, the model starts the new one with the current mood. The cancel is checked by the model task between forward passes, so the old request ends within one forward pass.

### 9. Model sizes and growth rule (user decisions)

- Start at 3 layers × dimension 64. Ladder: 3 × 64 → 4 × 96 → 6 × 128; the last is the largest size measured on the device. Only the next step is trained, and only when the current one fails (user question: not every size has to be done).
- Rule: a host check runs first on texts generated by the host kernel from the quantised image (not from the float model): only alphabet glyphs, emoji first and from the curated set, emoji plus text within the hard limit of output pieces, no verbatim copy of a training text. Then the user rates 30 texts for fixed event / mood cases; fewer than 20 usable means one step up. A size that fails the host check also goes up.
- Sampling: the prototype kernel picks greedily; the product samples with temperature and top-k from a per-request seed so the same event and mood give varied texts. Temperature and k are calibration values, chosen during the user's rating; host tests use fixed seeds.

### 10. Training data: written by a Claude Code session, reviewed by the user (user decision)

- A Claude Code session writes the corpus into text files in the repo, each line tagged with event and five mood levels; today's fixed texts are part of it. This runs on the user's Max plan; the Claude API would be billed separately and is not used. The step starts only after the user's explicit yes; the corpus size is agreed with the user at that step.
- The user reviews samples and removes lines. A validation split is held back for the training loss.
- Result (2026-11-18): layout, size and scheme are user decisions of task 5.1. The corpus is one TSV per event in `apps/mainframe/text_model/corpus/` (five levels, tab, emoji and text). Every planned line has one lead value at low or high, and the other four levels are random (`apps/mainframe/text_model/plan/`). Today's 32 fixed texts are included with a lead that fits their content. After the first review the user set the rule that every text must make its event recognizable, and the mood only colours it; 2,022 lines were rewritten for it, together with the user's own edits. The second review round needed no change. Final size: 3,632 lines (300 per event plus the fixed texts); `text_model_corpus.py check` reports no problem.
- Validation split: 10 % of every event file (360 lines), drawn with the fixed seed 52 in `text_model_corpus.split`. The user left this value to Claude.

### 11. Training on the user's Mac (user decision)

- Python training script (PyTorch) and an exporter that writes the kernel's image format (header version 1, 4-bit groups, separate output head). Installing the Python packages is a download and starts only after the user's yes.
- The exporter is verified by a host test (`tests/run_text_model_export.sh`, tool `apps/mainframe/tools/text_model_check.py`): for every event with five mood combinations, the host kernel on the exported image is compared with a Python twin of the kernel's arithmetic (the exported 4-bit weights and int8 activations, as `slm_kernel.h` sets `LLM_INT8_ACT`). Threshold: the kernel equals the twin in greedy output pieces in every case. The comparison with the float model is reported, not required: on the smoke model (3 × 64, seed 1, 600 steps on today's fixed texts) the kernel equalled the twin in 60 of 60 cases and the float model in 54 of 60, the first piece included, so quantisation changes close choices **[measured, host, 2026-11-18]**. Text quality is judged on the image itself by the form check and the user's rating (Decision 9).
- Images are not committed; corpus, scripts, settings and seed are, and the SHA-256 of each image used on the device is recorded in `docs/project-context.md`.

### 12. Builds: own env first, then the normal build (user decision)

- New product env `mainframe-slm` (extends `mainframe`, `-DDECAFLASH_SLM=1`, `partitions_slm.csv`, no measurement flags). `mainframe-slm-capture` gets the request mode with the existing automatic text triggers for load measurements.
- After the device acceptance (Decision 13) and the user's yes, the normal `mainframe` env takes the partition table and the runtime, and `mainframe-slm` is removed. Consequence the user accepted: no second app slot for OTA and no SPIFFS in the normal build; the Mainframe uses neither. With a 3 × 64 image of roughly 0.1–0.2 MB **[computed, rough]** a smaller model partition would keep the second app slot; the measured table is kept because it fits every size of the ladder.

### 13. Device acceptance

- Same-session comparison with and without image, show with music and texts triggered by the automatic run, method of `docs/slm-prototype.md` Decision 9.
- Limits: 0 audio drops, 0 resets, 0 refused ESP-NOW packets; largest audio gap, largest IMU gap and beat lateness at most 10 ms above the baseline (the tolerant limit of the prototype; user decision, 2026-11-18: it also decides this change).
- Also recorded: time from event to emoji, to first word and to complete text; share of fallback texts.

## Calibration values

| Value | Initial | Source |
|---|---|---|
| Wait for the emoji | 2 s | user decision |
| Mood levels | 0–33 / 34–66 / 67–100 | user decision |
| Hard limit of output pieces | 48 | computed |
| Start size | 3 layers × 64 | user decision |
| Growth threshold | fewer than 20 of 30 usable | user decision |
| Temperature, top-k | set during the first rating | to be chosen |
| Validation split | 10 % per event, seed 52 | Claude's choice, left to it by the user |
| Display time, cooldown, scroll times | as today (`message-panel` Current values) | unchanged |

## Risks / Trade-offs

- [Quality of a tiny model] 3 × 64 may write broken German or ignore the mood. → Growth rule; the ladder ends at 6 × 128 (about 3 s per text at 98 ms per step **[computed]**, so the emoji would still arrive well within 2 s).
- [Coverage of 243 mood combinations] Rare combinations may be missing from the corpus. → The corpus is written per value, not per combination; the user's rating cases include rare combinations.
- [Load on core 1 during writing] The prototype's +10 points came from continuous generation; short bursts are not measured, and writing happens while the panel is visible, the tightest state. → Device acceptance per Decision 13; the 40 ms eye reserve from `ideas.md` stays available.
- [Quantisation] 4-bit weights can damage a small model more than a large one. → The form check and the rating run on the quantised image through the host kernel.
- [Layout change for all texts] Every fixed text looks different after this change. → Shipped and looked at on the device first, without a model (tasks group 2).
- [Training reproducibility] Training on the Mac may not be bit-exact between runs. → Images are identified by checksum; the corpus and settings are versioned.

## Migration Plan

- Group 2 ships the layout change in the normal build without any model.
- Until the move (Decision 12) the model exists only in `mainframe-slm` and `mainframe-slm-capture`; flashing `mainframe` restores `default_8MB.csv`.
- After the move, flashing `mainframe` writes the model partition table; the model image is written once with its own flash command (documented in `README.md`). Without an image the device runs with the fixed lists.

## Open Questions

None. Corpus size and file layout are agreed with the user in task 5.1; temperature and top-k are chosen in task 6.2.
