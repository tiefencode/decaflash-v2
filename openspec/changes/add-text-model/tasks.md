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
- [x] 3.2 Write the form check as a host tool and test: alphabet only, emoji first and curated, emoji plus text within the hard limit of output pieces (design.md Decision 7), no verbatim copy of a training line; verify it on today's fixed texts (all pass) and on hand-made bad examples (all fail).

## 4. Training pipeline (host, user's Mac)

- [x] 4.1 Ask the user for a yes to install the Python training packages (name the packages, source and download size); after a yes, add the training script and settings for a given size (layers, dimension) and seed.
- [x] 4.2 Add the exporter to the kernel's image format and a host test: on a smoke corpus of today's fixed texts, the host kernel on the exported image gives the same greedy output pieces as the float model for fixed inputs; record the threshold used in design.md Decision 11.

## 5. Training corpus

- [ ] 5.1 Agree the corpus size and the file layout with the user in the question dialog, then ask for a yes to write it in a Claude Code session; after a yes, write the tagged corpus (event, five mood levels, emoji first, text) including today's fixed texts; verify every line passes the form check of 3.2.
- [ ] 5.2 Give the user samples across events and rare mood combinations to review; remove what they reject; record the final size and the validation split.

## 6. First model and growth rule

- [ ] 6.1 Train 3 layers × 64, export, and generate texts with the host kernel for 30 fixed event / mood cases chosen with the user; run the form check.
- [ ] 6.2 Let the user rate the 30 texts (and choose temperature and top-k while doing so); if fewer than 20 are usable or the form check fails, train the next size (4 × 96, then 6 × 128) and repeat 6.1–6.2; record the chosen size, settings, seed and image checksum in design.md and `docs/project-context.md`.

## 7. Runtime in the firmware

- [ ] 7.1 Extend the kernel in `apps/mainframe/src/slm/` with output masks (first piece curated emoji, then text alphabet), sampling with temperature, top-k and seed, the hard piece limit, streamed delivery of emoji and words, and cancel between forward passes; host test: fixed seed gives fixed output, masks hold, cancel stops within one pass; verify `sh tests/run_all.sh`.
- [ ] 7.2 Replace the prototype's continuous generation by the request interface (event, mood levels from the snapshot, seed) and wire it to 2.1 and 2.2; the model task sleeps without a request; add env `mainframe-slm` (design.md Decision 12) and switch `mainframe-slm-capture` to request mode with its automatic text triggers; update `README.md`; verify `sh tests/run_all.sh` and `pio run -e mainframe -e mainframe-slm -e mainframe-slm-capture`, and that `mainframe` contains no runtime symbols.

## 8. Device tests with the model

- [ ] 8.1 Device test (Mainframe, `mainframe-slm`, erased model partition): verify the serial line reports "no model image", no model task computes, and every gesture and presence event shows a fixed text with the emoji in row 0.
- [ ] 8.2 Device test (Mainframe, `mainframe-slm`, chosen image at 0x340000): verify texts appear emoji first and then word by word, a new gesture while writing replaces the text, sleep and debug screen use the fixed texts, and the output for a fixed seed equals the host; record time from event to emoji, first word and complete text, and the share of fallback texts.
- [ ] 8.3 Device test (Mainframe, `mainframe-slm-capture`): capture show with music and automatic texts with and without image in one session; verify the limits of design.md Decision 13 and report each as held or missed with the numbers; do not name a cause without a measurement. Flash the normal `mainframe` build afterwards.

## 9. Move into the normal build

- [ ] 9.1 Present the results of group 8 and ask the user for a yes to move; after a yes, give `mainframe` the model partition table and the runtime, remove `mainframe-slm`, document writing the model image in `README.md`; verify `sh tests/run_all.sh` and `pio run -e mainframe -e mainframe-slm-capture -e mainframe-loop-load`.
- [ ] 9.2 Device test (Mainframe, `mainframe`, with and without the image): verify it boots with the model partition table, texts come from the model with the image and from the fixed lists without it, and the show runs.

## 10. Wrap-up

- [ ] 10.1 Write the results into `docs/project-context.md` (host and hardware results apart, what was not verified on hardware), run `openspec validate add-text-model --strict`, and update the `message-panel` Current values (emoji position, wait time) for the sync.
