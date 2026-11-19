# Tasks

## 1. Verse corpus

- [x] 1.1 Assign a lead value to each of the 90 sample verses; write top-ups so tap, shake and left have at least three verses per lead value; write 30 verses for each of the other nine events (three per lead value, forms mixed).
- [x] 1.2 User review of all verses and lead values (Claude Doc); apply the changes into `apps/mainframe/text_model/verse_corpus/`.

## 2. Bank and firmware

- [x] 2.1 `text_bank.py`: build the verse bank into `text_bank_verse_data.h` (` / ` → `\n`, length warning), `--check` covers both headers.
- [x] 2.2 `text_bank.cpp`: choose the bank by `DECAFLASH_TEXT_BANK_CLASSIC`; `platformio.ini`: `mainframe-classic`.
- [x] 2.3 `message_panel.cpp`: line break ends a line; tests in `tests/message_panel.cpp`; the text bank test covers both banks.
- [x] 2.4 README: verse bank, switching back.

## 3. Verification

- [x] 3.1 `sh tests/run_all.sh`; `pio run -e mainframe` and `pio run -e mainframe-classic`.
- [ ] 3.2 Flash `mainframe`; a few taps, shakes and a leave show verses with line breaks as rows.
