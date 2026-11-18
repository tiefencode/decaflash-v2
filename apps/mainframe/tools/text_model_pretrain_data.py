#!/usr/bin/env python3
"""General German text for pretraining the text model (add-text-model, design.md Decision 14).

The corpus of reaction texts is too small to teach a character model German
spelling and grammar, so the model first learns general German from
public-domain books of Project Gutenberg and is then fine-tuned on the corpus.
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
# 2026-11-18). All four books are public domain in the USA, and their authors
# died before 1956, so they are public domain in Germany too. Approved by the
# user on 2026-11-18.
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
]

ALPHABET = set(vocab_module.TEXT_ALPHABET)
REPLACE = {";": ",", ":": ",", "–": "-", "—": "-", "’": "'", "´": "'"}
START = re.compile(r"^\*\*\* ?START OF.*$", re.MULTILINE)
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
        if re.search(r"gutenberg|produced by|transcriber|e-text|ebook", paragraph, re.I):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("command", choices=["fetch", "prepare"])
    args = parser.parse_args()
    fetch() if args.command == "fetch" else prepare()
    return 0


if __name__ == "__main__":
    sys.exit(main())
