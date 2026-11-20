"""Count the event word fields in the corpus and in the verse pretraining text.

A word field is a list of anchor words per event: content words that say or
evoke the event, occur in the pretraining text and recur in the event's corpus
lines. The pretraining text has no event tokens, so these shared words are the
only bridge between an event and what the model learned in pretraining.

  text_model_word_fields.py [--fields FILE] [--text FILE] [--examples N] [--words]

Reads apps/mainframe/text_model/word_fields.tsv (lines `EVENTS<TAB>WORD`, whole
word forms after the model's alphabet mapping, `#` starts a comment). EVENTS is
one event or a comma-separated list: a word listed for several events belongs
to their shared base field, the other words distinguish the events. Prints per
event:
  - pretraining coverage: share of pretraining sentences with a field word,
  - corpus coverage: share of the event's lines in the old corpus and in the
    verse corpus with a field word,
  - foreign: share of other events' corpus lines with a word of this field,
  - own words: share of the event's old corpus lines with a word that belongs
    to this event only (only differs from corpus coverage for shared fields),
  - ambiguity: share of the pretraining sentences hit by this field that also
    contain a word of another event's field that this event does not share.
--words adds one row per word (pretraining count, own old corpus lines, number
of other events whose old corpus lines contain it); --examples N prints the N
best pretraining sentences per event (at most 160 characters; most distinct words of the event's own field,
then of the whole field, then closest to 60 characters).
The pretraining text comes from text_model_verse_data.py prepare and is not
versioned; pass --text when it lives in another checkout.
"""
import argparse
import collections
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_corpus as corpus  # noqa: E402
import text_model_vocab as vocab  # noqa: E402

FIELDS = vocab.REPO / "apps/mainframe/text_model/word_fields.tsv"
TEXT = vocab.REPO / "models/text-model/verse/text.txt"
VERSE_CORPUS = vocab.REPO / "apps/mainframe/text_model/verse_corpus"
WORD = re.compile(r"[A-ZÄÖÜ']+")
EXAMPLE_LENGTH = 160  # longer sentences are mostly word lists or prose periods


def read_fields(path):
    fields = {event: set() for event in vocab.EVENTS}
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        events, word = line.split("\t")
        if not WORD.fullmatch(word) or any(e not in fields for e in events.split(",")):
            raise ValueError(f"{path.name}:{number}: bad line")
        for event in events.split(","):
            fields[event].add(word)
    return fields


def words(text):
    return set(WORD.findall(text.upper()))


def old_corpus():
    lines = collections.defaultdict(list)
    for event, _, text in corpus.read_corpus():
        lines[event].append(words(text))
    return lines


def verse_corpus():
    lines = collections.defaultdict(list)
    for event in vocab.EVENTS:
        path = VERSE_CORPUS / corpus.file_name(event)
        if path.exists():
            for line in path.read_text(encoding="utf-8").splitlines():
                if line.strip():
                    lines[event].append(words(line.split("\t")[-1]))
    return lines


def share(lines, field):
    return sum(bool(ws & field) for ws in lines) / len(lines) if lines else 0.0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fields", type=pathlib.Path, default=FIELDS)
    parser.add_argument("--text", type=pathlib.Path, default=TEXT)
    parser.add_argument("--examples", type=int, default=0)
    parser.add_argument("--words", action="store_true")
    args = parser.parse_args()

    fields = read_fields(args.fields)
    sentences = args.text.read_text(encoding="utf-8").splitlines()
    pre = [words(s) for s in sentences]
    old, verse = old_corpus(), verse_corpus()
    union = {e: set().union(*(f for o, f in fields.items() if o != e)) - fields[e] for e in fields}
    shared = {e: f & set().union(*(g for o, g in fields.items() if o != e)) for e, f in fields.items()}

    print("| Ereignis | Wörter | Vortraining | alter Korpus | eigene Wörter | Vers-Korpus | fremde Zeilen | mehrdeutig |")
    print("|---|---|---|---|---|---|---|---|")
    for event, field in fields.items():
        hits = [ws for ws in pre if ws & field]
        ambiguous = sum(bool(ws & union[event]) for ws in hits)
        foreign = [ws for e, ls in old.items() if e != event for ws in ls]
        print(f"| {event.lower()} | {len(field)} | {100 * len(hits) / len(pre):.1f} % "
              f"| {100 * share(old[event], field):.0f} % | {100 * share(old[event], field - shared[event]):.0f} % "
              f"| {100 * share(verse[event], field):.0f} % "
              f"| {100 * share(foreign, field):.1f} % "
              f"| {100 * ambiguous / len(hits) if hits else 0:.0f} % |")

    if args.words:
        count = collections.Counter(w for s in sentences for w in WORD.findall(s))
        print("\n| Ereignis | Wort | Vortraining | eigene Korpuszeilen | andere Ereignisse |")
        print("|---|---|---|---|---|")
        for event, field in fields.items():
            for word in sorted(field, key=lambda w: -count[w]):
                own = sum(word in ws for ws in old[event])
                others = sum(any(word in ws for ws in ls) for e, ls in old.items() if e != event)
                print(f"| {event.lower()} | {word} | {count[word]} | {own} | {others} |")

    for event, field in fields.items() if args.examples else ():
        own = field - shared[event]
        ranked = sorted((i for i, ws in enumerate(pre) if ws & field and len(sentences[i]) <= EXAMPLE_LENGTH),
                        key=lambda i: (-len(pre[i] & own), -len(pre[i] & field), abs(len(sentences[i]) - 60)))
        print(f"\n{event.lower()}:")
        for i in ranked[:args.examples]:
            print(f"  {sentences[i]}")


if __name__ == "__main__":
    main()
