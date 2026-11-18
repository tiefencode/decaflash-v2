#!/usr/bin/env python3
"""Random cases with per-token confidence on the device's 4-bit path, for the
confidence gate measurement (change add-text-model-confidence-gate, second
measurement with the product's sampling schedule). Needs no torch.

  text_model_gate_cases.py <model.bin> --measure <text_model_measure>
      --text-bank <text_bank_try> --form-check <text_model_form_check>
      [--cases 1200] [--seed 1119] --out <records.jsonl>

Each case: one of the twelve events, five mood values 0-100 mapped to levels
as reaction_texts.cpp does, a random 32-bit seed. The text is written by the
firmware's writer (text_model_measure); each record adds the text bank line
for the same event and values (first line text_bank_try prints), the form
check result and whether the text is a verbatim corpus line.
"""

import argparse
import json
import pathlib
import random
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_corpus as corpus  # noqa: E402
import text_model_vocab as vocab_module  # noqa: E402


def level(value):
    return 0 if value <= 33 else 1 if value <= 66 else 2


def form_results(form_check, texts, training):
    """Form check result per text ("ok" or the failure), as text_model_confidence.py."""
    with tempfile.TemporaryDirectory() as tmp:
        texts_path = pathlib.Path(tmp) / "texts.txt"
        texts_path.write_text("\n".join(t if t else "(empty)" for t in texts) + "\n",
                              encoding="utf-8")
        training_path = pathlib.Path(tmp) / "training.txt"
        training_path.write_text("\n".join(training) + "\n", encoding="utf-8")
        run = subprocess.run([form_check, "--training", str(training_path),
                              str(texts_path)], capture_output=True, text=True)
    results = ["ok"] * len(texts)
    for line in run.stdout.splitlines():
        if line.startswith("FAIL line "):
            results[int(line.split()[2]) - 1] = line.split("(", 1)[1].split(")", 1)[0]
    return results


def text_bank_line(tool, event_index, values):
    out = subprocess.run([tool, str(event_index + 1)] + [str(v) for v in values],
                         capture_output=True, text=True).stdout.splitlines()
    lines = [l.strip() for l in out[1:] if l.strip() and not l.strip().startswith("Keine")]
    return lines[0] if lines else ""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("model")
    parser.add_argument("--measure", required=True)
    parser.add_argument("--text-bank", required=True)
    parser.add_argument("--form-check", required=True)
    parser.add_argument("--cases", type=int, default=1200)
    parser.add_argument("--seed", type=int, default=1119)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()
    draw = random.Random(args.seed)
    cases = []
    for _ in range(args.cases):
        event = draw.randrange(len(vocab_module.EVENTS))
        values = [draw.randint(0, 100) for _ in range(5)]
        cases.append((event, values, draw.getrandbits(32)))
    stdin = "".join(f"{e} {' '.join(str(level(v)) for v in vs)} {s}\n" for e, vs, s in cases)
    out = subprocess.run([args.measure, args.model], input=stdin, capture_output=True,
                         text=True, check=True).stdout.splitlines()
    copies = set(vocab_module.corpus_bodies())
    training = [text for _, _, text in corpus.read_corpus()]
    records = []
    for number, ((event, values, _), line) in enumerate(zip(cases, out), 1):
        record = json.loads(line)
        body = record["text"].split(" ", 1)[1] if " " in record["text"] else ""
        record.update({"id": f"d{number}", "event": vocab_module.EVENTS[event],
                       "values": values, "copy": body in copies,
                       "bank": text_bank_line(args.text_bank, event, values)})
        records.append(record)
    for record, form in zip(records, form_results(args.form_check,
                                                  [r["text"] for r in records], training)):
        record["form"] = form
    pathlib.Path(args.out).write_text(
        "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in records), encoding="utf-8")
    print(f"{len(records)} texts, {sum(r['form'] == 'ok' for r in records)} pass the form "
          f"check, {sum(r['copy'] for r in records)} verbatim corpus lines")
    return 0


if __name__ == "__main__":
    sys.exit(main())
