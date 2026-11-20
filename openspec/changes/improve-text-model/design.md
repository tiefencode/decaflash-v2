# Design

## Context

See proposal.md, Why. The model on the device today is `w6x128-pre-ft-400-3e-3.bin`: word tokens, 6 layers × 128, pretrained on novels and fine-tuned on the classic corpus. Its baseline on the harness is in `docs/text-model-measurements.md`, section "Harness baseline" (16.4 % verbatim training lines, about 36 openings per 100 seeds, event classifier 57.8 % against the held-out ceiling of 65.3 %, lead classifier 30.0 % against 47.5 %).

Device state: `mainframe` builds the classic text bank and shows no model texts (`DECAFLASH_TEXT_MODEL_TEXTS=0`, commit dedc994, flashed). The model still runs only in the measurement build `mainframe-slm-capture`. This stays so until an improved model is accepted.

## Goals / Non-Goals

**Goals:**
- Better texts from the same model kind (word tokens, 6 × 128, classic corpus): fewer copies, more varied openings, event fidelity not worse, and texts the user prefers in blind pairs.
- One fixed measurement for every step, so that steps are compared on the same cases and seeds.

**Non-Goals:**
- Pretraining on foreign text (novels or poetry) and word parts (BPE): not pursued (Decision 6).
- The verse voice: parked (section "Parked: the verse work").
- Model size or architecture changes.

## Decisions

### Decision 1: Six steps in a fixed order (user, 2026-11-20)

1. Measurement harness with a gold file of the user's ratings and a new rating set.
2. Writer: repetition block, word mask per event, fewer once-only words, slightly wider sampling.
3. Conditioning on event and lead value only.
4. Regularisation: dropout, early stopping, several seeds.
5. Closed word list of corpus words.
6. Only then more corpus in the classic style.

Each step is measured on the harness before the next starts. The order puts the cheap, model-independent steps (measurement, writer) first and the corpus last, because more corpus only helps once conditioning and training stop wasting it (second opinion).

### Decision 2: How a step is accepted (user, 2026-11-20)

- The harness has to improve first: fewer copies, more openings, event fidelity not worse.
- Then the user reads 60 blind pairs old against new (same event and mood). About 49 decided pairs find a 70/30 preference (two-sided sign test, alpha 0.05, power 0.8; `docs/text-model-measurements.md`, section "Harness baseline").
- No automatic measure stands in for the user's reading. None of the harness proxies beat the base rate on the old ratings either (a weak hint only, same section).

### Decision 3: Step 1, harness, gold file and new rating set

- **Harness** (built): `apps/mainframe/tools/text_model_harness.py`, method and baseline in `docs/text-model-measurements.md`, section "Harness baseline". It runs the firmware's writer on the 4-bit image over fixed cases (30 rating cases, the pinned device moods rest and party, a lead grid of 12 events × 10 lead values) in three seed blocks and reports copies, variety, event and lead fidelity, once-only words, end reason, form check and the confidence gate, with the text bank and held-out corpus lines as reference rows.
- **Gold file** (built): the user's 179 old ratings are collected in `apps/mainframe/text_model/ratings_superseded_2026-11-18.tsv`. The user marked them superseded on 2026-11-20 (no longer representative); they are not used for calibration.
- **New rating set** (design decided by the user, 2026-11-20): 144 blind pairs, text model against the text bank line for the same case: each of the 12 events with each of the 10 lead values (that value low or high, the others middle) plus rest and party per event. The user marks each text usable or not and chooses A, B or equally good. That gives 288 usable marks, 144 per source (about enough to tell a proxy with AUC 0.70 from chance), and a pair comparison. Presented on an Artifact page that stores the answers.
- **State:** the set is prepared with `apps/mainframe/tools/text_model_rating_set.py` for today's device image (ignored `models/text-model/rating-set/w6x128-pre-ft-400-3e-3/`, `pairs.json` shown, `key.json` hidden). The page is not built; the user has not rated any pair yet.

### Decision 4: Steps 2–6, content (user, 2026-11-20, from the agreed plan)

- **Step 2, writer.** A repetition block (a prototype is on branch `claude/text-model-repetition-a5d96d`, commit f927b96, not merged: banned and penalised rows before the top-k in `apps/mainframe/src/slm/text_writer.h`, measurement only), a word mask per event, demoting once-only words, slightly wider sampling than today's temperature 0.5 / top-k 3 after the first token. Values are calibration and are recorded here when measured.
- **Step 3, conditioning.** Event and lead value only; the non-lead levels are middle in training. The device computes the lead value like the text bank (`TextBank::leadFor`). This gives 120 conditions with about 27 lines each instead of about 1,990 conditions with about 1.8 lines each.
- **Step 4, regularisation.** Dropout 0.1–0.2, early stopping on validation loss, three seeds with the noise between them reported.
- **Step 5, word list.** A closed word list of corpus words only (about 1,000–1,500 words); words that occur fewer than three times are replaced in the corpus.
- **Step 6, corpus.** More corpus in the classic style (synthetic, filtered); the user reads samples.

### Decision 5: Carried over from the verse work

Lead-only corpus format, balanced lines per event and lead value, closed word list, automatic round checks, word fields as a measure of event fidelity (`apps/mainframe/text_model/word_fields.tsv`), blind pairs, line breaks on the panel.

### Decision 6: Not pursued: pretraining on foreign text and word parts (user, 2026-11-20)

Measured on 2026-11-18 on the host and repeated on 2026-11-19 (reported by the user, run in a Codex worktree, not re-checked): word tokens are better, the Gutenberg pretraining shows no benefit, word parts solve neither language quality nor event fidelity (`docs/text-model-measurements.md`, section "Tokenizer comparison"; archived change `add-text-model`, design.md, Decision 14). The earlier wish of the user (2026-11-18) to try word parts once more is closed by this decision. What it would have needed, kept for the record: the same three criteria (error per validation text against word tokens, effect of pretraining, share of invented words), useful mainly with more corpus or a pretraining in the own style; invented words could be excluded during generation by allowing only parts that lead to a known word (idea, not checked). The pretraining tool is committed; the experiment scripts lay in the notes folder under `text-model-tokens/` and are not in the repo.

### Decision 7: Spec deltas

- **Step 3** changes the requirement "Request input" of the `text-model` spec: the request carries event and lead value (delta in `specs/text-model/spec.md`).
- **Step 2** is not yet decidable as spec text: whether the repetition block and the word mask per event become requirements (for example "a text repeats no word except …", "a text uses only words allowed for its event") depends on their measured form. If the sampling schedule keeps "wider opening, narrower rest", the requirement "Varied texts" stays; only its calibration values in design.md change. The delta is written when step 2 is accepted.
- **Steps 4–6** change training and data, not device behaviour; no spec change expected. Step 5 may shrink the vocabulary but keeps the requirement "Output form".
- **Device:** when an accepted model goes onto the device and `mainframe` shows model texts again, that is a build setting, already covered by the message-panel and text-bank specs.

## Earlier text model notes (record only, not part of the plan)

Moved from `ideas.md` on 2026-11-20 to keep their measurements and earlier decisions. They are not tasks of this change; the ideas themselves stay in `ideas.md`, and whether one is built is the user's call.

### Little response to the mood levels

- **Measurement (2026-11-18, Mac):** for one counted sentence ending ("ICH BIN NICHT EINSAM" at Klopfer) and for the visitor texts of "Kommt davor" the model hardly followed the loneliness level; the event decided most of what it wrote.
- **Re-measurement (2026-11-19, Mac):** one mood axis each at low, middle and high. The emoji follows each axis clearly and in the expected direction; the words follow more weakly but in a direction. So "little response" holds only for that one sentence ending; the model reacts to the levels mostly through the emoji. Numbers in `docs/text-model-measurements.md`, section "Per-axis mood response".
- **Plan:** check why the levels are used weakly (distribution in the corpus, weight of the condition tokens, training) and retrain so that they visibly steer the texts; clarify goal and measure in an interview. Step 3 answers this in part: with lead-only conditioning there are about 27 lines per condition. Check by repeating the per-axis measurement (the harness lead grid covers the lead classifier and the emoji match).

### Reject threshold for uncertain texts

- **State:** measured on 2026-11-18, change `add-text-model-confidence-gate` archived on 2026-11-20; the panel action of the gate is switched off (b4329c4). Result: no statement early on, only moderate over the whole text; the threshold stays as an emergency brake (abort from token 5, about 20 % aborts; harness baseline 19.6 %). Numbers in the archived design.md and in `docs/text-model-measurements.md`, section "Confidence gate".
- **Why it was planned:** on the device the emoji of the model came so late that the text bank practically never stepped in; in the user's rating the text bank was clearly better than the model.
- **Decision of the user (2026-11-18):** "finish writing first, then show" is out of the question; the emoji and the word-by-word growth stay. Open then: decide early or abort a started text and replace it with the text bank line.
- **Noted, not begun:** a mask that allows only verb forms that fit the subject (worth it only if one kind of grammar error predominates; not so far), and a pretraining text of short, simple German sentences in the model's vocabulary instead of the novels (the latter is closed by Decision 6).

### Habituation to permanent extreme values

- **Why:** annoyance, attention and depression rest at 0, loneliness drops to 0 at a party and stays there, energy stays high with loud music. The lead value is the value farthest from the middle, so at rest and at the party the same low values always win, and with small annoyance and depression only "lonely low" (measurement 2026-11-18 with `TextBank::leadFor`, `docs/text-model-measurements.md`, section "Pinned mood and habituation"). With lead-only conditioning (step 3) this applies to the text model too.
- **Decisions of the user (2026-11-18):** habituation plus weighted drawing, only for the reaction texts (eye, sounds and light instruction keep the real values). All five values, both extremes. A value is habituated when it has been in the same third for 5 minutes; a change of third makes it conspicuous again at once and restarts the time. A habituated value counts as middle: weight 0 for the text bank, level middle for the text model, which gets the same levels. Among the remaining values the lead value is drawn linearly by distance from the middle; if no value has a weight, draw by the raw values. Open: how the debug view and the serial line show a habituated value.
- **On hold (user, 2026-11-18):** until the text bank speaks regularly again (since dedc994 `mainframe` shows only text bank lines; the user has not decided to take the plan up again). Check: host test of the rule, short device test with the debug view.

### Second language pass through the corpus

- **Why (user, 2026-11-18):** some corpus sentences still sound odd, and the model learns them too. On the device some text bank lines are bad as well; the user calls this one of the next projects. Bad corpus lines count twice: as a text bank line and as training material. The rater measurement of 2026-11-18 (two Claude raters consider almost all text bank lines usable but agree with the user only 60–68 % of the time) says little about the user's standard (`docs/text-model-measurements.md`, section "Text bank line ratings").
- **What:** read all files in `apps/mainframe/text_model/corpus/` for grammar and natural German. The user first collects examples that strike them as odd; derive patterns and correct them in all files, not only single lines. These stay: the levels as planned, the context rule (the event stays recognisable), the "Zu viel klopfen" rule in `multi_tap`, the originals the user wants to keep, and unambiguous texts.
- **Related to step 5:** in the corpus almost half of the words occur only once (`docs/text-model-measurements.md`, section "Event word repetitions and corpus word counts"); the reference model Barista gets by with a few hundred hand-picked words. Rewriting lines with rare words and doing the language pass in one go saves a second round.
- **Example "Drehteller" (2026-11-19):** a word that occurs once in the corpus shows up dozens of times in the model texts, because the model masters "ICH BIN KEIN(E) …" and fills the gap with a default word (interpretation). Search for sentence patterns whose gap is filled only by unique words; use such words several times and fitting the event, or make the pattern rarer. Harness baseline: 859 of 25,200 texts (3.4 %).
- **Repetitions (2026-11-19, Mac):** few closing sentences, many literal corpus lines, few distinct texts per event and mood. Causes by interpretation: the mood hardly changes on the device, the mild selection takes the most probable word almost always after the first word, and the corpus offers few continuations for frequent openings (same section).
- **Check after any corpus edit:** `python3 -I apps/mainframe/tools/text_model_vocab.py`, `python3 -I apps/mainframe/tools/text_bank.py`, `python3 -I apps/mainframe/tools/text_model_corpus.py check`, `sh tests/run_all.sh`; commit both headers with the corpus; then retrain.

### Depression poems from the own model (later)

- **Decision of the user (2026-11-17, interview on `add-text-model`):** the model writes only the reaction texts first; the poems stay the fixed list for now but should later come from the model as well.
- **What:** poems as their own event kind in the same model; emoji first and word by word as with the reaction texts. Length, tone and whether the mood is included are clarified in an interview.
- **When:** after this change, when model size and quality of the reaction texts are settled. Check: form check and rating as in `add-text-model` (archived design.md, Decision 9), device test with `mainframe-slm-capture`.

## Parked: the verse work

Parked by the user on 2026-11-20 in favour of this change. A verse model may be tested later. Measurements in `docs/text-model-measurements.md`, sections "Verse pretraining", "First word-field counts", "Event word fields", "Verse corpus word coverage" and "Verse word list and pretraining".

**What exists (committed up to 613bc55):**
- Tone probe (2026-11-19): 90 example texts for knocking, shaking and someone leaving, ten per form, moods annoyed, lonely, tired and cheerful, all under 47 characters; free short verses in the tone of Rilke and Hölderlin (the user's favourite poets). Claude Doc "Kurzverse für das Wesen – Probe". The user liked it "richtig gut".
- Verse corpus: 200 verses per event (2,400), about 20 per lead value, three forms mixed (`apps/mainframe/text_model/verse_corpus/`). The user approved samples of 24 verses in rounds 1 and 2 and then decided that no further samples were needed; Claude checked every later verse (length, curated emoji, duplicates, repeated patterns, rare words).
- Verse text bank: build `mainframe-verse` (archived change `add-verse-text-bank`); `mainframe` builds the classic bank.
- Word fields per event, agreed with the user on 2026-11-20 (`apps/mainframe/text_model/word_fields.tsv`).
- Closed verse word list of 2,500 words (`apps/mainframe/text_model/verse_words.txt`) and one 6 × 128 pretraining run on poetry with checkpoints every 1,000 steps (commits a9da84b, 613bc55; checkpoints in the ignored `models/text-model/`).

**Not done:** fine-tuning on the verse corpus, blind reading against the verse bank, device test.

**Decisions of the user on the verse voice:**
- Why verses (2026-11-19): size, novel pretraining and token kind did not improve the texts; verses are short and self-contained like the reaction texts; an unusual word order reads as style, a repetition as a refrain; the being would get a consistent voice with the depression poems.
- No rhymes (the texts of the being stay unrhymed). That fits technically: with word tokens the model does not know the sound of a word. An earlier note allowed rhymed short poems (Busch, Morgenstern, Ringelnatz, Heine) as a second pretraining group; the source list shows both groups separately.
- The voice mixes three forms: free short verse ("DU GEHST, / UND DER RAUM / WIRD WIEDER GROSS."), terse lines ("KLOPF? / DU? / ENDLICH."), saying ("WER KLOPFT, GLAUBT AN EINE TÜR.").
- A verse break becomes a new line on the panel (built: `\n` in bank lines, `tests/message_panel.cpp`).
- Verses in a second text bank with its own corpus folder, chosen by a build setting so that the user can return to the classic bank (built).
- Pretraining material (released by the user): unrhymed poetry, aphorisms and elevated prose without novels, about 6 MB, including rhythmic prose in the tone of Hyperion (Rilke, Malte and Cornet; Hölderlin, Empedokles; Novalis, Heinrich von Ofterdingen; Goethe, Werther); Hölderlin's poems 1784–1800 in full, also the rhymed ones. The user particularly likes Hyperion. Old spelling is aligned through a short rule list (th→t, ey→ei, ß/ss by today's rule), then the most frequent remaining old forms are checked, because with word tokens "THRÄNE" would be a different word than "TRÄNE". Only if basic grammar is missing or the model reproduces whole lines by heart, a second variant follows: first novels, then the poetry.
- Pretraining runs (2026-11-20, host): word tokens against own BPE with the same size and steps; the user read 27 pairs each blind: word tokens better, not secured. Remarks: too many Bible verses, texts partly too long. A second run without Luther, Zarathustra and Klopstock showed that leaving material out does not help.
- Next pretraining trial, if ever: more material only in short forms (Novalis fragments, Goethe's Maximen und Reflexionen, Morgenstern's Stufen, Mombert, Whitman in the translation of Johannes Schlaf); none checked for source, licence and size; show list and licence before any download. Dropped: the whole Luther Bible, Klopstock's Messias and Homer in the translation of Voß.
- Enlarge the verse corpus before any fine-tuning (done, 2,400 verses). Then: pretraining with word tokens on the verse word list (done), fine-tune, read blind against the verse bank, onto the device (open).
- Retraining and samples on events wait for the verse corpus; the event fidelity comparison (without pretraining, with pretraining, with pretraining marked by word fields, with mixed retraining) plus a run without the 289 verses that carry no event word (rewrite them only if event fidelity is better without them; criterion in `docs/text-model-measurements.md`, section "Event word fields in the verse corpus").
- The event must stay recognisable in the text (corpus rule).
- Caution from the second opinion (not measured on a model): the verse corpus is more templated than the classic one (frequent openings such as "DREH MICH"), so a model trained on it may copy more.

## Risks / Trade-offs

- [The harness measures copies, variety and fidelity, not quality] → the user's blind reading decides each step (Decision 2).
- [Lead-only conditioning drops the four other mood values from the text] → the emoji and the lead value carry the mood; the text bank works the same way and the user prefers it so far.
- [A tighter word list removes words the user likes] → rare words are rewritten in the corpus, not just dropped; the user reads samples.
- [Steps change the same training pipeline; mixing them hides cause and effect] → one step at a time, measured on the harness with three seed blocks (noise below one point for nearly every metric).
