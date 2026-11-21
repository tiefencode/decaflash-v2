# Tasks

Each step ends with its acceptance (design.md, Decision 2): harness first, then 60 blind pairs old against new. The next step starts only after that.

Working order (user, 2026-11-20): 1, 3, 4, 5, 2, 6, 7 (design.md, Decision 1).

## 1. Measurement harness, gold file and new rating set

- [x] 1.1 Collect every user rating into one gold file; verify `apps/mainframe/text_model/ratings_superseded_2026-11-18.tsv` holds the 179 ratings (e81997a)
- [x] 1.2 Build the fixed harness `apps/mainframe/tools/text_model_harness.py`; verify three seed blocks run on the device image (f3a0d75)
- [x] 1.3 Record the baseline of `w6x128-pre-ft-400-3e-3.bin` in `docs/text-model-measurements.md`, section "Harness baseline", and fix the stale README statements (ad01ba0, dd02bed)
- [x] 1.4 Mark the old ratings superseded and score the harness proxies against them; verify the table "Automatic proxies against the old ratings" (200a06e)
- [x] 1.5 Prepare the new rating set of 144 blind pairs with `apps/mainframe/tools/text_model_rating_set.py`; verify `pairs.json` and the hidden `key.json` exist under `models/text-model/rating-set/w6x128-pre-ft-400-3e-3/` (b0101dc)
- [x] 1.6 Build the Artifact page that shows the 144 pairs and stores the answers; verify a test answer is stored and read back (page built with `apps/mainframe/tools/text_model_rating_page.py`, artifact `58c76d8b-dbf5-49bc-b36f-1fa1b75e4826`, collection `answers`; test answer written, read back and deleted)
- [x] 1.7 The user reads the new rating set (usable or not per text, A/B/equal per pair): 55 of 144 pairs read; the user decided on 2026-11-20 that they are enough as the baseline. Result in `docs/text-model-measurements.md`, section "User rating of the baseline"
- [ ] 1.8 Score the harness proxies against the new ratings (55 pairs) and record the result in `docs/text-model-measurements.md`

## 2. Writer

- [x] 2.1 Repetition block: start from the prototype on `claude/text-model-repetition-a5d96d` (f927b96), measure it on the harness and record values in design.md (8 texts, opening penalty 2, word triples)
- [x] 2.2 Word mask per event: measure on the harness (event classifier, field words, copies)
- [x] 2.3 Demote once-only words in the writer: measure on the harness (once-only share, Drehteller count)
- [x] 2.4 Slightly wider sampling after the first token: measure on the harness and record temperature and top-k in design.md (temperature 0.5, top-k 4)
- [x] 2.5 Write the `text-model` spec delta for the writer if its behaviour became a requirement (design.md, Decision 7): requirements "Words of the event" and "Repetition block" added; "Varied texts" stays (opening 0.8 / top-k 8, rest 0.5 / top-k 4)
- [x] 2.6 Host checks: `sh tests/run_all.sh` and `pio run -e mainframe` and `pio run -e mainframe-slm-capture` pass
- [x] 2.7 Acceptance: harness better than the baseline (fewer copies, more openings, event fidelity not worse), then the user reads 60 blind pairs old against new; harness better (design.md, step 2 values); 60 pairs prepared with `text_model_rating_set.py --compare plain=1 --seed 20261121` (ignored `models/text-model/rating-set/w6x128-pre-ft-400-3e-3-compare/`, 1 pair with identical texts) and published as artifact `f30aff03-1be3-4614-85ca-b99e3b7189ad` (https://claude.ai/artifact/X1hJTPXBfMpC99UfwCLH9v, collection `answers`); the user accepted step 2 without reading the pairs and asked for the merge (2026-11-20)

## 3. Conditioning on event and lead value

- [x] 3.1 Training data: event plus lead value, non-lead levels middle; verify about 120 conditions with about 27 lines each
- [x] 3.2 Firmware request: event and lead value computed like `TextBank::leadFor`; add a host test for the scenarios of the "Request input" delta
- [x] 3.3 Retrain, export the image and run the harness (lead grid: emoji match and lead classifier against the baseline)
- [x] 3.4 Host checks: `sh tests/run_all.sh` and `pio run -e mainframe` and `pio run -e mainframe-slm-capture` pass
- [x] 3.5 Acceptance: harness, then 60 blind pairs; harness better; 60 pairs device image against the chosen model `s3-lead-pre-ft-400.bin` with the firmware writer published as artifact `8455f88f-6f5e-48b4-ac6d-b5b337c9fd23` (https://claude.ai/artifact/HLoSd4ppNQMu746UcJkwH4, collection `answers`); the user read them on 2026-11-21: new preferred 25 against 16 (13 equal, 6 without choice, p = 0.21), usable 49/59 against 45/58; accepted by the user on 2026-11-21 and merged

## 4. Regularisation

- [x] 4.1 Add dropout (0.1–0.2) and early stopping on validation loss to `apps/mainframe/tools/text_model.py`; verify the chosen checkpoint is the one with the lowest validation loss
- [x] 4.2 Train three seeds and record the noise between them in `docs/text-model-measurements.md`
- [x] 4.3 Acceptance: harness, then 60 blind pairs; not kept: event fidelity worse on the harness (design.md, step 4), so no blind pairs

## 5. Closed word list of corpus words

- [x] 5.0 Remove the words PIEKSER and DREHTELLER from the corpus (user, 2026-11-20): the 6 lines that contain them are deleted; word list and text bank header regenerated. The word list changed, so the device image `w6x128-pre-ft-400-3e-3.bin` only matches the word list up to a844a2d; measure it from there.
- [ ] 5.1 Replace words that occur fewer than three times in the corpus; regenerate the word list and the text bank header and run `python3 -I apps/mainframe/tools/text_model_corpus.py check`
- [ ] 5.2 Build the closed word list (about 1,000–1,500 corpus words), retrain and run the harness
- [ ] 5.3 Host checks: `sh tests/run_all.sh` and `pio run -e mainframe` pass
- [ ] 5.4 Acceptance: harness, then 60 blind pairs

## 6. More corpus in the classic style

- [ ] 6.1 Write synthetic lines in the classic style, filter them automatically; the user reads samples
- [ ] 6.2 Regenerate headers, retrain and run the harness
- [ ] 6.3 Acceptance: harness, then 60 blind pairs

## 7. Device

- [ ] 7.1 Host checks for the accepted model: `sh tests/run_all.sh`, `pio run -e mainframe` with model texts switched on
- [ ] 7.2 Device test on the Mainframe (AtomS3R), `mainframe-slm-capture` with the accepted image: a few motion events per kind; observe the emoji first, words growing, no repeated text in a row, and the audio and IMU gaps in the loop-load output; then flash the normal `mainframe`
- [x] 7.3 The user decides whether `mainframe` shows model texts again (user, 2026-11-21: `mainframe` shows model texts again, `DECAFLASH_TEXT_MODEL_TEXTS` default 1, image `s3-lead-pre-ft-400.bin`)

## Workflow follow-up

- Archive the change once every step is accepted or the user ends it.
