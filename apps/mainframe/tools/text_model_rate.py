#!/usr/bin/env python3
"""Texts for the user's rating (add-text-model, tasks 6.1 and 6.2).

For every case in apps/mainframe/text_model/rating_cases.tsv (event, tab, five
levels) the host kernel writes one greedy text from the exported image
(text_model_generate). Each text then goes through the form check, with the
corpus as training texts for the copy check.

Usage:
  text_model_rate.py <model.bin> <text_model_generate> <text_model_form_check> [--out texts.tsv]
Prints one line per case: number, event, levels, form result, text.
"""

import argparse
import pathlib
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_corpus as corpus  # noqa: E402
import text_model_vocab as vocab_module  # noqa: E402

CASES = vocab_module.REPO / "apps/mainframe/text_model/rating_cases.tsv"
MAX_PIECES = 48


def condition(tokens, event, levels):
    n = vocab_module.counts(tokens)
    ids = [n["start"], n["first_event"] + vocab_module.EVENTS.index(event)]
    ids += [n["first_mood"] + 3 * value + level for value, level in enumerate(levels)]
    return ids


def decode(tokens, ids):
    first_emoji = vocab_module.counts(tokens)["first_emoji"]
    text = "".join(tokens[i]["text"] for i in ids)
    if ids and ids[0] >= first_emoji:
        emoji = tokens[ids[0]]["text"]
        return emoji + " " + text[len(emoji):]
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("image")
    parser.add_argument("generator")
    parser.add_argument("form_check")
    parser.add_argument("--out")
    args = parser.parse_args()

    tokens = vocab_module.build_vocab()
    cases = []
    for line in CASES.read_text(encoding="utf-8").splitlines():
        if line.strip():
            event, levels = line.split("\t")
            cases.append((event, levels, corpus.parse_levels(levels)))

    texts = []
    for event, _, levels in cases:
        run = subprocess.run([args.generator, args.image, str(MAX_PIECES)]
                             + [str(i) for i in condition(tokens, event, levels)],
                             capture_output=True, text=True, check=True)
        texts.append(decode(tokens, [int(x) for x in run.stdout.split()]))

    with tempfile.TemporaryDirectory() as tmp:
        texts_path = pathlib.Path(tmp) / "texts.txt"
        texts_path.write_text("\n".join(t if t else "(empty)" for t in texts) + "\n",
                              encoding="utf-8")
        training_path = pathlib.Path(tmp) / "training.txt"
        training_path.write_text("\n".join(t for _, _, t in corpus.read_corpus()) + "\n",
                                 encoding="utf-8")
        run = subprocess.run([args.form_check, "--training", str(training_path),
                              str(texts_path)], capture_output=True, text=True)
    failures = {}
    for line in run.stdout.splitlines():
        if line.startswith("FAIL line "):
            number = int(line.split()[2])
            failures[number] = line.split("(", 1)[1].split(")", 1)[0]

    rows = []
    for number, ((event, levels, _), text) in enumerate(zip(cases, texts), 1):
        result = failures.get(number, "ok")
        rows.append(f"{number}\t{event}\t{levels}\t{result}\t{text}")
        print(rows[-1])
    passed = sum(1 for n in range(1, len(cases) + 1) if n not in failures)
    print(f"{passed} of {len(cases)} texts pass the form check")
    if args.out:
        pathlib.Path(args.out).write_text("\n".join(rows) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
