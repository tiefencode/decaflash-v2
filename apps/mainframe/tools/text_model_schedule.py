#!/usr/bin/env python3
"""Sampling schedules for the text model: sample only the first tokens of a
text, then finish it with the most likely (or a milder sampled) token. Host
comparison through the 4-bit twin; run it with the Python of /.venv-text-model/.

  text_model_schedule.py run <checkpoint.pt> --form-check <text_model_form_check>
                         [--cases 1000] [--seed 1118] --out <records.jsonl>
      Draws the same random cases as text_model_gate.py measure (same seed),
      and writes one text per case and schedule. Every case has its own
      generator seed, the same for every schedule, so all schedules share the
      emoji and the first sampled tokens. Records: case, schedule, text, form
      check result, verbatim corpus line, and the raw log-probability
      (temperature 1, no top-k) of every token.

  text_model_schedule.py report <records.jsonl>
      One table per schedule: form check passed, verbatim corpus lines, near
      copies (at most two pieces from a corpus line), distinct texts, emoji
      and first two words, mean length in pieces, mean raw log-probability.

A schedule is a list of (from step, temperature, top-k); step 0 is the emoji,
temperature 0 means the most likely token.
"""

import argparse
import collections
import json
import pathlib
import random
import sys

import torch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model  # noqa: E402
import text_model_confidence as confidence  # noqa: E402
import text_model_corpus as corpus  # noqa: E402
import text_model_vocab as vocab_module  # noqa: E402
from text_model_gate import level  # noqa: E402

SCHEDULES = {
    "now": [(0, 0.8, 8)],
    "greedy-after-1": [(0, 0.8, 8), (2, 0, 1)],
    "greedy-after-2": [(0, 0.8, 8), (3, 0, 1)],
    "mild-after-1": [(0, 0.8, 8), (2, 0.5, 3)],
}


def setting(schedule, step):
    return [s for s in schedule if s[0] <= step][-1][1:]


@torch.no_grad()
def generate(model, vocab, condition, schedule, generator):
    tokens, ids, raws = list(condition), [], []
    for step in range(text_model.MAX_OUTPUT_PIECES + 1):
        row = model(torch.tensor([tokens]))[0, -1].clone()
        if step == 0:
            row[:vocab.first_emoji] = float("-inf")
        else:
            row[vocab.first_emoji:] = float("-inf")
        raw = torch.log_softmax(row, dim=-1)
        temperature, top_k = setting(schedule, step)
        values, indices = torch.topk(row, top_k)
        # Always draw once so the generator stays in step across schedules.
        pick = int(torch.multinomial(torch.softmax(values / max(temperature, 1e-6), -1), 1,
                                     generator=generator))
        best = int(indices[0 if temperature == 0 else pick])
        raws.append(float(raw[best]))
        if best == vocab.end:
            break
        ids.append(best)
        tokens.append(best)
    return ids, raws


def command_run(args):
    vocab = text_model.Vocab()
    twin = confidence.load_models(args.checkpoint)["twin"]
    draw = random.Random(args.seed)
    copies = set(vocab_module.corpus_bodies())
    training = [text for _, _, text in corpus.read_corpus()]
    cases = []
    for _ in range(args.cases):
        event_index = draw.randrange(len(vocab_module.EVENTS))
        cases.append((vocab_module.EVENTS[event_index], [level(draw.randint(0, 100))
                                                         for _ in range(5)]))
    records = []
    for name, schedule in SCHEDULES.items():
        for number, (event, levels) in enumerate(cases):
            generator = torch.Generator().manual_seed(args.seed * 100000 + number)
            ids, raws = generate(twin, vocab, vocab.condition(event, levels), schedule, generator)
            text = vocab.decode(ids)
            body = text.split(" ", 1)[1] if " " in text else ""
            records.append({"case": number + 1, "schedule": name, "event": event,
                            "levels": levels, "text": text, "copy": body in copies,
                            "raw": raws})
        print(f"{name} done", file=sys.stderr)
    forms = confidence.form_results(args.form_check, [r["text"] for r in records], training)
    for record, form in zip(records, forms):
        record["form"] = form
    pathlib.Path(args.out).write_text(
        "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in records), encoding="utf-8")
    return 0


def distance(a, b, limit):
    """Levenshtein distance between piece lists, capped at limit + 1."""
    previous = list(range(len(b) + 1))
    for i, x in enumerate(a, 1):
        current = [i]
        for j, y in enumerate(b, 1):
            current.append(min(previous[j] + 1, current[j - 1] + 1, previous[j - 1] + (x != y)))
        if min(current) > limit:
            return limit + 1
        previous = current
    return previous[-1]


def near_copy(pieces, corpus_pieces, limit=2):
    for other in corpus_pieces:
        if abs(len(other) - len(pieces)) > limit:
            continue
        if len(set(pieces) & set(other)) < max(len(pieces), len(other)) - limit - 1:
            continue
        if distance(pieces, other, limit) <= limit:
            return True
    return False


def command_report(args):
    records = [json.loads(l) for l in pathlib.Path(args.records).read_text(
        encoding="utf-8").splitlines() if l]
    corpus_pieces = [vocab_module.split_body(b) for b in set(vocab_module.corpus_bodies())]
    groups = collections.defaultdict(list)
    for r in records:
        groups[r["schedule"]].append(r)
    print("| schedule | form ok | verbatim | near copy | distinct texts | distinct emoji "
          "| distinct first two words | mean pieces | mean raw log-prob |")
    print("|---|---|---|---|---|---|---|---|---|")
    for name, rows in groups.items():
        n = len(rows)
        bodies = [r["text"].split(" ", 1)[1] if " " in r["text"] else "" for r in rows]
        pieces = [vocab_module.split_body(b) for b in bodies]
        near = sum(not r["copy"] and near_copy(p, corpus_pieces) for r, p in zip(rows, pieces))
        tokens = [t for r in rows for t in r["raw"]]
        print(f"| {name} | {sum(r['form'] == 'ok' for r in rows) / n:.0%} "
              f"| {sum(r['copy'] for r in rows) / n:.0%} | {near / n:.0%} "
              f"| {len(set(r['text'] for r in rows))} "
              f"| {len(set(r['text'].split(' ', 1)[0] for r in rows))} "
              f"| {len(set(tuple(p[:2]) for p in pieces))} "
              f"| {sum(map(len, pieces)) / n:.1f} | {sum(tokens) / len(tokens):.2f} |")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    r = sub.add_parser("run")
    r.add_argument("checkpoint")
    r.add_argument("--form-check", required=True)
    r.add_argument("--cases", type=int, default=1000)
    r.add_argument("--seed", type=int, default=1118)
    r.add_argument("--out", required=True)
    p = sub.add_parser("report")
    p.add_argument("records")
    args = parser.parse_args()
    return {"run": command_run, "report": command_report}[args.command](args)


if __name__ == "__main__":
    sys.exit(main())
