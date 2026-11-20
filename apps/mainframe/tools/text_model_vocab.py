#!/usr/bin/env python3
"""Vocabulary of the on-device text model (add-text-model, design.md Decision 6).

One token per word, punctuation mark or curated emoji. The words are every
word of the corpus (apps/mainframe/text_model/corpus/) plus the most frequent
words of the pretraining text, listed in apps/mainframe/text_model/book_words.txt
(written by text_model_pretrain_data.py words, most frequent first), MAX_WORDS
in all: the corpus words, then book words in order until the total is reached. The curated
emoji come from the panel's glyph table (apps/mainframe/src/panel_glyphs.cpp),
so the model can only emit listed words and glyphs the panel draws.

Token ids:
  output classes 0 .. kOutputTokens-1: end, punctuation, words, curated emoji
  input tokens: the output tokens with the same ids (identity output-to-input
  table), then start, 12 event tokens and 15 mood tokens (5 values x 3 levels).

A text is "<emoji> <body>". The body is written from its pieces with one space
before every word, none before . , ! ? and none on either side of -
(join_body); split_body is the inverse for canonical bodies. The body has at
most MAX_TEXT_CHARACTERS characters (design.md Decision 7).

This module is the single source of the table: the firmware header
apps/mainframe/src/slm/text_model_vocab.h is generated from it, and the host
tools import build_vocab(). The header also carries the writer's word sets
(improve-text-model, step 2) as bitmaps over the output rows: per event the
words of that event's corpus lines plus the end token and punctuation (word
mask per event), and the words that occur exactly once in the whole corpus
(demoted once-only words).

A closed word list (--words in text_model.py) replaces the corpus and book words:
the verse text model reads every word from a file such as
apps/mainframe/text_model/verse_words.txt (written by text_model_verse_data.py
vocab). The firmware header always comes from the classic words.

Usage:
  text_model_vocab.py            write the firmware header
  text_model_vocab.py --check    exit 1 if the header is not up to date
"""

import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
GLYPHS_CPP = REPO / "apps/mainframe/src/panel_glyphs.cpp"
CORPUS = REPO / "apps/mainframe/text_model/corpus"
BOOK_WORDS = REPO / "apps/mainframe/text_model/book_words.txt"
VERSE_CORPUS = REPO / "apps/mainframe/text_model/verse_corpus"
VERSE_WORDS = REPO / "apps/mainframe/text_model/verse_words.txt"
VERSE_BREAK = " / "  # between verse lines in the verse corpus
MAX_WORDS = 8000  # corpus and book words together (design.md Decision 6)
HEADER = REPO / "apps/mainframe/src/slm/text_model_vocab.h"

# Capital letters, umlauts as own glyphs, no lower case and no sharp s (the
# panel draws it as B). Punctuation: today's texts use . , ! ? - and
# design.md Decision 6 adds the apostrophe.
TEXT_ALPHABET = (
    [" "]
    + [chr(c) for c in range(ord("A"), ord("Z") + 1)]
    + ["Ä", "Ö", "Ü"]
    + [str(d) for d in range(10)]
    + [".", ",", "!", "?", "-", "'"]
)

# Punctuation tokens; the apostrophe belongs to words (GEHT'S).
PUNCTUATION = [".", ",", "!", "?", "-"]
HYPHEN = "-"
WORD_CHARACTERS = set(c for c in TEXT_ALPHABET if c not in PUNCTUATION and c != " ")
MAX_TEXT_CHARACTERS = 47  # body characters after the emoji (design.md Decision 7)
_PIECE = re.compile(r"[A-ZÄÖÜ0-9']+|[.,!?-]| |.", re.S)

# Order matches TextModelEvent in apps/mainframe/src/reaction_texts.h.
EVENTS = [
    "MOVE", "TAP", "DOUBLE_TAP", "MULTI_TAP", "IMPACT", "ROTATE", "REPOSITION",
    "SHAKE", "ARRIVED_IN_FRONT", "WALKED_PAST", "LEFT", "STANDS_FAR",
]
# Order matches TextModelRequest::mood.
MOOD_VALUES = ["ENERGY", "ANNOYANCE", "ATTENTION", "LONELINESS", "DEPRESSION"]
MOOD_LEVELS = ["LOW", "MIDDLE", "HIGH"]

LETTER_GLYPHS = {"Aumlaut", "Oumlaut", "Uumlaut", "SharpS"}


def curated_emoji():
    """(utf8, PanelGlyph name) in table order, letters left out."""
    source = GLYPHS_CPP.read_text(encoding="utf-8")
    table = source[source.index("kGlyphs[]"):]
    table = table[:table.index("};")]
    pairs = re.findall(r'\{u8"([^"]+)",\s*PanelGlyph::(\w+)\}', table)
    return [(utf8, name) for utf8, name in pairs if name not in LETTER_GLYPHS]


def split_body(body):
    """Body text -> pieces (words and punctuation). ValueError on a character
    outside the text alphabet; spaces only separate pieces."""
    pieces = []
    for match in _PIECE.findall(body):
        if match == " ":
            continue
        if len(match) == 1 and match not in WORD_CHARACTERS and match not in PUNCTUATION:
            raise ValueError(f"character not in the text alphabet: {match!r} in {body!r}")
        pieces.append(match)
    return pieces


def join_body(pieces):
    """Pieces -> body text with the spacing rule of the module docstring."""
    out = ""
    for index, piece in enumerate(pieces):
        word = piece not in PUNCTUATION
        if index and word and pieces[index - 1] != HYPHEN:
            out += " "
        out += piece
    return out


def cut(tokens, ids):
    """Output ids up to the hard length limit: the first piece (the emoji) and
    the pieces whose body stays within MAX_TEXT_CHARACTERS."""
    first_emoji = counts(tokens)["first_emoji"]
    out = []
    for index, i in enumerate(ids):
        body = [tokens[j]["text"] for j in out + [i] if j < first_emoji]
        if index and len(join_body(body)) > MAX_TEXT_CHARACTERS:
            break
        out.append(i)
    return out


def decode(tokens, ids):
    """Output ids -> '<emoji> <body>' (reading space after the emoji), cut at
    the hard length limit."""
    first_emoji = counts(tokens)["first_emoji"]
    ids = cut(tokens, ids)
    lead = ""
    if ids and ids[0] >= first_emoji:
        lead, ids = tokens[ids[0]]["text"] + " ", ids[1:]
    return lead + join_body([tokens[i]["text"] for i in ids])


def corpus_bodies(directory=CORPUS):
    """Body texts of every corpus line (the text after the leading emoji)."""
    emoji = sorted((u for u, _ in curated_emoji()), key=len, reverse=True)
    bodies = []
    for path in sorted(pathlib.Path(directory).glob("*.tsv")):
        for line in path.read_text(encoding="utf-8").splitlines():
            text = line.partition("\t")[2]
            if not text:
                continue
            lead = next((e for e in emoji if text.startswith(e)), "")
            bodies.append(text[len(lead):].strip())
    return bodies


def corpus_words(directory=CORPUS):
    """Sorted list of the distinct words of the corpus."""
    return sorted({p for body in corpus_bodies(directory) for p in split_body(body)
                   if p not in PUNCTUATION})


def verse_bodies(directory=VERSE_CORPUS):
    """Body texts of every verse corpus line (lines LEAD<TAB>FORM<TAB>TEXT), the
    verse line breaks and colons removed, so only words and . , ! ? - remain."""
    emoji = sorted((u for u, _ in curated_emoji()), key=len, reverse=True)
    bodies = []
    for path in sorted(pathlib.Path(directory).glob("*.tsv")):
        for line in path.read_text(encoding="utf-8").splitlines():
            fields = line.split("\t")
            if len(fields) < 3 or line.startswith("#"):
                continue
            text = fields[2]
            lead = next((e for e in emoji if text.startswith(e)), "")
            bodies.append(text[len(lead):].strip().replace(VERSE_BREAK, " ").replace(":", ""))
    return bodies


def verse_corpus_words(directory=VERSE_CORPUS):
    """Sorted list of the distinct words of the verse corpus."""
    return sorted({p for body in verse_bodies(directory) for p in split_body(body)
                   if p not in PUNCTUATION})


def closed_words(path=VERSE_WORDS):
    """Words of a closed word list: one per line, # starts a comment line."""
    return [w for w in pathlib.Path(path).read_text(encoding="utf-8").split("\n")
            if w and not w.startswith("#")]


def book_words(path=BOOK_WORDS):
    """Words from the pretraining text, one per line, most frequent first (empty
    if the file is missing)."""
    if not pathlib.Path(path).exists():
        return []
    return [w for w in pathlib.Path(path).read_text(encoding="utf-8").split("\n") if w]


def vocab_words(words_file=None):
    """Sorted list of every word of the vocabulary: corpus words and book words,
    or the words of a closed word list (words_file)."""
    if words_file:
        words = set(closed_words(words_file))
        for word in words:
            if any(c not in WORD_CHARACTERS for c in word):
                raise ValueError(f"word outside the text alphabet: {word!r}")
        return sorted(words)
    words = set(corpus_words())
    if len(words) > MAX_WORDS:
        raise ValueError(f"{len(words)} corpus words, more than MAX_WORDS ({MAX_WORDS})")
    # Book words fill the rest in order of frequency, so a corpus edit that adds
    # or removes words keeps the total at MAX_WORDS without a new book list.
    for word in book_words():
        if len(words) == MAX_WORDS:
            break
        words.add(word)
    for word in words:
        if any(c not in WORD_CHARACTERS for c in word):
            raise ValueError(f"word outside the text alphabet: {word!r}")
    return sorted(words)


def build_vocab(words_file=None):
    """List of dicts: id, text, kind (end/punct/word/emoji/start/event/mood), glyph.
    words_file: closed word list instead of corpus and book words."""
    tokens = [{"text": "<END>", "kind": "end", "glyph": None}]
    tokens += [{"text": c, "kind": "punct", "glyph": None} for c in PUNCTUATION]
    tokens += [{"text": w, "kind": "word", "glyph": None} for w in vocab_words(words_file)]
    tokens += [{"text": u, "kind": "emoji", "glyph": g} for u, g in curated_emoji()]
    tokens.append({"text": "<START>", "kind": "start", "glyph": None})
    tokens += [{"text": f"<{e}>", "kind": "event", "glyph": None} for e in EVENTS]
    tokens += [
        {"text": f"<{v}_{l}>", "kind": "mood", "glyph": None}
        for v in MOOD_VALUES for l in MOOD_LEVELS
    ]
    for index, token in enumerate(tokens):
        token["id"] = index
    return tokens


def counts(tokens):
    output = sum(1 for t in tokens if t["kind"] in ("end", "punct", "word", "emoji"))
    first = lambda kind: next(t["id"] for t in tokens if t["kind"] == kind)
    return {
        "output": output,
        "input": len(tokens),
        "first_text": first("punct"),
        "first_word": first("word"),
        "first_emoji": first("emoji"),
        "start": first("start"),
        "first_event": first("event"),
        "first_mood": first("mood"),
    }


def event_bodies(directory=CORPUS):
    """{event: [body]} from the event files (<event in lower case>.tsv)."""
    emoji = sorted((u for u, _ in curated_emoji()), key=len, reverse=True)
    out = {}
    for event in EVENTS:
        path = pathlib.Path(directory) / f"{event.lower()}.tsv"
        bodies = []
        if path.exists():
            for line in path.read_text(encoding="utf-8").splitlines():
                text = line.partition("\t")[2]
                if not text:
                    continue
                lead = next((e for e in emoji if text.startswith(e)), "")
                bodies.append(text[len(lead):].strip())
        out[event] = bodies
    return out


def word_sets(tokens):
    """(per-event allowed rows, once-only word rows) as sorted id lists."""
    ids = {t["text"]: t["id"] for t in tokens if t["kind"] == "word"}
    always = [t["id"] for t in tokens if t["kind"] in ("end", "punct")]
    events = []
    count = {}
    for event, bodies in event_bodies().items():
        words = {p for b in bodies for p in split_body(b) if p not in PUNCTUATION}
        events.append(sorted(always + [ids[w] for w in words if w in ids]))
        for b in bodies:
            for p in split_body(b):
                if p not in PUNCTUATION:
                    count[p] = count.get(p, 0) + 1
    once = sorted(ids[w] for w, c in count.items() if c == 1 and w in ids)
    return events, once


def bitmap(rows, words):
    bits = [0] * words
    for r in rows:
        bits[r >> 5] |= 1 << (r & 31)
    return bits


def render_bitmap(bits, indent="  "):
    out = []
    for i in range(0, len(bits), 8):
        out.append(indent + ", ".join(f"0x{b:08x}u" for b in bits[i:i + 8]) + ",")
    return out


def c_string(text):
    out = []
    for ch in text:
        if ch in ('"', "\\"):
            out.append("\\" + ch)
        elif 0x20 <= ord(ch) < 0x7F:
            out.append(ch)
        else:
            out.append("".join(f"\\x{b:02x}" for b in ch.encode("utf-8")))
    # Split after hex escapes so a following hex digit is not swallowed.
    return '"' + re.sub(r'(\\x[0-9a-f]{2})(?=[0-9a-fA-F])', r'\1""', "".join(out)) + '"'


def render_header(tokens):
    n = counts(tokens)
    kinds = {"end": "End", "punct": "Punct", "word": "Word", "emoji": "Emoji",
             "start": "Start", "event": "Event", "mood": "Mood"}
    hyphen = next(t["id"] for t in tokens if t["text"] == HYPHEN)
    lines = [
        "// Generated by apps/mainframe/tools/text_model_vocab.py. Do not edit.",
        "// Vocabulary of the on-device text model (add-text-model, design.md Decision 6).",
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        '#include "panel_glyphs.h"',
        "",
        "namespace decaflash::mainframe::text_model {",
        "",
        "enum class TokenKind : uint8_t { End, Punct, Word, Emoji, Start, Event, Mood };",
        "",
        "struct Token {",
        "  const char* utf8;",
        "  TokenKind kind;",
        "  PanelGlyph glyph;",
        "};",
        "",
        f"constexpr uint16_t kOutputTokens = {n['output']};",
        f"constexpr uint16_t kInputTokens = {n['input']};",
        "constexpr uint16_t kEndToken = 0;",
        "// Text pieces: punctuation from kFirstTextToken, words from kFirstWordToken",
        "// (sorted by their UTF-8 bytes), emoji from kFirstEmojiToken.",
        f"constexpr uint16_t kFirstTextToken = {n['first_text']};",
        f"constexpr uint16_t kFirstWordToken = {n['first_word']};",
        f"constexpr uint16_t kHyphenToken = {hyphen};",
        f"constexpr uint16_t kFirstEmojiToken = {n['first_emoji']};",
        f"constexpr uint16_t kStartToken = {n['start']};",
        f"constexpr uint16_t kFirstEventToken = {n['first_event']};",
        "// Mood token: kFirstMoodToken + 3 * value + level (value in",
        "// TextModelRequest::mood order, level low / middle / high).",
        f"constexpr uint16_t kFirstMoodToken = {n['first_mood']};",
        "// Body characters after the emoji (design.md Decision 7).",
        f"constexpr uint16_t kMaxTextCharacters = {MAX_TEXT_CHARACTERS};",
        "",
        "inline constexpr Token kTokens[kInputTokens] = {",
    ]
    for t in tokens:
        glyph = f"PanelGlyph::{t['glyph']}" if t["glyph"] else "PanelGlyph::None"
        comment = f"  // {t['id']}" + (f" {t['text']}" if t["kind"] == "emoji" else "")
        if t["kind"] == "word" and t["id"] % 50:
            comment = ""
        lines.append(f"  {{{c_string(t['text'])}, TokenKind::{kinds[t['kind']]}, {glyph}}},"
                     + comment)
    bit_words = (n["output"] + 31) // 32
    events, once = word_sets(tokens)
    lines += [
        "};",
        "",
        "// Bitmaps over the output rows (bit r of word r / 32).",
        f"constexpr int kOutputBitWords = {bit_words};",
        "// Word mask per event (TextModelEvent order): the words of the event's",
        "// corpus lines, the end token and punctuation.",
        f"inline constexpr uint32_t kEventRows[{len(EVENTS)}][kOutputBitWords] = {{",
    ]
    for event, rows in zip(EVENTS, events):
        lines.append(f"  {{  // {event}, {len(rows)} rows")
        lines += render_bitmap(bitmap(rows, bit_words), "    ")
        lines.append("  },")
    lines += [
        "};",
        f"// Words that occur exactly once in the corpus ({len(once)} words).",
        "inline constexpr uint32_t kOnceWordRows[kOutputBitWords] = {",
    ]
    lines += render_bitmap(bitmap(once, bit_words))
    lines += [
        "};",
        "",
        "// True if a space goes before text piece `id` that follows text piece",
        "// `previous` in a body (kEndToken: `id` is the first piece of the body).",
        "inline constexpr bool spaceBefore(uint16_t previous, uint16_t id) {",
        "  return previous != kEndToken && previous != kHyphenToken &&",
        "         kTokens[id].kind == TokenKind::Word;",
        "}",
        "",
        "}  // namespace decaflash::mainframe::text_model",
        "",
    ]
    return "\n".join(lines)


def main():
    header = render_header(build_vocab())
    if "--check" in sys.argv[1:]:
        current = HEADER.read_text(encoding="utf-8") if HEADER.exists() else ""
        if current != header:
            print(f"{HEADER.relative_to(REPO)} is not up to date; run {sys.argv[0]}")
            return 1
        return 0
    HEADER.write_text(header, encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
