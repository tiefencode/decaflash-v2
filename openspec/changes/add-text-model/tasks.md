# Tasks

Device for all device tasks: the Mainframe (AtomS3R with Atomic Voice Base and Unit TMOS PIR). Flashing is approved through the upload command's permission prompt; writing the model image to 0x340000 is a flash command of its own and also goes through its prompt. Ask before opening the serial port. Record every device result in `docs/project-context.md` with date, build and state, separate from host results. Downloads, package installs and the corpus writing start only after the user's explicit yes at that step.

## 1. Emoji in the first row (no model)

- [x] 1.1 Lay out the closing curated emoji of every message large and centred in row 0 with the text lines below (design.md Decision 5), counting it as one element for display time and scrolling; update `tests/message_panel.cpp` to the new layout scenarios; verify `sh tests/run_all.sh` passes and `pio run -e mainframe` builds.
- [x] 1.2 Device test (Mainframe, `mainframe`): trigger a Tap, a Shake, a presence text and a poem; verify the emoji stands large and centred above the text, a long text still scrolls and ends with its last lines visible, and show the result to the user for their look.

## 2. Growing messages in the panel (no model)

- [x] 2.1 Add a growing-message interface to `MessagePanel` (start with emoji, append word, complete, cancel) with: old message kept until the emoji arrives, display time from completion, moving up while a text grows past four rows, poem waiting while a request is open, cooldown from acceptance; extend `tests/message_panel.cpp` with the scenarios of the `message-panel` delta; verify `sh tests/run_all.sh` passes and `pio run -e mainframe` builds.
- [x] 2.2 Add the fallback path in `main.cpp`: request when the runtime is on and the panel visible, fixed text when the runtime is off, the panel hidden, or no emoji within the wait time; with no runtime in `mainframe` every event takes the fixed path; host test for the decision with a fake model; verify `sh tests/run_all.sh` and `pio run -e mainframe`.

## 3. Tokenizer and form check (host)

- [x] 3.1 Generate the character tokenizer (output alphabet of design.md Decision 6, curated emoji from `panel_glyphs`, 12 event and 15 mood tokens, start and end) as one table shared by the firmware and the host tools; host test: every output token is drawable by the panel and every curated emoji has exactly one token.
- [x] 3.2 Write the form check as a host tool and test: alphabet only, emoji first and curated, emoji plus text within the hard limit of output tokens (design.md Decision 7), no verbatim copy of a training line; verify it on today's fixed texts (all pass) and on hand-made bad examples (all fail).

- [x] 3.3 Switch the tokenizer to word tokens (design.md Decision 6): generate the word list from the whole corpus into the shared table for firmware and host tools; host test: every word is made of the text alphabet, every curated emoji has exactly one token, and every corpus line encodes without an unknown word; adapt the form check to words of the list and the hard length limit of Decision 7; verify `sh tests/run_all.sh`.

## 4. Training pipeline (host, user's Mac)

- [x] 4.1 Ask the user for a yes to install the Python training packages (name the packages, source and download size); after a yes, add the training script and settings for a given size (layers, dimension) and seed.
- [x] 4.2 Add the exporter to the kernel's image format and a host test: on a smoke corpus of today's fixed texts, the host kernel on the exported image gives the same greedy output tokens as the float model for fixed inputs; record the threshold used in design.md Decision 11.

- [x] 4.3 Add the pretraining tool (design.md Decision 14): download script with source list and SHA-256 for the four Gutenberg books into the ignored `models/text-model/pretrain/`, mapping to the text alphabet, and `--pretrain` / `--init` in `text_model.py`; kept as a lever, not a pipeline step.
- [x] 4.4 Move training, exporter and the exporter host test to word tokens (3.3); verify `sh tests/run_all.sh`.
- [x] 4.5 Show the user the list of about 20 more Gutenberg books (author, title, licence, size); after a yes, add them to the source list of `text_model_pretrain_data.py` with SHA-256, download and prepare them (design.md Decision 14).
- [x] 4.6 Grow the word list to about 8,000 words (every corpus word plus the most frequent pretraining words) and set the PLE width of 6 × 128 to 64, so the image fits the model partition (design.md Decision 6); regenerate the header, keep the form check and its host tests passing, and add a check that the exported image is at most 4,915,200 bytes; verify `sh tests/run_all.sh`.
- [x] 4.7 Pretrain 6 × 128 on the books (GPU of the Mac where it helps), then train it on the corpus with `--init`; pick the step counts by the validation loss; export and run the exporter check.

## 5. Training corpus

- [x] 5.1 Agree the corpus size and the file layout with the user in the question dialog, then ask for a yes to write it in a Claude Code session; after a yes, write the tagged corpus (event, five mood levels, emoji first, text) including today's fixed texts; verify every line passes the form check of 3.2.
- [x] 5.2 Give the user samples across events and rare mood combinations to review; remove what they reject; record the final size and the validation split.

## 6. First model and growth rule

- [x] 6.1 Train 3 layers × 64 with word tokens (the character run of 2026-11-18 is recorded in design.md Decision 14), export, and generate texts with the host kernel for 30 fixed event / mood cases chosen with the user; run the form check.
- [x] 6.2 Let the user rate the 30 texts (and choose temperature and top-k while doing so); if fewer than 20 are usable or the form check fails, train the next size (4 × 96, then 6 × 128) and repeat 6.1–6.2; record the chosen size, settings, seed and image checksum in design.md and `docs/project-context.md`. Result: 3 × 64 rated 16 of 30, 4 × 96 rated 8 of 30, temperature 0.8 and top-k 8 chosen; the user then chose pretraining at 6 × 128 instead of the next ladder step (design.md Decisions 9 and 14).
- [x] 6.3 Let the user rate the pretrained 6 × 128 (temperature 0.8, top-k 8) in a blind comparison with the text bank (design.md Decision 9): for the 30 cases, two texts A and B in random order without their origin (the model's text and a text bank line for the same event and mood), each marked usable or not, and A, B or equally good chosen; the origin is shown afterwards. At 15 or more cases where the model is better or equal (the usable share is recorded only), record size, seed and image checksum in design.md and `docs/project-context.md`; below, decide the next step with the user. A rating already started in the old form (usable or not) is redone in this form or agreed with the user.

## 7. Runtime in the firmware

- [ ] 7.1 Extend the kernel in `apps/mainframe/src/slm/` with output masks (first token curated emoji, then words and punctuation), sampling with temperature, top-k and seed, the hard token limit, an output head that computes only the emoji rows for the first token (design.md Decision 3), streamed delivery of emoji and words, and cancel between forward passes; host test: fixed seed gives fixed output, masks hold, cancel stops within one pass; verify `sh tests/run_all.sh`.
- [ ] 7.2 Replace the prototype's continuous generation by the request interface (event, mood levels from the snapshot, seed) and wire it to 2.1 and 2.2; the model task sleeps without a request; add env `mainframe-slm` (design.md Decision 12) and switch `mainframe-slm-capture` to request mode with its automatic text triggers; update `README.md`; verify `sh tests/run_all.sh` and `pio run -e mainframe -e mainframe-slm -e mainframe-slm-capture`, and that `mainframe` contains no runtime symbols.

- [ ] 7.3 Choose the origin mark with the user in the question dialog (design.md Decision 7a), record the choice in design.md, and implement it in the panel drawing for texts whose emoji came from the text model. Add host test scenarios for the `message-panel` requirement "Origin of a text visible to the owner". Verify `sh tests/run_all.sh` and `pio run -e mainframe -e mainframe-slm`; on the device, check it as part of 8.2.

## 8. Device tests with the model

- [ ] 8.1 Device test (Mainframe, `mainframe-slm`, erased model partition): verify the serial line reports "no model image", no model task computes, and every gesture and presence event shows a text bank line with the emoji in row 0.
- [ ] 8.2 Device test (Mainframe, `mainframe-slm`, chosen image at 0x340000): verify texts appear emoji first and then word by word, a new gesture while writing replaces the text, sleep and debug screen use the text bank, and the output for a fixed seed equals the host; record time from event to emoji, first word and complete text, and the share of fallback texts.
- [ ] 8.3 Device test (Mainframe, `mainframe-slm-capture`): capture show with music and automatic texts with and without image in one session; verify the limits of design.md Decision 13 and report each as held or missed with the numbers; do not name a cause without a measurement. Flash the normal `mainframe` build afterwards.

## 9. Move into the normal build

- [ ] 9.1 Present the results of group 8 and ask the user for a yes to move; after a yes, give `mainframe` the model partition table and the runtime, remove `mainframe-slm`, document writing the model image in `README.md`; verify `sh tests/run_all.sh` and `pio run -e mainframe -e mainframe-slm-capture -e mainframe-loop-load`.
- [ ] 9.2 Device test (Mainframe, `mainframe`, with and without the image): verify it boots with the model partition table, texts come from the model with the image and from the text bank without it, and the show runs.

## 10. Wrap-up

- [ ] 10.1 Write the results into `docs/project-context.md` (host and hardware results apart, what was not verified on hardware), run `openspec validate add-text-model --strict`, and update the `message-panel` Current values (emoji position, wait time, the Shake text `AAH, ICH KOTZE! 🤢`) for the sync.
