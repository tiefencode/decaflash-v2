# Tasks

Each step ends with its acceptance (design.md, Decision 2): harness first, then 60 blind pairs old against new. The next step starts only after that.

## 1. Measurement harness, gold file and new rating set

- [x] 1.1 Collect every user rating into one gold file; verify `apps/mainframe/text_model/ratings_superseded_2026-11-18.tsv` holds the 179 ratings (e81997a)
- [x] 1.2 Build the fixed harness `apps/mainframe/tools/text_model_harness.py`; verify three seed blocks run on the device image (f3a0d75)
- [x] 1.3 Record the baseline of `w6x128-pre-ft-400-3e-3.bin` in `docs/text-model-measurements.md`, section "Harness baseline", and fix the stale README statements (ad01ba0, dd02bed)
- [x] 1.4 Mark the old ratings superseded and score the harness proxies against them; verify the table "Automatic proxies against the old ratings" (200a06e)
- [x] 1.5 Prepare the new rating set of 144 blind pairs with `apps/mainframe/tools/text_model_rating_set.py`; verify `pairs.json` and the hidden `key.json` exist under `models/text-model/rating-set/w6x128-pre-ft-400-3e-3/` (b0101dc)
- [ ] 1.6 Build the Artifact page that shows the 144 pairs and stores the answers; verify a test answer is stored and read back
- [ ] 1.7 The user reads the new rating set (usable or not per text, A/B/equal per pair)
- [ ] 1.8 Score the harness proxies against the new ratings and record the result in `docs/text-model-measurements.md`

## 2. Writer

- [ ] 2.1 Repetition block: start from the prototype on `claude/text-model-repetition-a5d96d` (f927b96), measure it on the harness and record values in design.md
- [ ] 2.2 Word mask per event: measure on the harness (event classifier, field words, copies)
- [ ] 2.3 Demote once-only words in the writer: measure on the harness (once-only share, Drehteller count)
- [ ] 2.4 Slightly wider sampling after the first token: measure on the harness and record temperature and top-k in design.md
- [ ] 2.5 Write the `text-model` spec delta for the writer if its behaviour became a requirement (design.md, Decision 7)
- [ ] 2.6 Host checks: `sh tests/run_all.sh` and `pio run -e mainframe` and `pio run -e mainframe-slm-capture` pass
- [ ] 2.7 Acceptance: harness better than the baseline (fewer copies, more openings, event fidelity not worse), then the user reads 60 blind pairs old against new

## 3. Conditioning on event and lead value

- [ ] 3.1 Training data: event plus lead value, non-lead levels middle; verify about 120 conditions with about 27 lines each
- [ ] 3.2 Firmware request: event and lead value computed like `TextBank::leadFor`; add a host test for the scenarios of the "Request input" delta
- [ ] 3.3 Retrain, export the image and run the harness (lead grid: emoji match and lead classifier against the baseline)
- [ ] 3.4 Host checks: `sh tests/run_all.sh` and `pio run -e mainframe` and `pio run -e mainframe-slm-capture` pass
- [ ] 3.5 Acceptance: harness, then 60 blind pairs

## 4. Regularisation

- [ ] 4.1 Add dropout (0.1–0.2) and early stopping on validation loss to `apps/mainframe/tools/text_model.py`; verify the chosen checkpoint is the one with the lowest validation loss
- [ ] 4.2 Train three seeds and record the noise between them in `docs/text-model-measurements.md`
- [ ] 4.3 Acceptance: harness, then 60 blind pairs

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
- [ ] 7.3 The user decides whether `mainframe` shows model texts again

## Workflow follow-up

- Archive the change once every step is accepted or the user ends it.
