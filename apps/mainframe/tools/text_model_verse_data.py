#!/usr/bin/env python3
"""Unrhymed German poetry and short prose for a verse pretraining test.

Step 2 of the verse work (parked; change improve-text-model, design.md, section
"Parked: the verse work"): the text model is pretrained on poetry instead of the novels of
text_model_pretrain_data.py. The sources below were approved by the user on
2026-11-20, including the elevated prose added later that day. Nothing is versioned: `fetch` downloads into the ignored
models/text-model/verse/raw/ and writes a manifest with the SHA-256 of every file.

`prepare` cuts each source to the approved part, modernises old spelling with a
short rule list (th -> t, ey -> ei and a few frequent old forms; on every source, since
several editions use old spelling) and maps the text
to the model's alphabet with text_model_pretrain_data.py (one sentence per line).
Verse lines are joined; the model has no line-break token.

Usage:
  text_model_verse_data.py fetch     download missing sources (about one request a second)
  text_model_verse_data.py prepare   write models/text-model/verse/text.txt
  text_model_verse_data.py old       list the most frequent words that look like old spelling
  text_model_verse_data.py words     write models/text-model/verse/book_words.txt
  text_model_verse_data.py vocab [--size N]
                                     write the closed word list of the verse text model,
                                     apps/mainframe/text_model/verse_words.txt: every word
                                     of the verse corpus, every word of word_fields.tsv,
                                     then the most frequent words of text.txt up to N words
                                     (default 2,500, chosen on 2026-11-20); prints coverage
  --variant short-name (prepare, old, words): read and write models/text-model/verse/<name>/
  instead; the variant leaves out the sources in VARIANTS[name].
"""

import argparse
import collections
import hashlib
import html
import io
import pathlib
import re
import sys
import time
import urllib.parse
import urllib.request
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_pretrain_data as books  # noqa: E402
import text_model_vocab as vocab_module  # noqa: E402
import text_model_word_fields as word_fields  # noqa: E402

REPO = vocab_module.REPO
VERSE = REPO / "models/text-model/verse"
RAW = VERSE / "raw"
TEXT = VERSE / "text.txt"
BOOK_WORDS = VERSE / "book_words.txt"
AGENT = {"User-Agent": "Mozilla/5.0"}

ZENO = "http://www.zeno.org"
# (name, path below zeno.org); every page below the path is taken.
ZENO_WORKS = [
    ("rilke-elegien", "/Literatur/M/Rilke,+Rainer+Maria/Gedichte/Duineser+Elegien"),
    ("rilke-requiem", "/Literatur/M/Rilke,+Rainer+Maria/Gedichte/Requiem"),
    ("hoelderlin-1800", "/Literatur/M/H%C3%B6lderlin,+Friedrich/Gedichte/Gedichte+1800-1804"),
    ("hoelderlin-1784", "/Literatur/M/H%C3%B6lderlin,+Friedrich/Gedichte/Gedichte+1784-1800"),
    ("hoelderlin-hyperion", "/Literatur/M/H%C3%B6lderlin,+Friedrich/Roman/Hyperion+oder+der+Eremit+in+Griechenland"),
    ("klopstock-oden-1", "/Literatur/M/Klopstock,+Friedrich+Gottlieb/Gedichte/Oden.+Erster+Band"),
    ("klopstock-oden-2", "/Literatur/M/Klopstock,+Friedrich+Gottlieb/Gedichte/Oden.+Zweiter+Band"),
    ("stramm", "/Literatur/M/Stramm,+August/Gedichte"),
    ("holz-phantasus", "/Literatur/M/Holz,+Arno/Gedichte/Phantasus"),
    ("nietzsche-dithyramben", "/Philosophie/M/Nietzsche,+Friedrich/Dionysos-Dithyramben"),
    ("lichtenberg", "/Literatur/M/Lichtenberg,+Georg+Christoph/Aufzeichnungen+und+Aphorismen"),
    # Elevated prose in the tone of Hyperion, added by the user on 2026-11-20.
    ("hoelderlin-empedokles", "/Literatur/M/H%C3%B6lderlin,+Friedrich/Drama/Der+Tod+des+Empedokles"),
    ("novalis-ofterdingen", "/Literatur/M/Novalis/Romane/Heinrich+von+Ofterdingen"),
] + [
    (f"goethe-{i}", "/Literatur/M/Goethe,+Johann+Wolfgang/Gedichte/Gedichte+(Ausgabe+letzter+Hand.+1827)/Vermischte+Gedichte/" + poem)
    for i, poem in enumerate([
        "Prometheus", "Ganymed", "Harzreise+im+Winter", "Wandrers+Sturmlied", "An+Schwager+Kronos",
        "Mahomets-Gesang", "Grenzen+der+Menschheit", "Das+G%C3%B6ttliche",
        "Gesang+der+Geister+%C3%BCber+den+Wassern", "Seefahrt", "Adler+und+Taube"])
]

# Project Gutenberg mirror as in text_model_pretrain_data.py: (ebook, title, file, encoding, part)
GUTENBERG = [
    (29006, "Trakl: Die Dichtungen", "29006-8.txt", "latin-1", None),
    (43821, "Novalis: Hymnen an die Nacht", "43821-0.txt", "utf-8", "novalis"),
    (3498, "Heine: Die Nordsee", "3498-8.txt", "latin-1", "nordsee"),
    (7205, "Nietzsche: Also sprach Zarathustra", "7205-0.txt", "utf-8", None),
    (7207, "Nietzsche: Menschliches, Allzumenschliches", "7207-8.txt", "latin-1", None),
    (77889, "Ebner-Eschenbach: Aphorismen", "77889-0.txt", "utf-8", "aphorismen"),
    (2188, "Rilke: Die Aufzeichnungen des Malte Laurids Brigge", "2188-8.txt", "latin-1", None),
    (24043, "Rilke: Die Weise von Liebe und Tod des Cornets Christoph Rilke", "24043-0.txt", "utf-8", None),
    (2407, "Goethe: Die Leiden des jungen Werther, Buch 1", "2407-0.txt", "utf-8", None),
    (2408, "Goethe: Die Leiden des jungen Werther, Buch 2", "2408-0.txt", "utf-8", None),
]

LUTHER = "https://ebible.org/Scriptures/deu1912_readaloud.zip"
LUTHER_BOOKS = ("PSA", "PRO", "ECC")


# Variants of the material, decided by the user. "no-bible" (2026-11-20, after the
# first blind reading: too many Bible verses): without Luther, Zarathustra and Klopstock.
VARIANTS = {"no-bible": {"luther", "7205-0.txt", "klopstock-oden-1", "klopstock-oden-2"}}


def use_variant(name):
    global TEXT, BOOK_WORDS, LEAVE_OUT
    folder = VERSE / name
    folder.mkdir(parents=True, exist_ok=True)
    TEXT, BOOK_WORDS, LEAVE_OUT = folder / "text.txt", folder / "book_words.txt", VARIANTS[name]


LEAVE_OUT = set()


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=60) as r:
        data = r.read()
    time.sleep(1)
    return data


def zeno_page_text(page):
    """Text of a Zeno page and the links of its main part."""
    main = page.split('<div class="zenoCOMain">', 1)[-1].split('<div class="zenoCOFooter">', 1)[0]
    links = re.findall(r'href="(/[^"#?]+)"', main)
    main = re.sub(r'<a [^>]*class="zenoTXKonk"[^>]*>.*?</a>', "", main)
    main = re.sub(r"<br\s*/?>|</p>|</div>|</h\d>", "\n", main)
    text = html.unescape(re.sub(r"<[^>]+>", "", main))
    return text, links


def zeno_fetch(name, path):
    """Every page below path; pages with child pages are indexes, the rest text."""
    folder = RAW / "zeno" / name
    folder.mkdir(parents=True, exist_ok=True)
    todo, seen, n = [path], {path}, 0
    while todo:
        current = todo.pop(0)
        file = folder / (hashlib.sha1(current.encode()).hexdigest()[:16] + ".html")
        if not file.exists():
            file.write_bytes(get(ZENO + current))
        page = file.read_bytes().decode("cp1252", errors="replace")
        if "Gemeinfrei" not in page:
            raise SystemExit(f"no Gemeinfrei licence on {current}")
        _, links = zeno_page_text(page)
        children = [l for l in links if l.startswith(current + "/") and l not in seen]
        for l in children:
            seen.add(l)
            todo.append(l)
        n += 1
    print(f"ok  zeno {name}: {n} pages")


def fetch():
    RAW.mkdir(parents=True, exist_ok=True)
    for ebook, title, file, _, _ in GUTENBERG:
        path = RAW / file
        if not path.exists():
            path.write_bytes(get(books.url(ebook, file)))
        print(f"ok  {file}  {title}")
    luther = RAW / "deu1912_readaloud.zip"
    if not luther.exists():
        luther.write_bytes(get(LUTHER))
    print("ok  deu1912_readaloud.zip")
    for name, path in ZENO_WORKS:
        zeno_fetch(name, path)
    manifest = [f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(RAW)}"
                for p in sorted(RAW.rglob("*")) if p.is_file() and p.name != "manifest.txt"]
    (RAW / "manifest.txt").write_text("\n".join(manifest) + "\n", encoding="utf-8")


def zeno_text(name, path):
    """Text pages of one Zeno work, in crawl order, as one block per page."""
    folder = RAW / "zeno" / name
    todo, seen, out = [path], {path}, []
    while todo:
        current = todo.pop(0)
        file = folder / (hashlib.sha1(current.encode()).hexdigest()[:16] + ".html")
        text, links = zeno_page_text(file.read_bytes().decode("cp1252", errors="replace"))
        children = [l for l in links if l.startswith(current + "/") and l not in seen]
        if not children:
            out.append(text)
        for l in children:
            seen.add(l)
            todo.append(l)
    return out


def part(text, which):
    """The approved part of a Gutenberg book."""
    if which is None:
        return text
    marks = {
        "novalis": (r"^\s*Hymnen an die Nacht\s*$", r"^\s*Die Christenheit\s*$"),
        # Die Nordsee is the last part of the Buch der Lieder.
        "nordsee": (r"^\s*Die Nordsee\s*$", r"\Z"),
        "aphorismen": (r"^\s*Aphorismen\.\s*$", r"^\s*Parabeln und Märchen\.\s*$"),
    }[which]
    starts = [m.start() for m in re.finditer(marks[0], text, re.M)]
    # The last heading match is the text itself, not the table of contents.
    start = starts[-1] if starts else None
    end = re.search(marks[1], text[start:], re.M) if start is not None else None
    if start is None or end is None or end.start() < 1000:
        raise SystemExit(f"part {which} not found")
    return text[start:start + end.start()]


def luther_text():
    out = []
    with zipfile.ZipFile(RAW / "deu1912_readaloud.zip") as z:
        for name in sorted(z.namelist()):
            if any(f"_{b}_" in name for b in LUTHER_BOOKS) and name.endswith(".txt"):
                lines = z.read(name).decode("utf-8-sig").splitlines()
                # First lines are the book and chapter title.
                out.append("\n".join(l for l in lines[2:] if l.strip()))
    if not out:
        raise SystemExit("no Luther chapters found")
    return out


# Old spelling (Klopstock, Ebner-Eschenbach, Nietzsche editions): word-level rules
# on the upper-case text. th -> t only where today's spelling has no th.
OLD = [
    (r"\bTH", "T"), (r"(?<=[A-ZÄÖÜ])TH(?=[A-ZÄÖÜ]|\b)", "T"),
    (r"EY", "EI"), (r"\bSEYN\b", "SEIN"),
    (r"\bGIEBT\b", "GIBT"), (r"\bGIEBST\b", "GIBST"), (r"\bBROD\b", "BROT"),
    (r"\bTODT", "TOT"), (r"\bIZT\b|\bJEZT\b", "JETZT"), (r"\bITZT\b", "JETZT"),
    (r"\bAET(H)?ER", "ÄTHER"), (r"\bCULTUR", "KULTUR"), (r"GIEB(?=S?T\b)", "GIB"),
]
# Words with a real th today (after the rules above they would lose it).
KEEP_TH = {"THEATER", "THEMA", "THRON", "THEORIE", "MYTHOS", "RHYTHMUS", "ATHEN", "ÄTHER",
           "THESEUS", "PANTHER", "APOTHEKE", "MATHEMATIK", "METHODE", "LUTHER", "GOETHE",
           "ZARATHUSTRA", "THOMAS", "ETHIK", "THESE", "SYMPATHIE", "THEOLOG", "ATHLET", "KATHOLI",
           "ORTHO", "ENTHUSIAS", "PATHOS", "PATHETI", "HYPOTHES", "ATHENE", "PYTHIA", "LETHE",
           "BETHLEHEM", "ELISABETH", "GOTHIK", "LABYRINTH", "THRAZ", "THEBEN", "THESSAL",
           "ÄSTHETI", "PROMETHEUS", "ARITHMETI", "NATHAN", "MARTHA", "AGATHE", "BERTHA", "THEKLA"}


def modernise(line):
    words = []
    for w in line.split(" "):
        core = re.sub(r"[^A-ZÄÖÜ']", "", w)
        if any(core.startswith(k) for k in KEEP_TH):
            words.append(w)
            continue
        for old, new in OLD:
            w = re.sub(old, new, w)
        words.append(w)
    return " ".join(words)


def blocks():
    """(source, raw text) for every approved source."""
    for ebook, title, file, encoding, which in GUTENBERG:
        text = (RAW / file).read_bytes().decode(encoding).replace("\r\n", "\n")
        yield file, part(books.body(text), which)
    for chapter in luther_text():
        yield "luther", chapter
    for name, path in ZENO_WORKS:
        for page in zeno_text(name, path):
            yield name, page


def prepare():
    lines, sizes = [], collections.Counter()
    for source, text in blocks():
        if source in LEAVE_OUT:
            continue
        # Verse lines inside a stanza are joined; stanzas stay paragraphs.
        text = re.sub(r"[ \t]+\n", "\n", text)
        text = re.sub(r"(?<!\n)\n(?!\n)", " ", text)
        for sentence in books.sentences(text):
            if re.fullmatch(r"[IVXLC0-9., ]+", sentence):
                continue  # section numbers
            sentence = modernise(sentence)
            lines.append(sentence)
            sizes[source.split("-")[0] if source.startswith(("goethe", "klopstock")) else source] += len(sentence) + 1
    TEXT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    for source, n in sizes.most_common():
        print(f"{n:9d}  {source}")
    print(f"wrote {TEXT}: {len(lines)} sentences, {sum(sizes.values())} characters")


def old():
    """Frequent words with letter groups typical of old spelling, for checking."""
    counts = collections.Counter()
    for line in TEXT.read_text(encoding="utf-8").splitlines():
        counts.update(re.findall(r"[A-ZÄÖÜ']+", line))
    pattern = re.compile(r"TH|EY|AE|OE|UE|IE[BD]T|DT\b|^IZT|SZ|C[^H]|QV")
    for w, n in counts.most_common():
        if pattern.search(w) and n >= 5:
            print(n, w)


def words():
    """Book words of the verse text, as text_model_pretrain_data.py words does."""
    corpus = set(vocab_module.corpus_words())
    counts = collections.Counter()
    for line in TEXT.read_text(encoding="utf-8").splitlines():
        counts.update(p for p in vocab_module.split_body(line) if p not in vocab_module.PUNCTUATION)
    room = vocab_module.MAX_WORDS - len(corpus)
    usable = lambda w: re.fullmatch(r"'?[A-ZÄÖÜ][A-ZÄÖÜ']*", w) and not (len(w) <= 2 and w.endswith("'"))
    added = [w for w, _ in counts.most_common() if w not in corpus and usable(w)][:room]
    BOOK_WORDS.write_text("\n".join(added) + "\n", encoding="utf-8")
    known = corpus | set(added)
    total = sum(counts.values())
    covered = sum(n for w, n in counts.items() if w in known)
    print(f"wrote {BOOK_WORDS}: {len(added)} book words; with the corpus words they cover "
          f"{100 * covered / total:.1f} % of {total} words; least frequent occurs "
          f"{counts[added[-1]] if added else 0} times")


VOCAB_SIZE = 2500  # words of the verse text model (2026-11-20, docs/text-model-measurements.md)


def usable(word):
    """A pretraining word needs a letter; a bare apostrophe, a digit run or a quote
    mark stuck to one letter ("S'") is left out."""
    return re.fullmatch(r"'?[A-ZÄÖÜ][A-ZÄÖÜ']*", word) and not (
        len(word) <= 2 and word.endswith("'"))


def vocab(size=VOCAB_SIZE):
    """Closed word list: verse corpus words, field words, frequent pretraining words."""
    verse = collections.Counter(p for body in vocab_module.verse_bodies()
                                for p in vocab_module.split_body(body)
                                if p not in vocab_module.PUNCTUATION)
    fields = set().union(*word_fields.read_fields(word_fields.FIELDS).values())
    lines = TEXT.read_text(encoding="utf-8").splitlines()
    pieces = [vocab_module.split_body(line) for line in lines]
    counts = collections.Counter(p for ps in pieces for p in ps if p not in vocab_module.PUNCTUATION)
    required = set(verse) | fields
    if len(required) > size:
        raise SystemExit(f"{len(required)} required words, more than --size {size}")
    added = [w for w, _ in sorted(counts.items(), key=lambda item: (-item[1], item[0]))
             if w not in required and usable(w)][:size - len(required)]
    words = required | set(added)
    header = [
        "# Closed word list of the verse text model (text_model_verse_data.py vocab).",
        f"# {len(words)} words: {len(verse)} verse corpus words, {len(fields - set(verse))} more "
        f"field words, {len(added)} frequent pretraining words.",
    ]
    vocab_module.VERSE_WORDS.write_text("\n".join(header + sorted(words)) + "\n", encoding="utf-8")
    # Coverage, as the pretraining reads the text: a word outside the list cuts a
    # sentence, runs of at least three known tokens are kept (text_model.py).
    known = lambda p: p in words or p in vocab_module.PUNCTUATION
    total = kept = whole = ends = with_run = characters = 0
    for ps in pieces:
        total += len(ps)
        run, got = [], False
        for p in ps + [None]:
            if p is not None and known(p):
                run.append(p)
                continue
            if len(run) >= 3:
                kept += len(run)
                characters += len(vocab_module.join_body(run))
                ends += p is None
                got = True
            run = []
        with_run += got
        whole += all(known(p) for p in ps)
    word_total = sum(counts.values())
    print(f"wrote {vocab_module.VERSE_WORDS}: {len(words)} words")
    print(f"verse corpus: {sum(verse.values())} word tokens, {len(verse)} words, all in")
    print(f"field words: {sum(1 for w in fields if w in words)} of {len(fields)} in, "
          f"{sum(1 for w in fields if not counts[w])} of them absent from the pretraining text")
    print(f"pretraining text: {len(lines)} sentences; {100 * sum(n for w, n in counts.items() if w in words) / word_total:.1f} % "
          f"of {word_total} word tokens known; kept in runs of at least three tokens: "
          f"{100 * kept / total:.1f} % of {total} tokens ({characters} characters); "
          f"sentences entirely known {100 * whole / len(lines):.1f} %, with a kept run "
          f"{100 * with_run / len(lines):.1f} %, keeping their end {100 * ends / len(lines):.1f} %")
    if added:
        print(f"least frequent added pretraining word occurs {counts[added[-1]]} times")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("command", choices=["fetch", "prepare", "old", "words", "vocab"])
    parser.add_argument("--variant", choices=sorted(VARIANTS))
    parser.add_argument("--size", type=int, default=VOCAB_SIZE, help="vocab: words in all")
    args = parser.parse_args()
    if args.variant:
        use_variant(args.variant)
    if args.command == "vocab":
        vocab(args.size)
        return 0
    {"fetch": fetch, "prepare": prepare, "old": old, "words": words}[args.command]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
