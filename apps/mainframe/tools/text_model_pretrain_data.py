#!/usr/bin/env python3
"""General German text for pretraining the text model (add-text-model, design.md Decision 14).

The corpus of reaction texts is too small to teach the text model German
spelling and grammar, so the model first learns general German from
public-domain books of Project Gutenberg (35 books, mostly dark 19th-century prose) and is then fine-tuned on the corpus.
The books are not versioned: this tool downloads them into the ignored
models/text-model/pretrain/ and checks each file against its SHA-256 below.

`prepare` maps the books to the model's text alphabet (text_model_vocab.py):
capitals with Ä Ö Ü (ß becomes SS, as in the corpus), digits and . , ! ? - '.
Semicolon and colon become a comma, dashes a hyphen, typographic apostrophes
the apostrophe; quotation marks and every other character are dropped, and
accented letters lose their accent. The result is one sentence per line.

Usage:
  text_model_pretrain_data.py fetch      download missing books, check checksums
  text_model_pretrain_data.py prepare    write models/text-model/pretrain/text.txt
  text_model_pretrain_data.py words      write apps/mainframe/text_model/book_words.txt:
                                         the most frequent words of text.txt that are
                                         not corpus words, most frequent first; the
                                         vocabulary takes them until MAX_WORDS; then
                                         regenerate the vocabulary header
"""

import argparse
import hashlib
import pathlib
import re
import sys
import unicodedata
import urllib.request

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_vocab as vocab_module  # noqa: E402

REPO = vocab_module.REPO
PRETRAIN = REPO / "models/text-model/pretrain"
RAW = PRETRAIN / "raw"
TEXT = PRETRAIN / "text.txt"

# Official Project Gutenberg mirror (gutenberg.org was down for maintenance on
# 2026-11-18). All books are public domain in the USA, and their authors died
# before 1956, so they are public domain in Germany too. Approved by the user
# on 2026-11-18. Old spelling is allowed (design.md Decision 14).
MIRROR = "https://mirror.csclub.uwaterloo.ca/gutenberg"
SOURCES = [
    # (ebook, title, file, encoding, sha256)
    (5323, "Theodor Fontane: Effi Briest", "5323-0.txt", "utf-8",
     "f582a4b31c666cfb188fbf95963a3dbe22aa45a51c5ae6658cbb3392a5969241"),
    (22367, "Franz Kafka: Die Verwandlung", "22367-8.txt", "latin-1",
     "fe91c871104780ce5a8fe3e12e79b757f6fb5ffa28ed83d6186e618fb3bf344a"),
    (34811, "Thomas Mann: Buddenbrooks", "34811-8.txt", "latin-1",
     "6f3975bb68ea0f562fd23a5d06580ea5c547a88009b313e12f977d705d75b60c"),
    (69327, "Franz Kafka: Der Prozess", "69327-0.txt", "utf-8",
     "39eb13c50d8a702a1093d2f336886bf17cf597bd538028504b0d81a24b46b15b"),
    # 31 more books of dark 19th- and early 20th-century prose (task 4.5),
    # approved by the user on 2026-11-18; all authors died before 1956.
    (76360, "E. T. A. Hoffmann: Die Elixiere des Teufels", "76360-0.txt", "utf-8",
     "81795a2b6b41851e4ff623dc6a377620df818a1476efcd8632d044c00b5988eb"),
    (6341, "E. T. A. Hoffmann: Nachtstücke", "6341-8.txt", "latin-1",
     "cc610e9c542983db10e9937876f807f1a6dcf1507a103cdd715e136feea62f88"),
    (17362, "E. T. A. Hoffmann: Der goldne Topf", "17362-8.txt", "latin-1",
     "cf020d519372dbc4ebcbb19a0ad4fe85225f4cfb6dbad36ffee618d578d76c30"),
    (8889, "Theodor Storm: Aquis submersus", "8889-0.txt", "utf-8",
     "0744d1b1c5711a058ecad6d2b4e9679847e3511bf105c3814863709494580765"),
    (8925, "Theodor Storm: Der Spiegel des Cyprianus", "8925-8.txt", "latin-1",
     "3381e7c3bfa2d4267b891faec1a31b83e1557d6942c0f8762569829f1b7a9353"),
    (8916, "Theodor Storm: Bulemanns Haus", "8916-8.txt", "latin-1",
     "d7c6c258256c385da58af52d051a287f0087c853d3c844a3e1f1aa670a9247d6"),
    (6651, "Theodor Storm: Immensee", "6651-0.txt", "utf-8",
     "b48e4a1b924ab3fe9962394be3dd2c92a054504005201d35e54332cca591070f"),
    (45798, "Annette von Droste-Hülshoff: Die Judenbuche", "45798-0.txt", "utf-8",
     "b34bac43b904ea9afb8f083885420261c8b1653d9bef7b61dc314110372024d0"),
    (6640, "Wilhelm Hauff: Märchen-Almanach 1828", "6640-0.txt", "utf-8",
     "440f6d6ebfa9924121e739d4bea8f41548be2252082533b9059c38d1866de5ce"),
    (6638, "Wilhelm Hauff: Märchen-Almanach 1826", "6638-0.txt", "utf-8",
     "971157d02a58ceaf4883381a1abcdf97811e1fa2236e4bef12e1513e5f29d4bb"),
    (6892, "Wilhelm Hauff: Mitteilungen aus den Memoiren des Satan", "6892-8.txt", "latin-1",
     "5af79d3dc02a667c33a256ed51d53411b4416774de36d3792e2ecf9f55ca975c"),
    (50480, "Ludwig Tieck: Phantasus 1", "50480-0.txt", "utf-8",
     "d163d740f00af19c96c20ca1012fe20a09c40ebdbdbd213224f8bf79ec8731b1"),
    (50714, "Ludwig Tieck: Novellen 7", "50714-0.txt", "utf-8",
     "a8fd891cfab2b2208d606d027268ecf133709fb7410dac0a3284a9f76dc69813"),
    (50833, "Achim von Arnim: Die Majoratsherren", "50833-0.txt", "utf-8",
     "da82dd6898bdd66f0b049f37916b2d2d64735f0ad0c19de7c8083b3cf945d9f7"),
    (2190, "Achim von Arnim: Isabella von Ägypten", "2190-8.txt", "latin-1",
     "516035a18a7e9f54b639f45f98e8bc4430cbfd57f845c4545af311aa761aa147"),
    (6645, "Heinrich von Kleist: Ausgewählte Schriften", "6645-8.txt", "latin-1",
     "8aef43991e10b6abb0c8766db8e6872bb45224f9b459fc66455c9f989133e788"),
    (31538, "Adelbert von Chamisso: Peter Schlemihls wundersame Geschichte", "31538-8.txt", "latin-1",
     "770c84dbfd3907a4ab1ea3ad824b65919bd0ddbc1c3da9719e437f9c48618c29"),
    (4503, "Clemens Brentano: Vom braven Kasperl und dem schönen Annerl", "4503-0.txt", "utf-8",
     "58e95fd8d47789cebcf675d287a20f98f602bf57ef33f1daa1263f4976feaf75"),
    (51476, "Gustav Meyrink: Der Golem", "51476-0.txt", "utf-8",
     "2b5ab93ad74435dbe24dc3cde1e9039ec7934b00db2b7777e2716932a1c5f1a9"),
    (45936, "Gustav Meyrink: Das grüne Gesicht", "45936-0.txt", "utf-8",
     "b4c888e3e8765e922636696d2dda98930f50d43eab81333338c81fbdd2828d0d"),
    (79578, "Gustav Meyrink: Der weiße Dominikaner", "79578-0.txt", "utf-8",
     "824fa5409abd7f1b4b2aafb51badc3c6ff44fd0f981db7e56e0cc405662b7de2"),
    (32014, "Gustav Meyrink: Fledermäuse", "32014-8.txt", "latin-1",
     "d56541fc124c7c35de7795c308056ffe095966a77f8b16da501dbe51cb520b23"),
    (31164, "Gustav Meyrink: Der violette Tod", "31164-0.txt", "utf-8",
     "1bcf2a4d7f213fc50fb0917b43414c7b39b414426a047e54b329c9bdcaf13749"),
    (41320, "Paul Leppin: Severins Gang in die Finsternis", "41320-0.txt", "utf-8",
     "5447894e607188d037ba58174a8c9d2cffe89dc5a466282860fc985b41991c7e"),
    (26686, "Theodor Fontane: Unterm Birnbaum", "26686-0.txt", "utf-8",
     "edfe0671960fe2b98430841be9da03a9da835e7e22347f9d51c471c3b9e68965"),
    (29376, "Gerhart Hauptmann: Bahnwärter Thiel", "29376-8.txt", "latin-1",
     "ec8fcdeca922d3817c7e5d63e06b0d866ca43520e8f648edad225401152d52e4"),
    (17142, "Arthur Schnitzler: Die griechische Tänzerin", "17142-0.txt", "utf-8",
     "f93c5b4c7e016b479e59e6ffb52573ce347f7aa4dc296aece93ab3f0277be020"),
    (25204, "Arthur Schnitzler: Der Mörder", "25204-0.txt", "utf-8",
     "4aa4d99205a2788ba78e695a8facffb6655812707070f01b8916178f1675b188"),
    (36275, "Wilhelm Jensen: Gradiva", "36275-0.txt", "utf-8",
     "82d6e15afc293ffcbbfe6e5b41273c9fb859d3ebf4a867ea3dd6c88145d75aea"),
    (46411, "Wilhelm Raabe: Die Chronik der Sperlingsgasse", "46411-0.txt", "utf-8",
     "b7a86e2cbfe08cc0a6e35bfb83bd0382027cc1484d05b03847ee87972ce41ae6"),
    (76558, "Brüder Grimm: Deutsche Sagen", "76558-0.txt", "utf-8",
     "3e1a6a14d770969c68b1b655c61057a3f1a3fa9d087c7152d7363534bf63ec29"),
]

ALPHABET = set(vocab_module.TEXT_ALPHABET)
REPLACE = {";": ",", ":": ",", "–": "-", "—": "-", "’": "'", "´": "'"}
# Newer files mark the text with "*** START OF"; older ones end their header
# with "*END*THE SMALL PRINT!".
START = re.compile(r"^(\*\*\* ?START OF|\*END\*THE SMALL PRINT).*$", re.MULTILINE)
END = re.compile(r"^(\*\*\* ?END OF|End of (the )?Project Gutenberg).*$", re.MULTILINE)


def url(ebook, file):
    digits = str(ebook)
    return f"{MIRROR}/{'/'.join(digits[:-1])}/{ebook}/{file}"


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fetch():
    RAW.mkdir(parents=True, exist_ok=True)
    for ebook, title, file, _, digest in SOURCES:
        path = RAW / file
        if not path.exists():
            print(f"download {title} ({url(ebook, file)})")
            with urllib.request.urlopen(url(ebook, file)) as response:
                path.write_bytes(response.read())
        if sha256(path) != digest:
            raise SystemExit(f"checksum mismatch: {path}")
        print(f"ok  {file}  {title}")


def body(text):
    """The book without the Project Gutenberg header and licence."""
    start = START.search(text)
    end = END.search(text, start.end() if start else 0)
    return text[start.end() if start else 0:end.start() if end else len(text)]


def map_char(c):
    c = REPLACE.get(c, c)
    if c in ALPHABET:
        return c
    base = unicodedata.normalize("NFD", c)[0]
    return base if base in ALPHABET else ""


def to_alphabet(text):
    """Upper case (ß becomes SS), alphabet mapping, single spaces."""
    text = re.sub(r"\s+", " ", text.upper())
    text = "".join(map_char(c) for c in text)
    text = re.sub(r" +", " ", text)
    text = re.sub(r" ([.,!?])", r"\1", text)
    return text.strip()


def sentences(text):
    """Mapped sentences of one book, paragraphs about the edition left out."""
    out = []
    for paragraph in re.split(r"\n\s*\n", text):
        if re.search(r"gutenberg|produced by|prepared by|transcriber|e-?text|ebook|@", paragraph, re.I):
            continue
        mapped = to_alphabet(paragraph)
        for sentence in re.split(r"(?<=[.!?])\s+", mapped):
            if sum(ch.isalpha() for ch in sentence) >= 2:
                out.append(sentence)
    return out


def prepare():
    lines = []
    for _, title, file, encoding, digest in SOURCES:
        path = RAW / file
        if not path.exists() or sha256(path) != digest:
            raise SystemExit(f"missing or changed: {path} (run fetch)")
        text = path.read_bytes().decode(encoding).replace("\r\n", "\n")
        book = sentences(body(text))
        print(f"{title}: {len(book)} sentences, {sum(len(s) for s in book)} characters")
        lines += book
    TEXT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {TEXT}: {len(lines)} sentences, {sum(len(s) for s in lines)} characters")


def words():
    """Book words: the most frequent pretraining words outside the corpus."""
    import collections
    corpus = set(vocab_module.corpus_words())
    counts = collections.Counter()
    for line in TEXT.read_text(encoding="utf-8").splitlines():
        counts.update(p for p in vocab_module.split_body(line)
                      if p not in vocab_module.PUNCTUATION)
    # A few more than needed, so later corpus edits can take book words out.
    room = vocab_module.MAX_WORDS - len(corpus) + 200
    # A book word needs a letter; a bare apostrophe, a lone digit run (chapter
    # numbers) or a quote mark stuck to one letter ("S'") is left out.
    usable = lambda w: re.fullmatch(r"'?[A-ZÄÖÜ][A-ZÄÖÜ']*", w) and not (
        len(w) <= 2 and w.endswith("'"))
    added = [w for w, _ in counts.most_common() if w not in corpus and usable(w)][:room]
    vocab_module.BOOK_WORDS.write_text("\n".join(added) + "\n", encoding="utf-8")
    chosen = set(vocab_module.vocab_words())
    added = [w for w in added if w in chosen]
    total = sum(counts.values())
    covered = sum(n for w, n in counts.items() if w in chosen)
    lines = TEXT.read_text(encoding="utf-8").splitlines()
    whole = sum(1 for l in lines if all(p in chosen or p in vocab_module.PUNCTUATION
                                        for p in vocab_module.split_body(l)))
    print(f"wrote {vocab_module.BOOK_WORDS}: {len(added)} book words, {len(chosen)} in all; "
          f"they cover {100 * covered / total:.1f} % of the {total} book words, "
          f"{whole} of {len(lines)} sentences entirely; least frequent added word occurs "
          f"{counts[added[-1]] if added else 0} times")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("command", choices=["fetch", "prepare", "words"])
    args = parser.parse_args()
    {"fetch": fetch, "prepare": prepare, "words": words}[args.command]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
