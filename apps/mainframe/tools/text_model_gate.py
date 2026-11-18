#!/usr/bin/env python3
"""Per-token confidence of the text model, for a confidence gate (evaluation
before the change add-text-model-confidence-gate). Run it with the Python of
/.venv-text-model/.

  text_model_gate.py measure <checkpoint.pt> --text-bank <text_bank_try>
                     --form-check <text_model_form_check> [--cases 1000]
                     [--seed 1118] --out <records.jsonl>
      Draws random cases (one of the twelve events, five mood values 0-100,
      mapped to levels as reaction_texts.cpp does), samples one text per case
      from the 4-bit twin with the product's sampling (temperature 0.8, top-k
      8) and writes one JSON record per case: the text, its text bank line
      (first line text_bank_try prints), the form check result, whether it is
      a verbatim corpus line, and for every token (emoji, words, punctuation,
      end) the sampled log-probability, the raw log-probability (temperature
      1, no top-k), the entropy of the sampling distribution and the margin
      between its two best tokens.

  text_model_gate.py score <checkpoint.pt> <texts.tsv> --out <records.jsonl>
      The same per-token values for given texts (event, levels, text, mark),
      read back by teacher forcing, for texts that were rated earlier.

  text_model_gate.py analyse <records.jsonl>... [--marks marks.tsv]
      Joins the marks (record id, 1 usable / 0 not) to the records and prints
      AUC per confidence measure after the emoji plus 1, 2, 3 tokens and for
      the whole text, with a bootstrap interval, and a threshold table.

Build the two helpers as their own headers describe (text_bank_try.cpp,
text_model_form_check.cpp).
"""

import argparse
import json
import math
import pathlib
import random
import subprocess
import sys

import torch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model  # noqa: E402
import text_model_confidence as confidence  # noqa: E402
import text_model_corpus as corpus  # noqa: E402
import text_model_vocab as vocab_module  # noqa: E402


def level(value):
    """Mood value 0-100 -> 0 low, 1 middle, 2 high (reaction_texts.cpp)."""
    return 0 if value <= 33 else 1 if value <= 66 else 2


@torch.no_grad()
def step_values(logits_row, step, first_emoji, temperature, top_k):
    """Sampling distribution of one step: (indices, log-probs, raw log-probs
    over the whole masked row)."""
    row = logits_row.clone()
    if step == 0:
        row[:first_emoji] = float("-inf")
    else:
        row[first_emoji:] = float("-inf")
    raw = torch.log_softmax(row, dim=-1)
    values, indices = torch.topk(row, top_k)
    sampled = torch.log_softmax(values / temperature, dim=-1)
    return indices, sampled, raw


def token_record(indices, sampled, raw, target):
    hit = (indices == target).nonzero()
    probs = sampled.exp()
    return {"id": int(target),
            "lp": float(sampled[int(hit[0])]) if len(hit) else float("-inf"),
            "raw": float(raw[target]),
            "entropy": float(-(probs * sampled).sum()),
            "margin": float(sampled[0] - sampled[1])}


@torch.no_grad()
def generate(model, vocab, condition, temperature, top_k, generator):
    """text_model_try.sample with the per-token values recorded."""
    tokens = list(condition)
    steps = []
    for step in range(text_model.MAX_OUTPUT_PIECES + 1):
        logits = model(torch.tensor([tokens]))[0, -1]
        indices, sampled, raw = step_values(logits, step, vocab.first_emoji,
                                            temperature, top_k)
        best = int(indices[int(torch.multinomial(sampled.exp(), 1, generator=generator))])
        steps.append(token_record(indices, sampled, raw, best))
        if best == vocab.end:
            break
        tokens.append(best)
    return steps


@torch.no_grad()
def read_back(model, vocab, condition, ids, ended, temperature, top_k):
    targets = list(ids) + ([vocab.end] if ended else [])
    logits = model(torch.tensor([list(condition) + list(ids)]))[0]
    return [token_record(*step_values(logits[len(condition) - 1 + s], s, vocab.first_emoji,
                                      temperature, top_k), t)
            for s, t in enumerate(targets)]


def text_bank_line(tool, event_index, values):
    out = subprocess.run([tool, str(event_index + 1)] + [str(v) for v in values],
                         capture_output=True, text=True).stdout.splitlines()
    lines = [l.strip() for l in out[1:] if l.strip() and not l.strip().startswith("Keine")]
    return lines[0] if lines else ""


def command_measure(args):
    vocab = text_model.Vocab()
    twin = confidence.load_models(args.checkpoint)["twin"]
    generator = torch.Generator().manual_seed(args.seed)
    draw = random.Random(args.seed)
    copies = set(vocab_module.corpus_bodies())
    training = [text for _, _, text in corpus.read_corpus()]
    records = []
    for number in range(args.cases):
        event_index = draw.randrange(len(vocab_module.EVENTS))
        values = [draw.randint(0, 100) for _ in range(5)]
        levels = [level(v) for v in values]
        event = vocab_module.EVENTS[event_index]
        steps = generate(twin, vocab, vocab.condition(event, levels),
                         args.temperature, args.top_k, generator)
        ids = [s["id"] for s in steps if s["id"] != vocab.end]
        text = vocab.decode(ids)
        body = text.split(" ", 1)[1] if " " in text else ""
        records.append({"id": f"m{number + 1}", "event": event, "values": values,
                        "levels": levels, "text": text, "copy": body in copies,
                        "bank": text_bank_line(args.text_bank, event_index, values),
                        "tokens": steps})
        if (number + 1) % 100 == 0:
            print(f"{number + 1} cases", file=sys.stderr)
    forms = confidence.form_results(args.form_check, [r["text"] for r in records], training)
    for record, form in zip(records, forms):
        record["form"] = form
    write(records, args.out)
    print(f"{len(records)} texts, {sum(f == 'ok' for f in forms)} pass the form check, "
          f"{sum(r['copy'] for r in records)} verbatim corpus lines")
    return 0


def command_score(args):
    vocab = text_model.Vocab()
    twin = confidence.load_models(args.checkpoint)["twin"]
    copies = set(vocab_module.corpus_bodies())
    records = []
    for number, (event, levels_text, levels, rest) in enumerate(
            confidence.read_cases(args.texts), 1):
        text = rest[0]
        body = text.split(" ", 1)[1] if " " in text else ""
        records.append({"id": f"{args.prefix}{number}", "event": event, "levels": levels,
                        "text": text, "copy": body in copies,
                        "mark": int(rest[1]) if len(rest) > 1 and rest[1] else None,
                        "tokens": read_back(twin, vocab, vocab.condition(event, levels),
                                            vocab.encode_text(text),
                                            not confidence.was_cut(vocab, text),
                                            args.temperature, args.top_k)})
    write(records, args.out)
    return 0


def write(records, path):
    pathlib.Path(path).write_text(
        "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in records), encoding="utf-8")


# --- analysis -------------------------------------------------------------

def measure(tokens, name, upto):
    """Value of measure `name` over the first `upto` tokens (None = all).
    Higher means more confident."""
    part = tokens if upto is None else tokens[:upto]
    if name.startswith("mean_"):
        key = name[5:]
        return sum(t[key] for t in part) / len(part) * (-1 if key == "entropy" else 1)
    if name.startswith("min_"):
        key = name[4:]
        return min(t[key] for t in part)
    if name == "max_entropy":
        return -max(t["entropy"] for t in part)
    raise ValueError(name)


MEASURES = ["mean_lp", "min_lp", "mean_raw", "min_raw", "mean_entropy", "max_entropy",
            "mean_margin", "min_margin"]
POINTS = [("emoji+1", 2), ("emoji+2", 3), ("emoji+3", 4), ("whole", None)]


def bootstrap_auc(good, bad, rounds=2000, seed=1):
    draw = random.Random(seed)
    values = []
    for _ in range(rounds):
        g = [draw.choice(good) for _ in good]
        b = [draw.choice(bad) for _ in bad]
        values.append(confidence.auc(g, b))
    values.sort()
    return values[int(0.025 * rounds)], values[int(0.975 * rounds)]


def command_analyse(args):
    records = [json.loads(l) for p in args.records
               for l in pathlib.Path(p).read_text(encoding="utf-8").splitlines() if l]
    if args.marks:
        marks = dict(l.split("\t")[:2] for l in
                     pathlib.Path(args.marks).read_text(encoding="utf-8").splitlines() if l)
        for r in records:
            if r["id"] in marks:
                r["mark"] = int(marks[r["id"]])
    marked = [r for r in records if r.get("mark") is not None]
    good = [r for r in marked if r["mark"] == 1]
    bad = [r for r in marked if r["mark"] == 0]
    print(f"{len(marked)} marked texts: {len(good)} usable, {len(bad)} not usable")
    print("AUC (0.5 = no separation), 95 % bootstrap interval")
    print(f"{'measure':14s}" + "".join(f"{p:>22s}" for p, _ in POINTS))
    for name in MEASURES:
        cells = []
        for _, upto in POINTS:
            g = [measure(r["tokens"], name, upto) for r in good]
            b = [measure(r["tokens"], name, upto) for r in bad]
            low, high = bootstrap_auc(g, b)
            cells.append(f"{confidence.auc(g, b):.2f} ({low:.2f}-{high:.2f})")
        print(f"{name:14s}" + "".join(f"{c:>22s}" for c in cells))
    length_auc = confidence.auc([-len(r["text"]) for r in good], [-len(r["text"]) for r in bad])
    print(f"shorter text alone: AUC {length_auc:.2f}")
    if args.table:
        name, upto = args.table.split("@")
        upto = None if upto == "whole" else int(upto)
        population = [r for r in records if r["id"].startswith("m")]
        all_values = sorted(measure(r["tokens"], name, upto) for r in population)
        print(f"\nthresholds on {name} at {upto or 'whole'} tokens "
              f"(share of {len(population)} drawn cases below)")
        for q in (0.1, 0.2, 0.3, 0.4, 0.5):
            t = all_values[int(q * len(all_values))]
            caught = sum(measure(r["tokens"], name, upto) < t for r in bad)
            lost = sum(measure(r["tokens"], name, upto) < t for r in good)
            kept_good = len(good) - lost
            kept = kept_good + len(bad) - caught
            print(f"  below {t:7.3f} ({q:.0%} of events -> text bank): catches {caught}/{len(bad)} "
                  f"bad, loses {lost}/{len(good)} good, usable share of shown model texts "
                  f"{kept_good / kept:.0%} (now {len(good) / len(marked):.0%})")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    m = sub.add_parser("measure")
    m.add_argument("checkpoint")
    m.add_argument("--text-bank", required=True)
    m.add_argument("--form-check", required=True)
    m.add_argument("--cases", type=int, default=1000)
    m.add_argument("--seed", type=int, default=1118)
    s = sub.add_parser("score")
    s.add_argument("checkpoint")
    s.add_argument("texts")
    s.add_argument("--prefix", default="r")
    for p in (m, s):
        p.add_argument("--temperature", type=float, default=0.8)
        p.add_argument("--top-k", type=int, default=8)
        p.add_argument("--out", required=True)
    a = sub.add_parser("analyse")
    a.add_argument("records", nargs="+")
    a.add_argument("--marks")
    a.add_argument("--table", help="measure@tokens, e.g. mean_lp@whole or min_lp@3")
    args = parser.parse_args()
    return {"measure": command_measure, "score": command_score,
            "analyse": command_analyse}[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
