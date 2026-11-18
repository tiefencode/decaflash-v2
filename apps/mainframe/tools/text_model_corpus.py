#!/usr/bin/env python3
"""Training corpus of the on-device text model (add-text-model, task 5.1).

Layout (user decisions, 2026-11-18):
  apps/mainframe/text_model/corpus/<event>.tsv, one file per event, e.g. tap.tsv.
  Each line: the five mood levels in TextModelRequest order, a tab, the text
  written as "<emoji> <words>":
    ENERGY_HIGH ANNOYANCE_LOW ATTENTION_MIDDLE LONELINESS_LOW DEPRESSION_LOW<TAB>🙂 HUHU, NOCHMAL!
  The first lines follow the plan file of the event line by line; after them
  come today's fixed texts of the event, each once, with levels chosen to fit
  their content (the lead value set by the writer, the other four from `roll`).

Plan (user decision): every planned line has one lead value at LOW or HIGH; the
other four levels are random. Leads are spread evenly over the five values and
both extremes. The text follows the event and the lead value.
  apps/mainframe/text_model/plan/<event>.tsv, each line: lead, a tab, the levels.

Usage (any Python 3, no packages):
  text_model_corpus.py plan [--per-event 300] [--seed 1]   write the plan files
  text_model_corpus.py roll <LEAD> [--seed N]              levels for one lead
  text_model_corpus.py check                               check the corpus
"""

import argparse
import pathlib
import random
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_vocab as vocab  # noqa: E402

REPO = vocab.REPO
ROOT = REPO / "apps/mainframe/text_model"
CORPUS = ROOT / "corpus"
PLAN = ROOT / "plan"
LEVELS = vocab.MOOD_LEVELS            # LOW, MIDDLE, HIGH
VALUES = vocab.MOOD_VALUES            # ENERGY ... DEPRESSION
LEADS = [f"{v}_{l}" for v in VALUES for l in ("LOW", "HIGH")]


def file_name(event):
    return event.lower() + ".tsv"


def levels_text(levels):
    return " ".join(f"{VALUES[i]}_{LEVELS[l]}" for i, l in enumerate(levels))


def parse_levels(text):
    """'ENERGY_HIGH ANNOYANCE_LOW ...' -> [2, 0, ...]; ValueError if malformed."""
    parts = text.split(" ")
    if len(parts) != 5:
        raise ValueError(f"expected five mood levels, got {len(parts)}")
    levels = []
    for index, part in enumerate(parts):
        value, _, level = part.rpartition("_")
        if value != VALUES[index] or level not in LEVELS:
            raise ValueError(f"expected {VALUES[index]}_LOW/MIDDLE/HIGH, got {part!r}")
        levels.append(LEVELS.index(level))
    return levels


def roll(lead, rng):
    value, _, level = lead.rpartition("_")
    levels = [rng.randrange(3) for _ in VALUES]
    levels[VALUES.index(value)] = LEVELS.index(level)
    return levels


def write_plan(per_event, seed):
    PLAN.mkdir(parents=True, exist_ok=True)
    for event in vocab.EVENTS:
        rng = random.Random(f"{seed}:{event}")
        leads = [LEADS[i % len(LEADS)] for i in range(per_event)]
        rng.shuffle(leads)
        lines = [f"{lead}\t{levels_text(roll(lead, rng))}" for lead in leads]
        (PLAN / file_name(event)).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {len(vocab.EVENTS)} plan files with {per_event} lines each to "
          f"{PLAN.relative_to(REPO)}")


def read_corpus(directory=CORPUS):
    """[(event, levels, text)] over all event files; ValueError on a bad line."""
    examples = []
    for event in vocab.EVENTS:
        path = pathlib.Path(directory) / file_name(event)
        if not path.exists():
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip():
                continue
            levels_part, tab, text = line.partition("\t")
            if not tab:
                raise ValueError(f"{path.name}:{number}: no tab between levels and text")
            try:
                levels = parse_levels(levels_part)
            except ValueError as error:
                raise ValueError(f"{path.name}:{number}: {error}") from None
            examples.append((event, levels, text))
    return examples


def fixed_texts():
    """Today's fixed texts per event in the model's order (from message_panel.cpp)."""
    import re
    source = (REPO / "apps/mainframe/src/message_panel.cpp").read_text(encoding="utf-8")
    sets = dict(zip(vocab.EVENTS, ["kMove", "kTap", "kDoubleTap", "kMultiTap", "kImpact",
                                   "kRotate", "kReposition", "kShake", "kArrived",
                                   "kWalkedPast", "kLeft", "kStandsFar"]))
    emoji = sorted((t["text"] for t in vocab.build_vocab() if t["kind"] == "emoji"),
                   key=len, reverse=True)
    out = {}
    for event, name in sets.items():
        block = re.search(name + r"\[\] = \{(.*?)\};", source, re.S).group(1)
        texts = []
        for text in re.findall(r'u8"([^"]*)"', block):
            last = next(e for e in emoji if text.endswith(e))
            texts.append(last + " " + text[: -len(last)].rstrip())
        out[event] = texts
    return out


def form_check(lines):
    """Runs the C++ form check over the texts; returns its report lines."""
    tools = REPO / "apps/mainframe/tools"
    with tempfile.TemporaryDirectory() as tmp:
        binary = pathlib.Path(tmp) / "form_check"
        subprocess.run(["c++", "-std=c++17", "-O2", "-I", str(REPO / "apps/mainframe/src"),
                        "-I", str(tools), str(tools / "text_model_form_check.cpp"),
                        "-o", str(binary)], check=True)
        texts = pathlib.Path(tmp) / "texts.txt"
        texts.write_text("\n".join(lines) + "\n", encoding="utf-8")
        run = subprocess.run([str(binary), str(texts)], capture_output=True, text=True)
        return run.returncode, run.stdout.strip().splitlines()


def check():
    problems = []
    fixed = fixed_texts()
    all_lines = []
    total = 0
    for event in vocab.EVENTS:
        corpus_path = CORPUS / file_name(event)
        plan_path = PLAN / file_name(event)
        if not corpus_path.exists():
            problems.append(f"{corpus_path.name}: missing")
            continue
        plan = [line.split("\t")[1] for line in
                plan_path.read_text(encoding="utf-8").splitlines() if line]
        lines = [l for l in corpus_path.read_text(encoding="utf-8").splitlines() if l.strip()]
        for number, line in enumerate(lines, 1):
            levels_part, tab, text = line.partition("\t")
            try:
                parse_levels(levels_part)
            except ValueError as error:
                problems.append(f"{corpus_path.name}:{number}: {error}")
                continue
            if not tab:
                problems.append(f"{corpus_path.name}:{number}: no tab")
                continue
            if number <= len(plan) and levels_part != plan[number - 1]:
                problems.append(f"{corpus_path.name}:{number}: levels differ from the plan")
            all_lines.append(f"{corpus_path.name}:{number}\t{text}")
        planned = lines[:len(plan)]
        rest = [l.partition("\t")[2] for l in lines[len(plan):]]
        if len(planned) < len(plan):
            problems.append(f"{corpus_path.name}: {len(planned)} of {len(plan)} planned lines")
        if sorted(rest) != sorted(fixed[event]):
            problems.append(f"{corpus_path.name}: the lines after the plan are not exactly "
                            f"today's {len(fixed[event])} fixed texts")
        texts = [l.partition("\t")[2] for l in lines]
        duplicates = len(texts) - len(set(texts))
        if duplicates:
            problems.append(f"{corpus_path.name}: {duplicates} duplicate texts")
        total += len(lines)
    code, report = form_check(all_lines)
    if code:
        problems += [line for line in report if line.startswith("FAIL")]
    for problem in problems:
        print(problem)
    print(f"{total} lines, {len(problems)} problems")
    return 1 if problems else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("plan")
    p.add_argument("--per-event", type=int, default=300)
    p.add_argument("--seed", type=int, default=1)
    r = sub.add_parser("roll")
    r.add_argument("lead", choices=LEADS)
    r.add_argument("--seed", type=int, default=1)
    sub.add_parser("check")
    args = parser.parse_args()
    if args.command == "plan":
        write_plan(args.per_event, args.seed)
        return 0
    if args.command == "roll":
        print(levels_text(roll(args.lead, random.Random(args.seed))))
        return 0
    return check()


if __name__ == "__main__":
    sys.exit(main())
