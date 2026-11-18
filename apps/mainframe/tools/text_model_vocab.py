#!/usr/bin/env python3
"""Vocabulary of the on-device text model (add-text-model, design.md Decision 6).

One token per word of the corpus, punctuation mark or curated emoji. The words
are read from the corpus (apps/mainframe/text_model/corpus/), the curated emoji
from the panel's glyph table (apps/mainframe/src/panel_glyphs.cpp), so the
model can only emit corpus words and glyphs the panel draws.

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
tools import build_vocab().

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


def build_vocab():
    """List of dicts: id, text, kind (end/punct/word/emoji/start/event/mood), glyph."""
    tokens = [{"text": "<END>", "kind": "end", "glyph": None}]
    tokens += [{"text": c, "kind": "punct", "glyph": None} for c in PUNCTUATION]
    tokens += [{"text": w, "kind": "word", "glyph": None} for w in corpus_words()]
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
