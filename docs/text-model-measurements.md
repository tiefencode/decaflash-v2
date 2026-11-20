# Text model measurements

Measurement results for the text model (`add-text-model`) and the work around it. `ideas.md` keeps only the idea, the decisions and the state of each topic and points here for the numbers.

Every number carries its origin: **host** (measured on the Mac), **device** (measured on the Mainframe), **user** (reported by the user), **computed** or **estimated** (derived or interpretation, not measured). German example texts, paths, commit ids and tool names are kept as written in the source notes.

## Tokenizer comparison: words against word parts

Topic: should the text model use word parts (BPE, SentencePiece) instead of word tokens. Design context: `openspec/changes/add-text-model/design.md`, Decision 14 (branch `claude/slm-core-design-24642e`).

### 2026-11-18, host

- Setup: host, 3 × 64 runs, error per validation text in nats, same split. Corpus at that time: 3,632 lines, 16,312 words, 2,048 distinct words.
- Results (host):
  - Word tokens: 27.7, 0 % invented words.
  - Word parts without pretraining: 31.8–33.8, 6–13 % invented words.
  - Word parts with pretraining on four Gutenberg books: 30.6–30.9, 2–4 % invented words.
  - Word tokens with pretraining: 28.3–30.7.
- Supports: word tokens were better than word parts with the corpus of that day; the Gutenberg pretraining did not help word tokens.
- Idea, not tested: invented words could be excluded at generation time by allowing only word parts that lead to a known word.

### 2026-11-19, repeat (reported by the user)

- Setup: run in a Codex worktree, not re-checked here. 12 texts per variant rated by the user; validation loss per character on the same 360 validation lines.
- Results (user, loss per character computed by the run):
  - Word tokens with Gutenberg: 5 of 12 usable, loss about 1.012.
  - SentencePiece BPE, 2,000 tokens, without Gutenberg: 5 of 12, 1.203.
  - BPE with Gutenberg: 3 of 12, 1.212.
  - SentencePiece Unigram, 2,000 tokens, with Gutenberg: 4 of 12, 1.201.
  - Unigram without Gutenberg was not tested.
- Supports: twelve texts per variant do not separate the variants. The loss per character favours word tokens, as on 2026-11-18, and the Gutenberg pretraining again shows no benefit. Word parts solve neither language quality nor event fidelity.
- Tooling: the pretraining tool is committed on the branch; according to the measuring session the experiment scripts lay in the session notes folder under `text-model-tokens/` and are not in the repository.

## Confidence gate

Topic: reject texts the model is unsure about and show a text bank line instead (OpenSpec change `add-text-model-confidence-gate`, branch `claude/text-model-confidence-gate-650684`, not built yet). The detailed numbers are in the `design.md` of that change.

### 2026-11-18

- Result as summarised in `ideas.md`: the model's confidence says nothing early in the text and only moderately much over the whole text. The gate stays as an emergency brake: abort from token 5, about 20 % aborts (host; exact basis is in the change's `design.md`).
- Device: the emoji of the text model appears after 0.6–0.7 s, so the text bank practically never takes over.
- Ratings (user): 16 of 30 texts of the text model usable, 28 of 30 of the text bank.
- Planned measurement, not yet run in full: many random cases on the Mac with the confidence value (tool from commit `1a6a4e6`); the user rates a targeted sample from low, medium and high confidence without seeing the value; plus a Claude rating calibrated to the user's judgements over a few hundred texts.

## Text bank line ratings

### 2026-11-18

- Two Claude raters judged 194 of 200 text bank lines usable (97 %); the user had judged 28 of 30 usable.
- The raters agree with the user's judgement only 60–68 % of the time (computed), so the 97 % says little about the user's standard.

## Per-axis mood response

Topic: how strongly the text model follows the mood levels (loneliness, annoyance, attention, energy, depression).

### 2026-11-18, host

- Setup: `text_model_write`, image `w6x128-pre-ft-400-3e-3.bin`, 300 seeds per case.
- Event "Klopfer": "ICH BIN NICHT EINSAM" ends 8 texts at loneliness low, 2 at middle, 10 at high.
- Event "Kommt davor": 71 to 104 of 300 texts are about visitors or company, independent of the loneliness level.
- Read at the time: the model barely follows the level; the event decides what it writes.

### 2026-11-19, host (re-measurement)

- Setup: `text_model_write`, 300 seeds per case, event numbers 1 and 5; one mood axis at a time set to low, middle and high, the other four at middle.
- Emoji follow each axis clearly and in the expected direction:
  - Annoyance high: 🙄 😤 💢 in 231 of 300; low: 🙂 😂 😌.
  - Loneliness high: 🥺 🥹 😢 in 208; low: 🙂 😌.
  - Attention high: 👁 in 137; low: 🫠 in 130.
  - Energy low: 🫩 in 87; high: 🙌 in 130.
  - Depression high: 🖤 💀 in 127.
- Words follow more weakly but in the right direction (counts at low / middle / high):
  - NERV/GENUG/HÖR AUF/LASS under annoyance: 1 / 1 / 9 and 0 / 0 / 18 (two event numbers).
  - MÜDE/GÄHN/SCHLAF under energy: 8 / 1 / 0 and 18 / 7 / 2.
  - EINSAM/ALLEIN under loneliness: 58 / 19 / 36 and 37 / 8 / 17, so most frequent at low loneliness ("ICH BIN NICHT EINSAM").
- The word distribution between low and high differs per axis by 1.7 to 3 times the noise between two seed halves (computed).
- Supports: the finding "barely any response" holds only for the one sentence ending counted on 2026-11-18. The model reacts to the levels, mostly through the emoji. A permanently low loneliness roughly triples (computed) the texts with EINSAM/ALLEIN compared to the middle.

## Pinned mood and habituation

Topic: with permanently extreme mood values the text bank always draws the same lead value.

### 2026-11-18, host (code of `TextBank::leadFor`)

- The text bank takes as lead value the value with the largest distance from the middle.
- At rest (30/0/0/50/0): 33 % each annoyed, attentive and depression low.
- At the party (95/0/40/0/0): 33 % each lonely, annoyed and depression low.
- As soon as annoyance and depression have only 1 to 4 points: 100 % lonely low.
- Context: annoyance, attention and depression rest at 0, loneliness sinks to 0 at a party and stays there, energy stays high with loud music.

## Event word repetitions and corpus word counts

### Corpus words (2026-11-18/19)

- The reference model Barista gets by with 854 hand-picked words; in our corpus 956 of the 2,048 distinct words occur only once (counts).

### The "Drehteller" pattern (2026-11-19, host)

- The word occurs exactly once in the corpus (`rotate.tsv`: "SPINNST DU? ICH BIN KEIN DREHTELLER!").
- In 1,200 measured model texts (`models/text-model/gate2/cases-1200.jsonl`) "ICH BIN KEIN DREHTELLER" occurs 39 times (3.3 %), for eleven of the twelve events.
- The pattern "ICH BIN KEIN(E) …" occurs 17 times in the corpus, each time with a different, unique noun. The model masters the pattern and fills the gap with a default word (interpretation, not measured).

### Repetitions (2026-11-19, measured on the Mac)

- In the same 1,200 cases 64 texts end in "ICH BIN NICHT EINSAM.", 32 in "ICH BIN KEIN DREHTELLER!", 30 in "ICH BIN ZUFRIEDEN."; 159 are literal corpus lines.
- With the same event and mood (levels 1 1 1 0 1, like a party with low loneliness), 100 random values give only 73 and 70 distinct texts (two runs), with 8 emoji and 19 and 24 sentence openings; the most frequent text occurs 11 and 17 times.
- Causes by interpretation (not measured): the mood hardly changes on the device, the mild selection takes the most probable word almost always after the first word, and the corpus offers few continuations for frequent sentence openings.

## Verse pretraining

Topic: react with unrhymed short verses, pretrain on poetry instead of novels. Decisions and state: entry "Reaction texts as unrhymed short verses, pretraining on poetry" in `ideas.md`.

### Sources checked in public catalogues (2026-11-19, nothing downloaded)

- gutenberg.org was under maintenance during the check; sizes come from the mirror gutenberg.pglaf.org and from Zeno.org catalogue pages (about ±10 %); unchecked items are marked "ungeprüft". Zeno.org calls its texts "Gemeinfrei" and has modernised spelling; Project Gutenberg says "Public domain in the USA".
- Unrhymed, close to Rilke and Hölderlin: Hölderlin, Gedichte 1800–1804 and the unrhymed part of 1784–1800 (Zeno, about 340 KB), Hyperion (Zeno, 293 KB); Rilke, Duineser Elegien (Zeno, 42 KB) and Requiem (Zeno, 19 KB); Klopstock, Oden (Zeno, 391 KB, old spelling); Trakl, Die Dichtungen (PG #29006, 120 KB); Psalmen, Sprüche and Prediger of the Lutherbibel 1912 (eBible.org, about 380 KB, source ungeprüft); Novalis, Hymnen an die Nacht (PG #43821, about 25 KB); Goethe, freie Hymnen (Zeno, about 20 KB); Nietzsche, Dionysos-Dithyramben (Zeno, about 25 KB).
- Further unrhymed texts: Stramm (Zeno, about 25 KB), Holz, Phantasus (Zeno, 56 KB), Heine, Die Nordsee (in PG #3498, about 40 KB), aphorisms by Ebner-Eschenbach (PG #77889, about 70 KB, old spelling), Lichtenberg (Zeno, 653 KB), Nietzsche, Zarathustra (PG #7205, 547 KB) and Menschliches, Allzumenschliches (PG #7207, 583 KB), Goethe, Maximen und Reflexionen (Zeno, about 350 KB, ungeprüft).
- Rhymed, as a second group: most of Rilke (Stunden-Buch, Buch der Bilder, Neue Gedichte, Sonette an Orpheus; about 370 KB), Busch, Morgenstern, Ringelnatz (Zeno), Heine, Buch der Lieder; together about 1.1 MB.
- Amount (estimated): unrhymed about 5.4 MB, of which only about 1.5 MB verse; with the rhymed group about 6.5 MB. That is a quarter to a third of the 35 novels (assumed 20–25 MB, not re-measured). Hölderlin, Klopstock, the Elegien and the Sonette are not on gutenberg.org but on Zeno.org with one HTML page per poem, or in the Deutsches Textarchiv in old spelling (licence there unchecked).
- The full list with sizes and spelling was in the session notes on 2026-11-19 (`verse-pretraining-sources-2026-11-19.md`).

### Tone probe (2026-11-19)

- 90 example texts for knocking, shaking and someone leaving, ten per form, in the moods annoyed, lonely, tired and cheerful, all under 47 characters; the free short verses in the tone of Rilke and Hölderlin. The examples with form and event are in the Claude Doc "Kurzverse für das Wesen – Probe".

### Pretraining runs (2026-11-20, host, not on the device)

- Material, released by the user: unrhymed poetry, aphorisms and elevated prose (Hölderlin with Hyperion and Empedokles, Rilke with Elegien, Requiem, Malte and Cornet, Klopstock, Trakl, Novalis with Hymnen and Ofterdingen, Goethe with free hymns and Werther, Nietzsche, Stramm, Holz, Heine with "Die Nordsee", Luther 1912 with Psalmen, Sprüche and Prediger, Ebner-Eschenbach, Lichtenberg), no novels. Hölderlin's poems 1784–1800 are included in full, rhymed ones too (user's decision).
- Tool: `apps/mainframe/tools/text_model_verse_data.py`; data in `models/text-model/verse/` (not versioned, since deleted). After mapping to the model's alphabet 46,580 sentences with 4.6 million characters remain. Old spelling is aligned with a short rule list (th→t, ey→ei, giebt, Aether, Cultur) in all sources, because the Nietzsche and Novalis editions are also in old spelling. ß/ss is not needed because the alphabet writes ß as SS anyway.
- Two runs, both 6 × 128, 10,000 steps, batch 32, same learning rate:
  - Word tokens (corpus words plus 5,915 frequent poetry words; they cover 88.5 % of the words; sentences are cut at unknown words): 3.9 epochs.
  - Own BPE with 2,000 tokens (3.4 characters per part): 6.5 epochs.
  - Validation loss flattens for both from about step 7,000 to 9,000: word tokens 4.86 per word, word parts 3.97 per part. The numbers are not comparable because the units differ.
- Literal copying: in 27 samples per run (up to the first sentence end, at most 64 tokens) at most 6 consecutive words stand exactly so in the material (phrases like "DER HERR IST MEIN"), no whole lines. Word parts invent words (6 of 309 words, for example KORINE, UNSTUDLICH), word tokens none.
- Blind reading: 27 pairs of free continuations of verse openings, A and B assigned randomly per pair (Artifact "Lyrik-Vortraining blind", key in the session notes folder under `verse-pretrain/blind-key.tsv`). Judgement of the user (2026-11-20, user): word tokens better 13 times, word parts 6 times, 6 times equal, 2 times both unusable. Supports word tokens, but with 19 decided pairs it is not secured (sign test, two-sided p ≈ 0.17, computed).
- User remarks: too many Bible verses, and the texts are partly too long. Luther 1912 is only about 7 % of the material (computed); the biblical tone comes, by interpretation, also from Zarathustra (11 %) and Klopstock, the length from the long prose sentences (Lichtenberg, Nietzsche, Malte, Ofterdingen, Hyperion).
- Second run without Bible tone (variant `--variant no-bible`): word tokens, same size and steps, without Luther, Zarathustra and Klopstock; 3.4 million characters, 5.3 epochs. Validation loss rises again at the end (5.06 → 5.11), so the run starts to overfit. The texts become rather longer because the share of long prose rises. Blind reading against the first word-token run (27 pairs, user): whole material better 11 times, without Bible tone 6 times, 9 times equal, 1 time both unusable (not secured). Leaving material out does not help.
- Verse corpus size (estimated, not measured): 372 verses are about 31 per event, spread over the mood values; a model would probably copy or miss event and mood with that.

## First word-field counts

Topic: event word fields (first draft), counted in the old corpus and in the poetry text. (The separate section "Event word fields" of this file, if present, comes from the session "Wortfelder 1: Ankerwörter je Ereignis" and holds that session's own results.)

### 2026-11-20, host

- In the poetry text the fields are thin for knocking (0.4 % of the sentences), shaking (1.0 %), passing by (1.2 %) and turning (1.3 %).
- The fields for arriving (18 %), leaving (10 %) and double knock (9 %) consist of everyday words (DA, VOR, NOCH, WIEDER, WEG) that fit every sentence.
- In the old corpus only 25 % of the "Bewegen" lines and 26 % of the "Stoß" lines contain a word of their field.
- The table lay in the session notes folder under `verse-pretrain/word_fields_old_corpus.md`.

## Event word fields

Topic: anchor words per event. The pretraining text has no event tokens, so the model can tie an event to what it learned in pretraining only through words that the event's corpus lines share with the pretraining text. Agreed fields: `apps/mainframe/text_model/word_fields.tsv`; counts: `apps/mainframe/tools/text_model_word_fields.py` (pass `--text` when the pretraining text lives in the main checkout). The first draft is in the section "First word-field counts".

### 2026-11-20, host

- The pretraining text in `models/text-model/verse/` had been deleted; it was re-created with `text_model_verse_data.py fetch` and `prepare` and again has 46,580 sentences with 4,617,748 characters.
- Method: whole word forms instead of word stems, content words only (nouns, verbs, adjectives), no words that fit any sentence (DA, VOR, NOCH, WIEDER, WEG, KOMMT, KAM, FORT, ALLEIN, EINSAM). Words were found by searching the pretraining text for each event's situation; the user went through every field in the question dialog. Removed by the user: ZWEI (double knock, mostly counting), WENDEN/WENDET/WENDETE (turning, mostly "sich an jemanden wenden"), LIEGT and STELLE (repositioning, mostly without a change of place). Everything else stayed as proposed.
- Shared base fields: knock, double knock and multiple knock share the knocking words (KLOPFEN, POCHT, TÜR, PFORTE, FINGER); double knock is separated by ZWEIMAL, DOPPELT, ECHO, WIDERHALL, multiple knock by TROMMELN, HÄMMERN, UNAUFHÖRLICH, UNGEDULD, LÄRM; knock has no word of its own and is the base field alone. Arriving and standing farther away share NÄHER, NÄHE, NAH and are separated by coming, greeting and face (KOMMST, TRAT, GRÜSST, ANTLITZ) against distance (FERNE, ABSTAND, DRÜBEN, ZÖGERND). Impact and shaking share ERSCHÜTTERT, ERSCHÜTTERN, ERSCHÜTTERUNG.

| Event | Own words | Shared words |
|---|---|---|
| move | BEWEGEN BEWEGT BEWEGTE BEWEGUNG FAHREN FAHRT GETRAGEN HEBEN HEBT HOB REISE REISEN SCHAUKELN SCHAUKELT SCHWEBEN SCHWEBT TRAG TRAGEN TRÄGST TRÄGT UNTERWEGS WIEGEN WIEGT | – |
| tap | – | ANKLOPFEN ANKLOPFT FINGER GEKLOPFT KLOPF KLOPFEN KLOPFST KLOPFT KLOPFTE PFORTE PFORTEN POCH POCHEN POCHT POCHTE TÜR TÜRE TÜREN (with double_tap, multi_tap) |
| double_tap | DOPPELT DOPPELTE ECHO WIDERHALL ZWEIMAL | knocking base field |
| multi_tap | GETROMMELT HAMMER HÄMMERN IMMERFORT LÄRM TROMMEL TROMMELN TROMMELT UNABLÄSSIG UNAUFHÖRLICH UNGEDULD UNGEDULDIG | knocking base field |
| impact | BEULE GEHAUEN GESCHLAGEN GESTOSSEN GETROFFEN HIEB PRALLEN SCHLAG SCHLAGEN SCHLUG SCHMERZ SCHMERZEN STIESS STOSS STOSSEN STÖSST STURZ STÜRZT STÜRZTE TRAF TRIFFT WUNDE WUNDEN | ERSCHÜTTERN ERSCHÜTTERT ERSCHÜTTERUNG (with shake) |
| rotate | DREHE DREHEN DREHEND DREHN DREHT DREHTE DREHUNG GEDREHT UMGEDREHT KREIS KREISE KREISEN KREIST RAD RUND SCHWINDEL SCHWINDELT SCHWINDELTE WIRBEL WIRBELN WIRBELT | – |
| reposition | ECKE GELEGT GESETZT GESTELLT LAGE LEGT LEGTE LIEGE LIEGEN ORT PLATZ SETZT SETZTE STELLT STELLTE STÄTTE VERKEHRT | – |
| shake | BEBEN BEBEND BEBENDE BEBENDEN BEBT BEBTE ERBEBT ERBEBTE GERÜTTELT GESCHÜTTELT RÜTTELN RÜTTELT SCHAUDER SCHAUDERND SCHWANKEN SCHWANKT SCHWANKTE SCHÜTTELN SCHÜTTELST SCHÜTTELT SCHÜTTELTE TAUMEL TAUMELN TAUMELND WACKELT WANKEN WANKT ZITTERN ZITTERND ZITTERNDE ZITTERNDEN ZITTERT ZITTERTE | ERSCHÜTTERN ERSCHÜTTERT ERSCHÜTTERUNG (with impact) |
| arrived_in_front | ANGESICHT ANKOMMEN ANKUNFT ANTLITZ BEGEGNEN BEGEGNET BESUCH ERSCHEINST ERSCHEINT ERSCHIEN GAST GEGRÜSST GESICHT GRÜSSE GRÜSST HERAN KAMST KOMMST NAHEN NAHT NAHTE TRAT TRITT WILLKOMMEN | NAH NÄHE NÄHER (with stands_far) |
| walked_past | EILEND EILIG EILT FLÜCHTIG FLÜCHTIGE FLÜCHTIGEN HUSCH HUSCHT SCHRITT SCHRITTE SCHRITTEN STREIFT VORBEI VORBEIGEHEN VORBEIGEHT VORÜBER VORÜBERGEGANGEN VORÜBERGEHEN VORÜBERGEHN VORÜBERGEHT VORÜBERGING WANDERER WANDRER | – |
| left | ABSCHIED ABSCHIEDE ABSCHIEDS ENTFLOH FORTGEHEN FORTGEHT GEGANGEN GEHST GESCHIEDEN GINGST LEBEWOHL LEER LEERE SCHEIDEN SCHEIDEND SCHEIDENDEN SCHEIDET SCHWELLE VERLASSE VERLASSEN VERLASST VERLÄSST VERSCHWAND VERSCHWINDEN VERSCHWINDET | – |
| stands_far | ABSTAND DRÜBEN ENTFERNT ENTFERNUNG FERN FERNE FERNEN FERNHER HARREN HORIZONT RAND SAUM SCHEU WEITE WEITEM WEITEN ZÖGERND ZÖGERT | NAH NÄHE NÄHER (with arrived_in_front) |

- Coverage and ambiguity, before (first draft, its stems expanded to all word forms in the texts) and after (agreed fields), host. Pretraining: share of the 46,580 sentences with a field word. Old corpus and verse corpus: share of the event's lines with a field word (verse corpus: the 372 lines of `verse_corpus/`). Ambiguous: share of the pretraining sentences hit by the field that also contain a word of another event's field (shared words not counted).

| Event | Pretraining before → after | Old corpus before → after | Verse corpus before → after | Ambiguous before → after |
|---|---|---|---|---|
| move | 5.2 % → 1.8 % | 25 % → 34 % | 63 % → 63 % | 61 % → 19 % |
| tap | 0.4 % → 0.5 % | 64 % → 33 % | 97 % → 94 % | 68 % → 20 % |
| double_tap | 9.1 % → 0.6 % | 80 % → 54 % | 90 % → 77 % | 57 % → 22 % |
| multi_tap | 7.7 % → 0.8 % | 57 % → 33 % | 60 % → 50 % | 56 % → 19 % |
| impact | 2.8 % → 1.4 % | 26 % → 31 % | 37 % → 30 % | 57 % → 18 % |
| rotate | 1.3 % → 0.5 % | 64 % → 44 % | 83 % → 47 % | 65 % → 23 % |
| reposition | 6.7 % → 1.5 % | 37 % → 30 % | 47 % → 47 % | 58 % → 17 % |
| shake | 1.0 % → 0.8 % | 54 % → 49 % | 65 % → 74 % | 62 % → 20 % |
| arrived_in_front | 18.1 % → 2.6 % | 44 % → 22 % | 60 % → 50 % | 50 % → 17 % |
| walked_past | 1.2 % → 1.1 % | 54 % → 42 % | 70 % → 70 % | 64 % → 21 % |
| left | 10.4 % → 1.2 % | 53 % → 19 % | 91 % → 18 % | 54 % → 19 % |
| stands_far | 3.8 % → 1.8 % | 47 % → 32 % | 90 % → 77 % | 64 % → 22 % |

- Together the agreed fields hit 11.8 % of the pretraining sentences. Sentences per event (computed): move 823, impact 668, reposition 684, arriving 1,217, standing farther away 853, leaving 553, passing by 523, shaking 383, turning 216, knocking base field 224 (double knock own words 65, multiple knock own words 135).
- The ambiguity of about 20 % is roughly what chance gives for long sentences: a sentence with one field word often has another field's word somewhere else (interpretation, not measured against a shuffled baseline).
- Lower corpus coverage after the change is expected: the old corpus speaks colloquially (KLOPFER, TIPP, HALLO, TSCHÜSS, RUMMS, BEULE), and these words do not occur in the pretraining text; the verse lines for leaving use images (DEINE SCHRITTE WERDEN LEISER) that the field does not catch.

### Conclusions (interpretation)

- The poetry text offers too little for knocking, double knock and multiple knock whatever the words: KLOPF and POCH forms occur in about 30 sentences, ZWEIMAL in 18, the drum and hammer words in about 20. TÜR, PFORTE and FINGER add about 200 sentences, but most of them are not about knocking; the own words of double knock (ECHO, DOPPELT) and of multiple knock (UNGEDULD, UNAUFHÖRLICH) catch counting, echo and impatience, not the event. Turning is thin as well (216 sentences, many of them "Kreis" and "Rad" as images).
- Shaking, impact, passing by and leaving have a few hundred sentences each that often fit the situation, though impact leans to wounds and pain of the soul, and shaking to trembling with emotion.
- For the verse corpus: each event's lines should carry at least one word of its field, so that the field words the model knows from pretraining become the bridge to the event. For the knock family the bridge is the base field plus ZWEIMAL/DOPPELT/ECHO and TROMMELN/HÄMMERN.
- For the planned comparison with pretraining lines labelled by event: labels by word field would mark 11.8 % of the sentences, with a reasonable share of fitting situations for move, impact, shaking, arriving, passing by, leaving and standing farther away, but almost no true knocking situations. A labelled run therefore cannot teach the knock events anything from pretraining; for them only the verse corpus counts.
