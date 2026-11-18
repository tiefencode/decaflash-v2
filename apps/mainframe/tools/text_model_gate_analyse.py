#!/usr/bin/env python3
"""Analysis of the second confidence gate measurement (change
add-text-model-confidence-gate). Needs no torch.

  text_model_gate_analyse.py <cases.jsonl> <judge-key.json> <judge-a.tsv> <judge-b.tsv>
      [--bank-rate <usable share of text bank lines>]

cases.jsonl comes from text_model_gate_cases.py; judge-key.json maps item
numbers to case ids and sources (model text or text bank line); the two judge
files hold `nr<TAB>usable`. A text counts as not usable only when both judges
say so. Prints the judges' agreement, AUC per measure after each token
position and for the whole text, and gate tables against blind mixing.
"""

import argparse
import json
import pathlib
import random
import statistics
import sys


def auc(good, bad):
    wins = sum((a > b) + 0.5 * (a == b) for a in good for b in bad)
    return wins / (len(good) * len(bad))


def interval(good, bad, rounds=1000, seed=1):
    draw = random.Random(seed)
    values = sorted(auc([draw.choice(good) for _ in good], [draw.choice(bad) for _ in bad])
                    for _ in range(rounds))
    return values[int(0.025 * rounds)], values[int(0.975 * rounds)]


def measure(tokens, name, upto):
    """Higher means more confident. upto None = whole text; texts shorter than
    upto are taken whole (their decision falls at their end)."""
    part = tokens if upto is None else tokens[:upto]
    kind, key = name.split("_", 1)
    sign = -1 if key == "entropy" else 1
    values = [sign * t[key] for t in part]
    return sum(values) / len(values) if kind == "mean" else min(values)


MEASURES = ["mean_lp", "min_lp", "mean_raw", "min_raw", "mean_entropy", "min_entropy",
            "mean_margin", "min_margin"]
POSITIONS = [2, 3, 4, 5, 6, 7, 8, None]


def read_judge(path):
    rows = [l.split("\t") for l in pathlib.Path(path).read_text().splitlines()[1:] if l.strip()]
    return {int(r[0]): int(r[1]) for r in rows}


def visible_share(tokens, upto):
    """Share of the text's characters on the panel after token `upto`
    (emoji = token 1; words appear whole, so a word counts once its last token
    is in)."""
    pieces = [t for t in tokens if t["id"] != 0]
    total = sum(len(t["piece"]) + t["space"] for t in pieces)
    if upto is None or upto >= len(tokens):
        return 1.0
    shown = sum(len(t["piece"]) + t["space"] for t in pieces[:upto])
    return shown / total if total else 1.0


def rolling_cancel(tokens, check_from, threshold, key="margin"):
    """Planned gate: running mean of `key` from token 1, checked after every
    token from check_from on. Returns the token number of the cancel or None."""
    total = 0.0
    for number, token in enumerate(tokens, 1):
        total += token[key]
        if number >= check_from and total / number < threshold:
            return number
    return None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cases")
    parser.add_argument("key")
    parser.add_argument("judge_a")
    parser.add_argument("judge_b")
    args = parser.parse_args()
    cases = {r["id"]: r for r in map(json.loads,
                                     pathlib.Path(args.cases).read_text().splitlines())}
    key = json.loads(pathlib.Path(args.key).read_text())
    a, b = read_judge(args.judge_a), read_judge(args.judge_b)
    both = [it for it in key if it["nr"] in a and it["nr"] in b]
    agree = sum(a[it["nr"]] == b[it["nr"]] for it in both)
    print(f"judges: {len(both)} items, agree on {agree} ({agree / len(both):.1%})")
    for it in both:
        it["usable"] = 0 if a[it["nr"]] == 0 and b[it["nr"]] == 0 else 1
    model = [it for it in both if it["src"] == "model"]
    bank = [it for it in both if it["src"] == "bank"]
    p_model = sum(it["usable"] for it in model) / len(model)
    p_bank = sum(it["usable"] for it in bank) / len(bank)
    print(f"usable: model texts {sum(it['usable'] for it in model)}/{len(model)} "
          f"({p_model:.1%}), text bank lines {sum(it['usable'] for it in bank)}/{len(bank)} "
          f"({p_bank:.1%})")
    for it in model:
        it["tokens"] = cases[it["id"]]["tokens"]
    good = [it["tokens"] for it in model if it["usable"]]
    bad = [it["tokens"] for it in model if not it["usable"]]
    lengths = [len(it["tokens"]) for it in model]
    print(f"tokens per text (emoji and end counted): median {statistics.median(lengths)}, "
          f"shorter than 5: {sum(n < 5 for n in lengths) / len(lengths):.0%}")
    print("\nAUC (0.5 = no separation), 95 % bootstrap interval, after token n (emoji = 1)")
    print(f"{'measure':13s}" + "".join(f"{('whole' if p is None else p):>18}" for p in POSITIONS))
    for name in MEASURES:
        cells = []
        for p in POSITIONS:
            g = [measure(t, name, p) for t in good]
            k = [measure(t, name, p) for t in bad]
            low, high = interval(g, k)
            cells.append(f"{auc(g, k):.2f} ({low:.2f}-{high:.2f})")
        print(f"{name:13s}" + "".join(f"{c:>18s}" for c in cells))
    print(f"shorter text alone: AUC {auc([-len(t) for t in good], [-len(t) for t in bad]):.2f}")

    population = [r["tokens"] for r in cases.values()]
    print("\nGate tables: thresholds set on all cases; usable shares from the judged texts.")
    print(f"blind mixing: panel usable = (1-s)*{p_model:.3f} + s*{p_bank:.3f}")
    for name, p in [("mean_margin", None), ("mean_lp", None), ("mean_margin", 5),
                    ("mean_margin", 3)]:
        values = sorted(measure(t, name, p) for t in population)
        print(f"\n{name} decided {'at the end' if p is None else f'after token {p}'}:")
        print("  bank share | caught bad | lost good | shown model usable | panel usable | "
              "blind mixing | text visible at decision (median)")
        for s in (0.1, 0.2, 0.3, 0.4, 0.5):
            threshold = values[int(s * len(values))]
            rejected = [it for it in model if measure(it["tokens"], name, p) < threshold]
            kept = [it for it in model if measure(it["tokens"], name, p) >= threshold]
            share = len(rejected) / len(model)
            shown = sum(it["usable"] for it in kept) / len(kept)
            panel = (1 - share) * shown + share * p_bank
            blind = (1 - share) * p_model + share * p_bank
            seen = statistics.median(visible_share(it["tokens"], p) for it in rejected)
            print(f"  {share:9.0%} | {sum(not it['usable'] for it in rejected):4d}/{len(bad):3d} | "
                  f"{sum(it['usable'] for it in rejected):4d}/{len(good):3d} | {shown:17.1%} | "
                  f"{panel:11.1%} | {blind:11.1%} | {seen:.0%}")
    print("\nPlanned rule (running mean margin, checked after every token from token 5):")
    for threshold in (0.70, 1.0, 1.5, 2.0, 2.5, 3.0):
        cancels = [rolling_cancel(it["tokens"], 5, threshold) for it in model]
        rejected = [it for it, c in zip(model, cancels) if c]
        kept = [it for it, c in zip(model, cancels) if not c]
        all_share = sum(rolling_cancel(t, 5, threshold) is not None
                        for t in population) / len(population)
        if not rejected or not kept:
            print(f"  threshold {threshold:.2f}: cancels {all_share:.1%} of all cases")
            continue
        share = len(rejected) / len(model)
        shown = sum(it["usable"] for it in kept) / len(kept)
        panel = (1 - share) * shown + share * p_bank
        blind = (1 - share) * p_model + share * p_bank
        seen = statistics.median(visible_share(it["tokens"], c)
                                 for it, c in zip(model, cancels) if c)
        print(f"  threshold {threshold:.2f}: cancels {all_share:.1%} of all cases; judged: "
              f"caught bad {sum(not it['usable'] for it in rejected)}/{len(bad)}, lost good "
              f"{sum(it['usable'] for it in rejected)}/{len(good)}, shown model usable "
              f"{shown:.1%}, panel {panel:.1%}, blind mixing {blind:.1%}, text visible at "
              f"cancel {seen:.0%}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
