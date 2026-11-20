#!/usr/bin/env python3
"""Prepares a blind rating set: text model against text bank.

Design agreed with the user on 2026-11-20 (it replaces the superseded ratings
of 2026-11-18): 144 blind pairs, one per case. Cases: each of the 12 events
with each of the 10 lead values (that value low or high, the other four
middle; 120 cases) plus the pinned device moods rest (30/0/0/50/0) and party
(95/0/40/0/0) for each event (24 cases). Per pair one text from the model image
(the firmware's writer, text_model_write) and one text bank line for the same
case (the firmware's selection code, text_bank_draw, levels as values
16/50/84). Sides A and B are drawn per pair. The user marks each text usable or
not and chooses A, B or "equally good".

Writes into DIR (default models/text-model/rating-set/<image stem>/ of the main
checkout, ignored):
  pairs.json  what the user sees: number, event, mood, A, B (no origin)
  key.json    the origin of each side and the case; not shown before the
              ratings are in

With --compare OLD, the set instead compares two writers on the same image
(acceptance of a step, change improve-text-model, Decision 2): 60 blind pairs,
5 cases per event drawn from the cases above, one text by the writer settings
OLD (text_model_write -s, for example "plain=1") and one by the firmware's
writer, each with its own seed (with one seed both writers often write the
same text, which wastes the pair). The firmware's texts are written as one sequence
in the order of the pairs, so its repetition block remembers the texts before,
as on the device; the old texts are written the same way with OLD. key.json
then names the side of the new writer. Default output directory:
models/text-model/rating-set/<image stem>-compare/.

Usage (any Python 3, no packages; needs a C++ compiler):
  text_model_rating_set.py <model.bin> [--seed 20261120] [--out DIR]
  text_model_rating_set.py <model.bin> --compare "plain=1" [--per-event 5] [--seed N] [--out DIR]
"""

import argparse
import json
import pathlib
import random
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_harness as harness  # noqa: E402

EVENT_NAMES = {  # as on the panel and in earlier rating documents
    "MOVE": "Getragen", "TAP": "Klopfer", "DOUBLE_TAP": "Doppelklopfer",
    "MULTI_TAP": "3+ Klopfer", "IMPACT": "Stoß", "ROTATE": "Gedreht",
    "REPOSITION": "Hingestellt/umgedreht", "SHAKE": "Geschüttelt",
    "ARRIVED_IN_FRONT": "Kommt davor", "WALKED_PAST": "Läuft vorbei",
    "LEFT": "Geht weg", "STANDS_FAR": "Steht weiter weg",
}
VALUE_NAMES = ["Energie", "Genervt", "Aufmerksam", "Einsam", "Depression"]
LEVEL_NAMES = ["niedrig", "mittel", "hoch"]


def mood_text(group, levels):
    if group in harness.PINNED:
        label = {"rest": "Ruhe", "party": "Party"}[group]
        values = harness.PINNED[group]
        return f"{label} ({', '.join(f'{n} {v}' for n, v in zip(VALUE_NAMES, values))})"
    off = [f"{VALUE_NAMES[i]} {LEVEL_NAMES[l]}" for i, l in enumerate(levels) if l != 1]
    return ", ".join(off) + ", sonst mittel"


def cases():
    out = []
    for event in harness.EVENTS:
        for i, lead in enumerate(harness.LEADS):
            levels = [1] * 5
            levels[i // 2] = 0 if i % 2 == 0 else 2
            out.append(("grid", event, tuple(levels),
                        tuple(harness.LEVEL_VALUE[l] for l in levels)))
        for name, values in harness.PINNED.items():
            out.append((name, event, tuple(harness.level_of(v) for v in values), values))
    return out


OLD_SEED_OFFSET = 500_000  # seed of the old writer's text: the new one's plus this


def write_sequence(tool, image, settings, rows):
    """rows: [(event index, levels, seed)] written as one sequence -> [text dict]."""
    args = [tool]
    for setting in filter(None, settings.split(",")):
        args += ["-s", setting.strip()]
    stdin = "".join(f"{e} {' '.join(map(str, levels))} {seed}\n" for e, levels, seed in rows)
    run = subprocess.run([*args, str(image), "-"], input=stdin, capture_output=True, text=True,
                         check=True)
    out = []
    for line in run.stdout.splitlines():
        seed, end, gate, text = line.split("\t")
        out.append({"seed": int(seed), "end": end, "gate": gate.split()[0][5:], "text": text})
    if len(out) != len(rows):
        raise SystemExit(f"{len(out)} texts for {len(rows)} rows")
    return out


def compare(args, image):
    out = pathlib.Path(args.out) if args.out else \
        harness.models_dir() / "rating-set" / f"{image.stem}-compare"
    out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)
    chosen = []
    for event in harness.EVENTS:
        own = [c for c in cases() if c[1] == event]
        chosen += rng.sample(own, args.per_event)
    rng.shuffle(chosen)
    rows = [(harness.EVENTS.index(event), levels, args.seed * 1000 + number)
            for number, (_, event, levels, _) in enumerate(chosen, 1)]
    with tempfile.TemporaryDirectory() as tmp:
        tools = harness.build_tools(tmp)
        new = write_sequence(tools["write"], image, "", rows)
        old = write_sequence(tools["write"], image, args.compare,
                             [(e, levels, seed + OLD_SEED_OFFSET) for e, levels, seed in rows])
    pairs, key = [], []
    for number, ((group, event, levels, values), o, n) in enumerate(zip(chosen, old, new), 1):
        new_side = rng.choice("AB")
        a, b = (n["text"], o["text"]) if new_side == "A" else (o["text"], n["text"])
        pairs.append({"nr": number, "event": EVENT_NAMES[event],
                      "mood": mood_text(group, levels), "A": a, "B": b})
        key.append({"nr": number, "group": group, "event": event,
                    "levels": "".join(map(str, levels)), "values": list(values),
                    "new_seed": n["seed"], "old_seed": o["seed"], "new_side": new_side, "new_text": n["text"],
                    "new_end": n["end"], "old_text": o["text"], "old_end": o["end"],
                    "old_writer": args.compare, "identical": n["text"] == o["text"]})
    meta = {"image": str(image), "seed": args.seed, "pairs": len(pairs),
            "old_writer": args.compare, "new_writer": "firmware default"}
    (out / "pairs.json").write_text(json.dumps({"meta": {k: meta[k] for k in ("seed", "pairs")},
                                                "pairs": pairs}, indent=1, ensure_ascii=False),
                                    encoding="utf-8")
    (out / "key.json").write_text(json.dumps({"meta": meta, "key": key}, indent=1,
                                             ensure_ascii=False), encoding="utf-8")
    same = sum(k["identical"] for k in key)
    print(f"{len(pairs)} pairs written to {out}; {same} pairs with identical texts")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("image", help="model image (.bin)")
    parser.add_argument("--seed", type=int, default=20261120, help="seed of the set")
    parser.add_argument("--out", help="output directory")
    parser.add_argument("--compare", help="old writer settings: pairs old against the firmware's writer")
    parser.add_argument("--per-event", type=int, default=5, help="cases per event with --compare")
    args = parser.parse_args()
    image = pathlib.Path(args.image).resolve()
    if args.compare:
        compare(args, image)
        return
    out = pathlib.Path(args.out) if args.out else \
        harness.models_dir() / "rating-set" / image.stem
    out.mkdir(parents=True, exist_ok=True)
    rng = random.Random(args.seed)
    all_cases = cases()
    rng.shuffle(all_cases)
    pairs, key = [], []
    with tempfile.TemporaryDirectory() as tmp:
        tools = harness.build_tools(tmp)
        for number, (group, event, levels, values) in enumerate(all_cases, 1):
            seed = args.seed * 1000 + number
            e = harness.EVENTS.index(event)
            model = harness.write_texts(tools["write"], image, e, levels, [seed])[0]
            bank = harness.draw_bank(tools["bank"], e, values, 1, seed)[0]
            model_side = rng.choice("AB")
            a, b = (model["text"], bank["text"]) if model_side == "A" else \
                (bank["text"], model["text"])
            pairs.append({"nr": number, "event": EVENT_NAMES[event],
                          "mood": mood_text(group, levels), "A": a, "B": b})
            key.append({"nr": number, "group": group, "event": event,
                        "levels": "".join(map(str, levels)), "values": list(values),
                        "model_side": model_side, "model_text": model["text"],
                        "model_seed": seed, "model_end": model["end"],
                        "model_gate": model["gate"], "bank_text": bank["text"],
                        "bank_lead": bank["lead"], "identical": model["text"] == bank["text"]})
    meta = {"image": str(image), "seed": args.seed, "pairs": len(pairs)}
    (out / "pairs.json").write_text(json.dumps({"meta": meta, "pairs": pairs}, indent=1,
                                               ensure_ascii=False), encoding="utf-8")
    (out / "key.json").write_text(json.dumps({"meta": meta, "key": key}, indent=1,
                                             ensure_ascii=False), encoding="utf-8")
    same = sum(k["identical"] for k in key)
    print(f"{len(pairs)} pairs written to {out}; {same} pairs with identical texts")


if __name__ == "__main__":
    main()
