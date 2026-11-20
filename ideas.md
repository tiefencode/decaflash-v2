# Ideas

A collection point for things that are not decided yet. Nothing in this file counts as a rule or as an assignment.

## Rule candidates

Possible working rules for `CLAUDE.md`. A candidate is not a rule. The procedure is described by the skill `rule-candidates`.

- Move training data and models under `models/` (ignored) into the main checkout before removing a worktree; `git worktree remove` deletes ignored files without asking. (Nov 20)
- Before a function is switched off on request, say what the user will see on the device afterwards (example: gate off meant almost no verses any more). (Nov 20)


## Noted for later

Plans that are settled but wait for something else.

### Eye base cycle without text, or a low-level step

- **Why:** Decision of the user of 2026-11-17: check it as its own change after `reduce-panel-frame-rate`. Measured without text: 40 ms instead of 25 ms lowers the load of core 1 from 58.8 % to 47.1 % (`docs/project-context.md`, section E).
- **What:** either lower the base cycle or a fixed low-level step (e.g. 40 ms without text, 67 ms with text) that is switched on by known states, not by measured load. Clarify values and triggers in an interview.
- **Reserve:** Statement of the user of 2026-11-17: 40 ms as the general cycle of the eye, also without text, is an option we can use when the CPU is a bottleneck (for example for the language model). Measured in the sweep: audio gap without text 51 instead of 55 ms, with text at 40 ms 49 instead of 92 ms.
- **When:** after the merge of `reduce-panel-frame-rate`; fits the language model prototype.
- **Check:** look on the device, measurement with `mainframe-panel-rate-sweep`.

### Depression poems from the own language model

- **Why:** Decision of the user of 2026-11-17 in the interview on `add-text-model`: the model writes only the reaction texts first; the poems stay the fixed list for now but should later come from the model as well.
- **What:** poems as their own event kind in the same model; emoji first and word-by-word display as with the reaction texts. Clarify length, tone and whether the mood is included in an interview.
- **When:** after `add-text-model`, when model size and quality of the reaction texts are settled.
- **Check:** form check and rating as in `add-text-model` (design.md, Decision 9), device test with `mainframe-slm-capture`.

### Second language pass through the corpus

- **Why:** Statement of the user of 2026-11-18 after the revision of the corpus: some sentences still sound odd, and the model learns such sentences too. The corpus goes into training as it is for now; smoothing comes afterwards.
- **What:** read all files in `apps/mainframe/text_model/corpus/` for grammar and natural German. The user first collects examples that strike them as odd; derive patterns from them and correct them in all files, not only single lines. These stay: levels as planned, the context rule (the event stays recognisable), the "Zu viel klopfen" ("too much knocking") rule in `multi_tap`, the originals the user wants to keep, and unambiguous texts.
- **Addendum (2026-11-18, after the text bank and the text model were built in):** The user noticed on the device that some lines of the text bank are bad as well; they call this one of the next projects. The rater measurement of the same day (two Claude raters consider almost all text bank lines usable but agree with the user's judgement only 60–68 % of the time) says little about the user's standard; numbers in `docs/text-model-measurements.md`, section "Text bank line ratings". Bad corpus lines count twice: they appear as a text bank line and are training material of the text model.
- **Related:** a smaller word list from the frequent corpus words (the reference model Barista gets by with a few hundred hand-picked words; in ours almost half of the corpus words occur only once; numbers in `docs/text-model-measurements.md`, section "Event word repetitions and corpus word counts"). Rewriting lines with rare words anyway and doing the language pass in one go saves a second round.
- **Example "Drehteller" (2026-11-19):** A word that occurs exactly once in the corpus shows up dozens of times in the model texts, because the model masters the pattern "ICH BIN KEIN(E) …" ("I am no …") and fills the gap with a default word (interpretation, not measured). Search for: sentence patterns whose gap is filled by only unique words. Remedy: use such words several times and fitting the event, or make the pattern rarer.
- **Repetitions (measured on the Mac, 2026-11-19):** The model repeats itself a lot: a few closing sentences and many literal corpus lines, and few distinct texts for the same event and mood. Causes by interpretation: the mood hardly changes on the device, the mild selection takes the most probable word almost always after the first word, and the corpus offers few continuations for frequent sentence openings. Numbers on "Drehteller" and the repetitions in `docs/text-model-measurements.md`, section "Event word repetitions and corpus word counts".
- **When:** after the next training, when the rating cases (`apps/mainframe/text_model/rating_cases.tsv`) show which sentence patterns the model adopts.
- **Check:** regenerate with `python3 -I apps/mainframe/tools/text_model_vocab.py` and `python3 -I apps/mainframe/tools/text_bank.py`, `python3 -I apps/mainframe/tools/text_model_corpus.py check`, `sh tests/run_all.sh`; commit both headers with the corpus; then retrain.

### Split the Node `main.cpp`

- **Why:** `apps/node/src/main.cpp` has about 1,660 lines and is too big. For comparison: `apps/mainframe/src/main.cpp` has about 500.
- **What:** move parts into their own files under `apps/node/src`. Candidates by today's structure: button gestures, Morse/text overlay, demo cycle and beat rendering, configuration in NVS (node kind, role), serial commands (help, status, programs). Pure restructuring, no new behaviour.
- **When:** after the end-to-end test with a flashed V2 node (rule in `CLAUDE.md`), so that there is a verified state to compare against.
- **Check:** `sh tests/run_all.sh`, `pio run -e node`, then compare against the old state on the device.

### Remove mute on the Flashlight node

- **Why:** Decision of the user of 2026-11-16: muting by long press has no recognisable use and is probably a leftover from V1. A second long press does not lift it; under Mainframe control it ends only with a scene change, a Hello or a serial `mode`/`role` command.
- **Today:** described in `openspec/specs/node-device/spec.md`, requirements "Long press per Node kind" and "Output mute".
- **What:** remove `outputMuted` and the mute branch in `serviceButton()`; the long press on the Flashlight then has no function. Adjust the help text (`long press -> turn output off`) and `status` (`muted=`). This also removes BUG-021 ("Node text overlay lights a muted flashlight") in `bugs.md`.
- **When:** after the end-to-end test with a flashed V2 node (rule in `CLAUDE.md`), as its own OpenSpec change; fits splitting the Node `main.cpp`.
- **Check:** `sh tests/run_all.sh`, `pio run -e node`, on the device a long press on the Flashlight in demo and under Mainframe control.

### Host tests for scenes and node output

- **Why:** Neither `shared/include/scene_programs.h` nor the node output in `apps/node/src` has a host test. The node-output spec (`openspec/specs/node-output/spec.md`) describes the behaviour but does not check it.
- **What:** host tests for the pure logic: motif choice and parameters of the flash variation profile (deterministic per cycle), trigger cycles and beats, Morse encoding of the text overlay with gaps of 1/2/4 units. Renderers with FastLED or GPIO stay out as long as they do not run without hardware.
- **When:** open; useful before or together with "Split the Node `main.cpp`", because the Morse logic has to be extracted from `main.cpp` for it.
- **Check:** `sh tests/run_all.sh`.

### Unify RGB patterns

- **Why:** The RGB pattern PulseRow (a row of short pulses, triggered on a beat) uses no scene, nor do its templates `heartbeat()` and `triplePulse()` in `shared/include/scene_programs.h`. The patterns Heartbeat and RiserPulse are at heart pulse rows with a hard-coded shape. Described in `openspec/specs/node-output/spec.md`.
- **What:** extend PulseRow so that it can represent Heartbeat and RiserPulse: a decay after the last pulse, times in beat fractions instead of milliseconds, computation from the cycle position instead of as an event (so that it survives a missed beat or an override), strength per pulse. Then make Heartbeat and RiserPulse PulseRow templates and remove the special renderers, or delete PulseRow with its templates if it is not needed.
- **When:** open; together with the final light design of the scenes and only after the end-to-end test with a flashed V2 node (rule in `CLAUDE.md`).
- **Check:** `sh tests/run_all.sh`, `pio run -e node`, on the device compare old and new Heartbeat or Riser; the node-output spec changes with it.

### Blinking at rest

- **Why:** According to the requirement in `docs/project-context.md` B.2, blinking belongs to the resting behaviour of the eye. It is not implemented; the lids close today only when sleeping (`drawSleepLids()` in `apps/mainframe/src/eye_renderer.cpp`).
- **What:** occasional short blinking while the eye is awake. The obvious way is to close and reopen the existing sleep lids briefly. Whether mood (for example `attention`) influences the frequency is open.
- **When:** open; does not depend on the end-to-end test, because it concerns neither radio nor node output.
- **Afterwards:** add a requirement to the eye-iris spec (`openspec/specs/eye-iris/spec.md`).

### Message panel: consume nothing unseen

- **When:** after the inventory of the specs, as its own OpenSpec change.
- **Today:** While sleeping and while the debug view is shown, the renderer does not draw the panel. Texts and poems are triggered nevertheless and count as shown: the cooldown of the motion kind starts, the poem is taken from the list and the two-minute interval restarts. During the boot animation the renderer, on the other hand, does draw the panel but should not (BUG-010). Described in `openspec/specs/message-panel/spec.md`, requirement "Panel hidden while sleeping or in debug view".
- **What:** texts on motions are discarded in these states and during the boot animation without starting the cooldown; if the motion wakes the being, the text comes as before. A due poem waits until the panel is visible again, as when the panel is busy.
- **Where:** `apps/mainframe/src/main.cpp` (call of `showMotion` and `serviceDepressionPoetry`) and `apps/mainframe/src/eye_renderer.cpp` (boot check when drawing), not in the panel itself. The spec changes with it.

### Presence bus at 400 kHz

- **State:** Proposal from the measurement of 2026-11-17, not yet decided by the user.
- **Why:** The presence sensor (STHS34PF80) is polled at 100 kHz and takes 4.4–4.5 % of core 1 that way. At 400 kHz it was 1.7–1.9 % in the measurement: a poll without sample 0.31–0.37 ms instead of 0.69–0.75 ms, one with sample 1.04–1.10 ms instead of 3.05–3.12 ms, without read errors.
- **What:** one constant, `kBusHz` in `apps/mainframe/src/presence_sensor.cpp`.
- **Limits:** At 400 kHz only about 65 s were measured on the current Grove cable. The data sheet (DS13916, section 3.1.2) states I2C up to 1 MHz.
- **Check:** `sh tests/run_all.sh`, `pio run -e mainframe`, then watch the read-error counter on the device over a longer run.
- **Source:** measurement environment `mainframe-loop-load-trial` on the branch `claude/blocked-sections-analysis-a46e53` (not merged).
- **Not part of it:** reading the motion sensor through the FIFO of the BMI270. It would bring all 100 samples per second instead of 43–65 to the classifier, but the knock thresholds are tuned to today's data, and detection from FIFO data has not been checked.

### Split the Mainframe main loop into tasks

- **State:** Proposal of 2026-11-17, noted by the user as a plan, scope and split not decided yet.
- **Why:** Today everything of the Mainframe runs one after another in `loop()` on core 1. A long pass (eye, panel) delays fetching the audio blocks and the IMU samples. In the language model measurement of 2026-11-17 (`docs/slm-prototype.md`, results for 6.4) the largest gaps with the model computing became 2 to 11 ms longer (text state: audio 67 instead of 56 ms, IMU 50 instead of 39 ms), although the model computes on core 0. A separate task with higher priority for audio and IMU would decouple these gaps from the eye.
- **What:** audio fetching, tempo and IMU (possibly the show clock) into a task above `loop()`; eye, panel, personality and presence stay in `loop()`. Exchange shared data without blocking.
- **Limits:** Not measured whether it helps: core 1 also becomes slower with the model without more code running there; if that is memory or cache contention, a separate task does not help against it. Large change to the productive audio and tempo path.
- **When:** after the BPM detection is clarified, as its own OpenSpec change with an interview.
- **Check:** before and after on the device with `mainframe-loop-load` or `mainframe-slm-capture` (audio and IMU gaps, beat delay), `sh tests/run_all.sh`.
- **Related:** "Presence bus at 400 kHz" saves about 2.6 percentage points of core 1, "Eye base cycle without text" about 7 to 11 points; both smaller and without restructuring.
- **Not meant:** merely splitting `apps/mainframe/src/main.cpp` into files; that changes nothing about the timing.

### Text model: try word parts once more

- **State:** Wish of the user of 2026-11-18. The text model (`add-text-model`) uses word tokens; word parts (BPE) were measured and put aside.
- **Why:** Word parts can form rare words and German word forms from shared building blocks and make pretraining on foreign text possible. With the corpus of 2026-11-18 they were worse, though.
- **Measured:** on 2026-11-18 on the host and repeated on 2026-11-19 (reported by the user, run in a Codex worktree, not re-checked here): word tokens are better, the Gutenberg pretraining shows no benefit, word parts solve neither language quality nor event fidelity. Numbers in `docs/text-model-measurements.md`, section "Tokenizer comparison"; also described in `openspec/changes/add-text-model/design.md`, Decision 14 (branch `claude/slm-core-design-24642e`).
- **What:** repeat the same measurement with the same three criteria: error per validation text against word tokens, effect of pretraining, share of invented words. Useful mainly with more corpus or a pretraining in the own style. Invented words could be excluded during generation by allowing only parts that lead to a known word (idea, not checked).
- **When:** when the text model with word tokens is finished and accepted on the device. After the repeat of 2026-11-19 only useful if the corpus or the pretraining text changes significantly.
- **Tooling:** The pretraining tool is committed on the branch; according to the measuring session the experiment scripts lie in the notes folder under `text-model-tokens/` and are not in the repo.

### Text model: reject threshold for uncertain texts

- **State:** Measured on 2026-11-18 and planned as OpenSpec change `add-text-model-confidence-gate` (branch `claude/text-model-confidence-gate-650684`); not built yet. Result: no statement early on, only moderate over the whole text; the threshold stays as an emergency brake (abort from token 5, about 20 % aborts). Numbers in the design.md of the change.
- **Noted, not begun:** a mask that allows only verb forms that fit the subject (worth it only if one kind of grammar error predominates; not so far), and a pretraining text of short, simple German sentences in the vocabulary of the model instead of the novels.
- **Why:** On the device the emoji of the text model comes so late that the text bank practically never steps in. In the user's rating the text bank was clearly better than the text model; the user more often sees bad German and nonsensical texts on the device. Numbers in `docs/text-model-measurements.md`, section "Confidence gate".
- **What:** The text model knows its confidence at every token. If it is below a threshold, the panel shows a line from the text bank instead of the model text. This costs no compute time.
- **Decision of the user (2026-11-18):** The variant "finish writing first, then show" is out of the question; the emoji and the word-by-word growth stay. What remains: decide early (from the emoji and the first words) or abort a started text and replace it with the text bank line. This is still open.
- **Measure first:** generate many random cases on the Mac with the confidence value (tool from commit `1a6a4e6`); the user rates a targeted sample of low, medium and high confidence without seeing the value; plus a judgement by Claude, calibrated to the user's judgements, over a few hundred texts. The question is whether the confidence predicts the judgement, how early in the text that is recognisable and at which threshold how many texts go to the text bank.
- **When:** after `add-text-model` is finished, as its own OpenSpec change.

### Text model: little response to the mood levels

- **Why:** Measurement of 2026-11-18 on the Mac: for one counted sentence ending ("ICH BIN NICHT EINSAM", "I am not lonely", at Klopfer) and for the visitor texts of "Kommt davor" the model hardly followed the loneliness level; what it writes was determined mostly by the event.
- **Re-measurement (2026-11-19, Mac):** one mood axis each at low, middle and high. The emoji follows each axis clearly and in the expected direction; the words follow more weakly but in a direction. The finding "little response" thus holds only for the one counted sentence ending; the model reacts to the levels, mostly through the emoji. Numbers for both measurements in `docs/text-model-measurements.md`, section "Per-axis mood response".
- **What:** check why the model uses the mood levels weakly (distribution of the levels in the corpus, weight of the condition tokens, training) and retrain it so that the levels visibly steer the texts. Clarify goal and measure in an interview.
- **When:** after `add-text-model` is finished.
- **Check:** repeat the same measurement and compare with the numbers in `docs/text-model-measurements.md`, section "Per-axis mood response".

### Habituation to permanent extreme values for the text bank

- **Why:** Annoyance, attention and depression rest at 0, loneliness drops to 0 at a party and stays there, energy stays high with loud music. The text bank takes as lead value the value with the largest distance from the middle; so at rest and at the party the same low values always win, and with small values of annoyance and depression only "lonely low" (measurement of 2026-11-18 with the code of `TextBank::leadFor`, numbers in `docs/text-model-measurements.md`, section "Pinned mood and habituation"). Because the text bank today practically steps in only when the text model is too late, the user put the plan on hold on 2026-11-18.
- **What (decisions of the user of 2026-11-18):** habituation plus weighted drawing, only for the reaction texts (eye, sounds and light instruction stay with the real values). All five values, both extremes. A value is habituated when it has been in the same third for 5 minutes; a change of third makes it conspicuous again at once and restarts the time. A habituated value counts as middle: weight 0 for the text bank, level middle for the text model, which gets the same levels. Among the remaining values the lead value is drawn linearly by distance from the middle. If no value has a weight, draw by the raw values. Open: how the debug view and the serial line show a habituated value.
- **When:** when the text bank speaks regularly again, for example after the reject threshold above, or for builds without a model image.
- **Check:** host test of the rule, short device test with the debug view.

### Reaction texts as unrhymed short verses, pretraining on poetry

- **State:** Step 1 (tone probe) done on 2026-11-19, the tone is chosen; step 2 and the second text bank are open.
- **Why:** Size, novel pretraining and token kind did not improve the texts (measurements of 2026-11-18 and 2026-11-19). Verses are short and self-contained like the reaction texts; an unusual word order reads as style in a verse instead of as an error, a repetition as a refrain. The depression poems exist already, the being would get a consistent voice. The corpus has to be revised anyway (entry "Second language pass through the corpus").
- **Clarification by the user (2026-11-19, later that day):** Rhymes are not wanted, but not bad either if they are there. The rhyme question does not have to be decided now. It follows: the texts of the being and the example texts in step 1 stay unrhymed; for the pretraining, rhymed short poems come into question as a second group (for example Busch, Morgenstern, Ringelnatz, Heine), and the source list should show both groups separately. Whether rhymed material helps or harms with its word order is shown by the training trial in step 2 (unrhymed alone against mixed).
- **Decision of the user:** no rhymes. That also fits technically: with word tokens the model does not know the sound of a word and could only reproduce rhyme pairs it has seen.
- **Step 1, without training:** have example texts written for three events in two or three unrhymed forms (free short verse, very terse lines, saying form); the user chooses the tone.
- **Result step 1 (2026-11-19):** 90 example texts for knocking, shaking and someone leaving, ten per form, in the moods annoyed, lonely, tired and cheerful, all under 47 characters, the free short verses in the tone of Rilke and Hölderlin (the user's favourite poets). The user likes the probe "richtig gut" ("really good"). Decisions of the user:
  - The new voice mixes all three forms. Examples: "DU GEHST, / UND DER RAUM / WIRD WIEDER GROSS." ("You leave, and the room becomes big again"; free short verse), "KLOPF? / DU? / ENDLICH." ("Knock? You? Finally."; terse), "WER KLOPFT, GLAUBT AN EINE TÜR." ("Whoever knocks believes in a door"; saying).
  - A verse break ("/" in the examples) becomes a new line on the panel. The panel needs a character for the line break for that; that is a firmware change (text bank, panel layout, test in `tests/message_panel.cpp`).
  - The verses go into a second text bank with its own corpus folder next to the old one; a build-time setting chooses which text bank goes into the firmware, so that the user can return to the old one. It is built in its own session.
  - The example texts with form and event are in the tone probe (Claude Doc "Kurzverse für das Wesen – Probe"); they are a start for the new corpus.
- **Step 2, small training trial:** pretraining on unrhymed, short public-domain poetry instead of novels, once with word tokens and once with word parts (impression of the user of 2026-11-19: with word parts more of the wording of the pretraining text sticks; not measured).
- **Step 2, pretraining run (2026-11-20, host, not on the device):** Two runs with the same size and the same steps, word tokens and own BPE; the user read 27 pairs each blind: word tokens better, but not secured. Remarks of the user: too many Bible verses, texts partly too long. Numbers and counts in `docs/text-model-measurements.md`, section "Verse pretraining". Decisions of the user:
  - Material (released by the user): unrhymed poetry, aphorisms and elevated prose, without novels; Hölderlin's poems 1784–1800 in full, also the rhymed ones. Old spelling is aligned through a short rule list in all sources.
  - Second run without Bible tone (decision of 2026-11-20): without Luther, Zarathustra and Klopstock. Result: leaving material out does not help; if more material, then short forms instead of prose (candidates, unchecked: Whitman in the translation of Johannes Schlaf, Mombert, Novalis' fragments, Goethe's Maximen und Reflexionen, Morgenstern's Stufen).
  - Retraining and samples on events wait for the verse corpus (decision of 2026-11-20). The same holds for the comparison of event fidelity from the main session: without pretraining, with pretraining, with pretraining marked by word fields and with mixed retraining. Add a run without the 289 verses that carry no event word (user decision of 2026-11-20: rewrite them only if event fidelity is better without them; list criterion in `docs/text-model-measurements.md`, section "Event word fields in the verse corpus").
  - Word fields per event: agreed with the user on 2026-11-20 in `apps/mainframe/text_model/word_fields.tsv`; counts and conclusions in `docs/text-model-measurements.md`, section "Event word fields".
- **Step 3:** rebuild the corpus into the chosen form, together with the small word list.
- **Sources:** checked in public catalogues (2026-11-19, nothing downloaded), split into an unrhymed and a rhymed group; the list with sizes, spelling and licence notes is in `docs/text-model-measurements.md`, section "Verse pretraining". Every download is shown to the user beforehand with list and licence.
- **Decisions of the user on step 2 (2026-11-19):**
  - Material: all unrhymed texts, verse and prose (about 5.4 MB), not only the 1.5 MB of verse. Added on 2026-11-20: elevated, rhythmic prose in the tone of Hyperion (Rilke, Malte and Cornet; Hölderlin, Empedokles; Novalis, Heinrich von Ofterdingen; Goethe, Werther), together about 6 MB. The user particularly likes Hyperion. The first trial runs without novels (decision of 2026-11-20); only if basic grammar is missing or the model reproduces whole lines by heart, a second variant follows: first novels, then the poetry.
- **Enlarge the verse corpus before any fine-tuning (decision of the user of 2026-11-20):** 372 verses are about 31 per event, spread over the mood values; a model would probably copy or miss event and mood with that (not measured). The goal is a few hundred verses per event, each released by the user. Only then: rebuild the pretraining with word tokens (the data from step 2 are deleted), fine-tune, read blind against the verse text bank, onto the device.
  - **Done (2026-11-20):** the verse corpus has 200 verses per event (2,400), about 20 per lead value and the three forms mixed. The user reviewed samples of 24 verses in rounds 1 and 2 (all approved) and then decided that no further samples are needed; Claude checked every later verse itself (length, curated emoji, duplicates, repeated patterns, rare words). Word coverage against the poetry text: `docs/text-model-measurements.md`, section "Verse corpus word coverage". Next step: the vocabulary and pretraining for the verse corpus (today's word list is built from the classic corpus only).
- **Noted as next pretraining trial (decision of the user of 2026-11-20, after the result of step 2):** more material only in short forms, for example Novalis fragments, Goethe's Maximen und Reflexionen, Morgenstern's Stufen, Mombert, Whitman in the translation of Johannes Schlaf; none of it checked for source, licence and size, show list and licence before any download. Dropped is the earlier note with the whole Luther Bible, Klopstock's Messias and Homer in the translation of Voß: the user found too many Bible verses already in the trial and some texts too long, and long narration would reinforce that.
  - Old spelling (Klopstock, Ebner-Eschenbach) is modernised automatically (short rule list: th→t, ey→ei, ß/ss by today's rule), then the most frequent remaining old forms are checked. Reason: with word tokens "THRÄNE" would be a different word than "TRÄNE".
  - Two runs with the same model size and the same steps, once word tokens, once word parts; the user reads the texts of both runs blind.
  - Step 2 runs in parallel to the second text bank, in its own session. Before the download this session shows the user list and licence.
- **Measurement data:** `docs/text-model-measurements.md`, sections "Verse pretraining" and "First word-field counts".
- **Open:** The event must stay recognisable in the text (corpus rule).
