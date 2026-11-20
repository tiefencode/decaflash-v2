#!/usr/bin/env python3
"""Fixed automatic measurement harness for the text model.

One command per model image. It writes texts for fixed cases and seeds with
the firmware's writer on the 4-bit image (text_model_write: masks, sampling
schedule, length limit), draws text bank lines for the same cases with the
firmware's selection code (text_bank_draw), and takes the held-out corpus lines
(the validation split the training leaves out) as a third reference row. Every
row gets the same metrics:

  copies    verbatim training lines; longest word n-gram shared with a training line
  variety   distinct texts, distinct openings (first two words) and share of the
            most frequent text per 100 seeds, for the 30 rating cases and for the
            pinned device moods rest (30/0/0/50/0) and party (95/0/40/0/0)
  event     accuracy of a bag-of-words event classifier trained on the training
            lines (its accuracy on the held-out lines is the ceiling); share of
            texts with a field word of another event and none of their own
            (word_fields.tsv); share with a word of their own field
  mood      on the lead grid (one mood value low or high, the others middle):
            emoji-to-lead match (most likely lead of the emoji in the training
            lines) and a bag-of-words lead classifier
  form      share of word tokens that occur once in the training lines, share of
            words outside them, end reason (end token or length limit), form
            check pass rate, share the confidence gate would reject

Sampling noise: the model and text bank rows are measured in independent seed
blocks (default 3) and reported as mean and range over the blocks.

With --ratings FILE, a file of the user's own ratings (columns as in
apps/mainframe/text_model/ratings_superseded_2026-11-18.tsv) scores every
per-text automatic proxy: AUC (above 0.5: higher means
usable more often) and the cross-validated
accuracy of a one-threshold rule (stratified 10-fold cross-validation,
20 repeats) against the base rate (always "usable").
A proxy counts as useful only if the 95 % bootstrap interval of its accuracy
gain over the base rate lies above zero. The ratings of 2026-11-18 are
superseded (user, 2026-11-20) and are not scored by default.

Usage (any Python 3, no packages; needs a C++ compiler):
  text_model_harness.py <model.bin> [--blocks 3] [--out DIR]
  text_model_harness.py --ratings FILE [--ratings-only]

Output: a summary on stdout (Markdown), and in DIR (default
models/text-model/harness/<image stem>/ of the main checkout) summary.json and
every text as TSV.
"""

import argparse
import concurrent.futures
import json
import math
import pathlib
import random
import subprocess
import sys
import tempfile
from collections import Counter, defaultdict

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_bank  # noqa: E402
import text_model_corpus as corpus  # noqa: E402
import text_model_vocab as vocab  # noqa: E402

REPO = vocab.REPO
TOOLS = REPO / "apps/mainframe/tools"
SRC = REPO / "apps/mainframe/src"
ROOT = REPO / "apps/mainframe/text_model"
FIELDS = ROOT / "word_fields.tsv"
RATING_CASES = ROOT / "rating_cases.tsv"

EVENTS = vocab.EVENTS
LEADS = corpus.LEADS  # ENERGY_LOW, ENERGY_HIGH, ANNOYANCE_LOW, ...
LEVEL_VALUE = (16, 50, 84)  # a level as a mood value for the text bank (middle of the third)
PINNED = {"rest": (30, 0, 0, 50, 0), "party": (95, 0, 40, 0, 0)}

BLOCK_SEED = 100_000  # seed of block b, case c, draw i: b*BLOCK_SEED + c*1000 + i + 1
VARIETY_SEEDS = 100   # per rating case and per pinned case
GRID_SEEDS = 25       # per lead-grid case (12 events x 10 leads)


# --- corpus side --------------------------------------------------------------

def level_of(value):
    """Mood value 0-100 -> level 0/1/2, as reaction_texts.cpp moodLevel."""
    return 0 if value <= 33 else 1 if value <= 66 else 2


def leads_of(values):
    """The lead values TextBank::leadFor can draw for these mood values (ties)."""
    distance = [abs(v - 50) for v in values]
    best = max(distance)
    return [LEADS[2 * i + (0 if values[i] < 50 else 1)]
            for i, d in enumerate(distance) if d == best]


def split_text(text):
    """'<emoji> <body>' -> (emoji, [words]) without punctuation."""
    emoji, _, body = text.partition(" ")
    try:
        pieces = vocab.split_body(body)
    except ValueError:
        pieces = body.split()
    return emoji, [p for p in pieces if p not in vocab.PUNCTUATION]


def load_corpus():
    """(training, held) as [(event, lead, text)] in the model's split."""
    lead_of = {}
    for (e, l), texts in text_bank.build().items():
        for panel in texts:
            words, _, emoji = panel.rpartition(" ")
            lead_of[(EVENTS[e], f"{emoji} {words}")] = LEADS[l]
    training, held = corpus.split(corpus.read_corpus())
    def rows(examples):
        return [(event, lead_of[(event, text)], text) for event, _, text in examples]
    return rows(training), rows(held)


def load_fields():
    """{word: set of events}."""
    fields = defaultdict(set)
    for line in FIELDS.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        events, _, word = line.partition("\t")
        for event in events.split(","):
            fields[word.strip()].add(event.strip().upper())
    return fields


class NaiveBayes:
    """Multinomial naive Bayes over word lists, Laplace smoothing."""

    def __init__(self, examples):  # [(label, [features])]
        self.labels = sorted({label for label, _ in examples})
        self.prior = Counter(label for label, _ in examples)
        self.counts = {label: Counter() for label in self.labels}
        for label, features in examples:
            self.counts[label].update(features)
        self.vocab = {f for c in self.counts.values() for f in c}
        self.total = {label: sum(c.values()) for label, c in self.counts.items()}
        self.n = len(examples)

    def scores(self, features):
        size = len(self.vocab)
        out = {}
        for label in self.labels:
            s = math.log(self.prior[label] / self.n)
            for f in features:
                if f in self.vocab:
                    s += math.log((self.counts[label][f] + 1) / (self.total[label] + size))
            out[label] = s
        return out

    def posterior(self, features):
        s = self.scores(features)
        top = max(s.values())
        exp = {k: math.exp(v - top) for k, v in s.items()}
        z = sum(exp.values())
        return {k: v / z for k, v in exp.items()}

    def predict(self, features):
        s = self.scores(features)
        return max(s, key=s.get)


class Reference:
    """Everything measured against the training lines."""

    def __init__(self, training, held):
        self.training, self.held = training, held
        self.fields = load_fields()
        self.copies = {tuple(split_text(t)[1]) for _, _, t in training}
        self.word_count = Counter(w for _, _, t in training for w in split_text(t)[1])
        self.ngrams = defaultdict(set)  # n -> set of word n-grams
        for _, _, t in training:
            words = split_text(t)[1]
            for n in range(1, len(words) + 1):
                for i in range(len(words) - n + 1):
                    self.ngrams[n].add(tuple(words[i:i + n]))
        self.event_nb = NaiveBayes([(e, split_text(t)[1]) for e, _, t in training])
        self.lead_nb = NaiveBayes([(l, split_text(t)[1]) for _, l, t in training])
        emoji_lead = defaultdict(Counter)
        for _, l, t in training:
            emoji_lead[split_text(t)[0]][l] += 1
        self.emoji_lead = {e: c.most_common(1)[0][0] for e, c in emoji_lead.items()}

    def longest_shared(self, words):
        best = 0
        for n in range(1, len(words) + 1):
            grams = self.ngrams.get(n, ())
            if any(tuple(words[i:i + n]) in grams for i in range(len(words) - n + 1)):
                best = n
            else:
                break
        return best

    def field_words(self, words, event):
        own = other = False
        for w in words:
            events = self.fields.get(w)
            if not events:
                continue
            if event in events:
                own = True
            else:
                other = True
        return own, other

    def features(self, text, event, leads):
        """Per-text measures; leads: acceptable target leads or None."""
        emoji, words = split_text(text)
        own, other = self.field_words(words, event)
        once = sum(1 for w in words if self.word_count[w] == 1)
        outside = sum(1 for w in words if self.word_count[w] == 0)
        f = {
            "copy": tuple(words) in self.copies,
            "ngram": self.longest_shared(words),
            "words": len(words),
            "once": once, "outside": outside,
            "event_ok": self.event_nb.predict(words) == event,
            "event_p": self.event_nb.posterior(words)[event],
            "own_field": own, "other_field_only": other and not own,
            "opening": " ".join(words[:2]),
        }
        if leads:
            post = self.lead_nb.posterior(words)
            f["lead_ok"] = self.lead_nb.predict(words) in leads
            f["lead_p"] = max(post[l] for l in leads)
            f["emoji_ok"] = self.emoji_lead.get(emoji) in leads
        return f


# --- host tools ---------------------------------------------------------------

def build_tools(directory):
    d = pathlib.Path(directory)
    jobs = {
        "write": [str(TOOLS / "text_model_write.cpp")],
        "bank": [str(TOOLS / "text_bank_draw.cpp"), str(SRC / "text_bank.cpp")],
        "form": ["-I", str(TOOLS), str(TOOLS / "text_model_form_check.cpp")],
    }
    out = {}
    for name, args in jobs.items():
        binary = d / name
        subprocess.run(["c++", "-std=c++17", "-O2", "-I", str(SRC), *args, "-o", str(binary)],
                       check=True)
        out[name] = str(binary)
    return out


def write_texts(tool, image, event, levels, seeds):
    run = subprocess.run([tool, str(image), str(event), *map(str, levels), *map(str, seeds)],
                         capture_output=True, text=True, check=True)
    out = []
    for line in run.stdout.splitlines():
        seed, end, gate, text = line.split("\t")
        out.append({"seed": int(seed), "end": end, "gate": gate.split()[0][5:], "text": text})
    return out


def draw_bank(tool, event, values, count, seed):
    run = subprocess.run([tool, str(event), *map(str, values), str(count), str(seed)],
                         capture_output=True, text=True, check=True)
    return [{"lead": l, "text": t} for l, t in (x.split("\t") for x in run.stdout.splitlines())]


def form_pass(tool, texts):
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="utf-8") as f:
        f.write("\n".join(texts) + "\n")
        path = f.name
    run = subprocess.run([tool, path], capture_output=True, text=True)
    pathlib.Path(path).unlink()
    failed = sum(1 for line in run.stdout.splitlines() if line.startswith("FAIL"))
    return 1 - failed / len(texts)


# --- cases --------------------------------------------------------------------

def rating_cases():
    out = []
    for line in RATING_CASES.read_text(encoding="utf-8").splitlines():
        event, levels = line.split("\t")
        out.append((event, tuple(corpus.parse_levels(levels))))
    return out


def single_lead(levels):
    """The lead of a level tuple with exactly one non-middle level, else None."""
    off = [i for i, l in enumerate(levels) if l != 1]
    if len(off) != 1:
        return None
    return [LEADS[2 * off[0] + (0 if levels[off[0]] == 0 else 1)]]


def cases():
    """[(group, event, levels, values, target leads, seeds per block)]."""
    out = []
    for event, levels in rating_cases():
        out.append(("rating", event, levels, tuple(LEVEL_VALUE[l] for l in levels),
                    single_lead(levels), VARIETY_SEEDS))
    for name, values in PINNED.items():
        for event in EVENTS:
            out.append((name, event, tuple(level_of(v) for v in values), values,
                        leads_of(values), VARIETY_SEEDS))
    for event in EVENTS:
        for i, lead in enumerate(LEADS):
            levels = [1] * 5
            levels[i // 2] = 0 if i % 2 == 0 else 2
            out.append(("grid", event, tuple(levels), tuple(LEVEL_VALUE[l] for l in levels),
                        [lead], GRID_SEEDS))
    return out


# --- measuring ----------------------------------------------------------------

def mean(xs):
    xs = list(xs)
    return sum(xs) / len(xs) if xs else float("nan")


def summarise(items, ref, form_tool):
    """Row metrics over [(case, text, extra)]; case = cases() tuple."""
    feats = []
    for case, text, extra in items:
        group, event, levels, values, leads, _ = case
        f = ref.features(text, event, leads if group == "grid" else None)
        f.update(extra)
        f["case"] = (group, event, levels)
        feats.append((case, text, f))
    words = sum(f["words"] for _, _, f in feats)
    m = {
        "texts": len(feats),
        "copy": mean(f["copy"] for _, _, f in feats),
        "ngram_mean": mean(f["ngram"] for _, _, f in feats if not f["copy"]),
        "ngram_ge5": mean(f["ngram"] >= 5 for _, _, f in feats if not f["copy"]),
        "event_acc": mean(f["event_ok"] for _, _, f in feats),
        "own_field": mean(f["own_field"] for _, _, f in feats),
        "other_field_only": mean(f["other_field_only"] for _, _, f in feats),
        "once_share": sum(f["once"] for _, _, f in feats) / max(words, 1),
        "outside_share": sum(f["outside"] for _, _, f in feats) / max(words, 1),
        "words_mean": words / len(feats),
        "form_pass": form_pass(form_tool, [t for _, t, _ in feats]),
    }
    grid = [f for c, _, f in feats if c[0] == "grid"]
    if grid:
        m["emoji_lead"] = mean(f["emoji_ok"] for f in grid)
        m["word_lead"] = mean(f["lead_ok"] for f in grid)
    ends = [f["end"] for _, _, f in feats if "end" in f]
    if ends:
        m["end_limit"] = mean(e == "limit" for e in ends)
        m["gate_rejected"] = mean(f["gate"] == "rejected" for _, _, f in feats)
    for group in ("rating", "rest", "party"):
        per_case = defaultdict(list)
        for c, t, f in feats:
            if c[0] == group:
                per_case[(c[1], c[2])].append((t, f["opening"]))
        if not per_case:
            continue
        distinct, openings, top = [], [], []
        for draws in per_case.values():
            scale = 100 / len(draws)
            texts = Counter(t for t, _ in draws)
            distinct.append(len(texts) * scale)
            openings.append(len({o for _, o in draws}) * scale)
            top.append(texts.most_common(1)[0][1] / len(draws))
        m[f"{group}_distinct"] = mean(distinct)
        m[f"{group}_openings"] = mean(openings)
        m[f"{group}_top"] = mean(top)
    return m, feats


def held_row(ref, form_tool):
    """The held-out corpus lines as one reference row (lead metrics on every line)."""
    items, feats = [], []
    for event, lead, text in ref.held:
        f = ref.features(text, event, [lead])
        items.append(text)
        feats.append(f)
    words = sum(f["words"] for f in feats)
    return {
        "texts": len(feats),
        "copy": mean(f["copy"] for f in feats),
        "ngram_mean": mean(f["ngram"] for f in feats if not f["copy"]),
        "ngram_ge5": mean(f["ngram"] >= 5 for f in feats if not f["copy"]),
        "event_acc": mean(f["event_ok"] for f in feats),
        "own_field": mean(f["own_field"] for f in feats),
        "other_field_only": mean(f["other_field_only"] for f in feats),
        "once_share": sum(f["once"] for f in feats) / max(words, 1),
        "outside_share": sum(f["outside"] for f in feats) / max(words, 1),
        "words_mean": words / len(feats),
        "form_pass": form_pass(form_tool, items),
        "emoji_lead": mean(f["emoji_ok"] for f in feats),
        "word_lead": mean(f["lead_ok"] for f in feats),
    }


def run_blocks(image, tools, ref, blocks):
    all_cases = cases()
    model_rows, bank_rows, dump = [], [], []
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        for b in range(blocks):
            jobs = []
            for c, case in enumerate(all_cases):
                group, event, levels, values, leads, n = case
                seeds = [b * BLOCK_SEED + c * 1000 + i + 1 for i in range(n)]
                e = EVENTS.index(event)
                jobs.append((case, pool.submit(write_texts, tools["write"], image, e, levels, seeds),
                             pool.submit(draw_bank, tools["bank"], e, values, n, seeds[0])))
            model_items, bank_items = [], []
            for case, mj, bj in jobs:
                for w in mj.result():
                    model_items.append((case, w["text"], {"end": w["end"], "gate": w["gate"]}))
                    dump.append(("model", b, case[0], case[1], "".join(map(str, case[2])),
                                 str(w["seed"]), w["end"], w["gate"], w["text"]))
                for d in bj.result():
                    bank_items.append((case, d["text"], {}))
                    dump.append(("bank", b, case[0], case[1], "".join(map(str, case[2])),
                                 "", "", d["lead"], d["text"]))
            model_rows.append(summarise(model_items, ref, tools["form"])[0])
            bank_rows.append(summarise(bank_items, ref, tools["form"])[0])
    return model_rows, bank_rows, dump


def spread(rows):
    """{metric: (mean, min, max)} over seed blocks."""
    out = {}
    for key in rows[0]:
        xs = [r[key] for r in rows if key in r]
        out[key] = (mean(xs), min(xs), max(xs))
    return out


# --- user ratings -------------------------------------------------------------

def read_ratings(path):
    out = []
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        s, item, source, event, levels, values, text, usable, note = (line.split("\t") + [""])[:9]
        if usable == "":
            continue
        levels = tuple(int(x) for x in levels)
        vals = tuple(int(x) for x in values.split("/")) if values else \
            tuple(LEVEL_VALUE[l] for l in levels)
        out.append({"set": s, "source": source, "event": event, "levels": levels,
                    "leads": leads_of(vals), "text": text, "usable": int(usable)})
    return out


def auc(scores, labels):
    pos = [s for s, y in zip(scores, labels) if y]
    neg = [s for s, y in zip(scores, labels) if not y]
    if not pos or not neg:
        return float("nan")
    wins = sum((p > q) + 0.5 * (p == q) for p in pos for q in neg)
    return wins / (len(pos) * len(neg))


def best_rule(scores, labels):
    """(threshold, sign) of the most accurate 'usable if (score - t) * sign >= 0'."""
    order = sorted(set(scores))
    cuts = [order[0] - 1] + [(a + b) / 2 for a, b in zip(order, order[1:])] + [order[-1] + 1]
    best, rule = -1, None
    for t in cuts:
        for sign in (1, -1):
            acc = sum(((s - t) * sign >= 0) == bool(y) for s, y in zip(scores, labels))
            if acc > best:
                best, rule = acc, (t, sign)
    return rule


def cv_hits(scores, labels, rng, folds=10, repeats=20):
    """Per text, the share of repeats in which a one-threshold rule chosen on the
    other folds (stratified 10-fold cross-validation) classified it correctly."""
    n = len(scores)
    hits = [0.0] * n
    for _ in range(repeats):
        fold_of = [0] * n
        for label in (0, 1):
            idx = [i for i in range(n) if labels[i] == label]
            rng.shuffle(idx)
            for j, i in enumerate(idx):
                fold_of[i] = j % folds
        for f in range(folds):
            train = [i for i in range(n) if fold_of[i] != f]
            t, sign = best_rule([scores[i] for i in train], [labels[i] for i in train])
            for i in range(n):
                if fold_of[i] == f:
                    hits[i] += (((scores[i] - t) * sign >= 0) == bool(labels[i])) / repeats
    return hits


def score_proxies(ref, rated, seed=20261120, resamples=2000):
    proxies = {
        "event classifier, probability of the case event": lambda f: f["event_p"],
        "lead classifier, probability of the case lead": lambda f: f["lead_p"],
        "emoji matches the case lead": lambda f: float(f["emoji_ok"]),
        "event classifier agrees": lambda f: float(f["event_ok"]),
        "verbatim training line": lambda f: float(f["copy"]),
        "longest shared n-gram": lambda f: f["ngram"],
        "words occurring once (count)": lambda f: f["once"],
        "words outside the training lines": lambda f: f["outside"],
        "own field word": lambda f: float(f["own_field"]),
        "other event's field word only": lambda f: float(f["other_field_only"]),
        "number of words": lambda f: f["words"],
    }
    subsets = {f"all rated texts ({len(rated)})": rated}
    models = [g for g in rated if g["source"] != "text_bank"]
    if 0 < len(models) < len(rated):
        subsets[f"model texts ({len(models)})"] = models
    sources = Counter(g["source"] for g in rated)
    if len(sources) > 1:
        for source, n in sources.items():
            if n >= 30:
                subsets[f"{source} ({n})"] = [g for g in rated if g["source"] == source]
    rng = random.Random(seed)
    result = {}
    for sname, items in subsets.items():
        labels = [g["usable"] for g in items]
        feats = [ref.features(g["text"], g["event"], g["leads"]) for g in items]
        base = max(mean(labels), 1 - mean(labels))
        rows = {}
        for pname, fn in proxies.items():
            scores = [fn(f) for f in feats]
            a = auc(scores, labels)
            hits = cv_hits(scores, labels, rng)
            major = [y == (mean(labels) >= 0.5) for y in labels]
            gains, aucs = [], []
            n = len(items)
            for _ in range(resamples):
                idx = [rng.randrange(n) for _ in range(n)]
                gains.append(mean(hits[i] for i in idx) - mean(major[i] for i in idx))
                aucs.append(auc([scores[i] for i in idx], [labels[i] for i in idx]))
            gains.sort()
            aucs = sorted(x for x in aucs if not math.isnan(x))
            low, high = gains[int(0.025 * resamples)], gains[int(0.975 * resamples) - 1]
            rows[pname] = {"auc": a, "auc_ci": (aucs[int(0.025 * len(aucs))],
                                                aucs[int(0.975 * len(aucs)) - 1]),
                           "cv_acc": mean(hits), "base": base,
                           "gain_ci": (low, high), "useful": low > 1e-9}
        result[sname] = {"n": len(items), "usable": sum(labels), "base": base, "proxies": rows}
    return result


def pairs_needed(p=0.7, alpha=0.05, power=0.8):
    """Decided blind pairs for a two-sided sign test to detect preference p."""
    from math import comb
    for n in range(10, 400):
        def upper(k):
            return sum(comb(n, i) for i in range(k, n + 1)) / 2 ** n
        k = next(k for k in range(n // 2, n + 2) if upper(k) <= alpha / 2)
        pw = sum(comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(k, n + 1))
        if pw >= power:
            return n
    return None


# --- report -------------------------------------------------------------------

METRICS = [
    ("copy", "verbatim training line", "%"),
    ("ngram_mean", "longest shared n-gram, words (non-copies)", "f"),
    ("ngram_ge5", "non-copies sharing 5 or more words in a row", "%"),
    ("rating_distinct", "rating cases: distinct texts per 100 seeds", "f"),
    ("rating_openings", "rating cases: distinct openings per 100 seeds", "f"),
    ("rating_top", "rating cases: share of the most frequent text", "%"),
    ("rest_distinct", "rest: distinct texts per 100 seeds", "f"),
    ("rest_openings", "rest: distinct openings per 100 seeds", "f"),
    ("rest_top", "rest: share of the most frequent text", "%"),
    ("party_distinct", "party: distinct texts per 100 seeds", "f"),
    ("party_openings", "party: distinct openings per 100 seeds", "f"),
    ("party_top", "party: share of the most frequent text", "%"),
    ("event_acc", "event classifier recognises the event", "%"),
    ("own_field", "text has a word of its event's field", "%"),
    ("other_field_only", "text has another event's field word and none of its own", "%"),
    ("emoji_lead", "lead grid: emoji matches the lead", "%"),
    ("word_lead", "lead grid: lead classifier recognises the lead", "%"),
    ("once_share", "word tokens occurring once in the training lines", "%"),
    ("outside_share", "word tokens outside the training lines", "%"),
    ("words_mean", "words per text", "f"),
    ("end_limit", "ended at the length limit", "%"),
    ("form_pass", "form check passed (copies aside)", "%"),
    ("gate_rejected", "confidence gate would reject", "%"),
]


def fmt(value, kind):
    if value is None or (isinstance(value, float) and math.isnan(value)):
        return "–"
    return f"{100 * value:.1f} %" if kind == "%" else f"{value:.2f}"


def cell(entry, kind):
    if entry is None:
        return "–"
    if isinstance(entry, tuple):
        m, lo, hi = entry
        if abs(hi - lo) < 1e-9:
            return fmt(m, kind)
        return f"{fmt(m, kind)} ({fmt(lo, kind)}–{fmt(hi, kind)})"
    return fmt(entry, kind)


def report(summary):
    lines = [f"Model image: `{summary['image']}`, {summary['blocks']} seed blocks; "
             f"model and text bank: mean (range over blocks).", "",
             "| Metric | Text model | Text bank | Held-out corpus lines |", "|---|---|---|---|"]
    for key, label, kind in METRICS:
        lines.append(f"| {label} | {cell(summary['model'].get(key), kind)} | "
                     f"{cell(summary['bank'].get(key), kind)} | "
                     f"{cell(summary['held'].get(key), kind)} |")
    lines += ["", "About 90 % of the text bank lines are training lines of the classifiers and "
              "of the copy check, so the text bank's copy, n-gram and classifier values are not "
              "comparable; the held-out corpus lines are the fair reference."]
    return "\n".join(lines)


def ratings_report(scores):
    lines = []
    for sname, s in scores.items():
        lines += ["", f"Ratings: {sname}, {s['usable']} of {s['n']} usable, base rate "
                  f"{100 * s['base']:.1f} %", "",
                  "| Proxy | AUC (95 % CI) | CV accuracy | Gain over base rate (95 % CI) | Useful |",
                  "|---|---|---|---|---|"]
        for pname, r in s["proxies"].items():
            lo, hi = r["gain_ci"]
            al, ah = r["auc_ci"]
            lines.append(f"| {pname} | {r['auc']:.2f} ({al:.2f}–{ah:.2f}) | {100 * r['cv_acc']:.1f} % | "
                         f"{100 * lo:+.1f} to {100 * hi:+.1f} points | "
                         f"{'yes' if r['useful'] else 'no'} |")
    return "\n".join(lines)


def models_dir():
    """models/text-model of the main checkout: ignored files in a worktree are lost
    when the worktree is removed."""
    try:
        common = subprocess.run(["git", "-C", str(REPO), "rev-parse", "--path-format=absolute",
                                 "--git-common-dir"], capture_output=True, text=True, check=True)
        return pathlib.Path(common.stdout.strip()).parent / "models/text-model"
    except (OSError, subprocess.CalledProcessError):
        return REPO / "models/text-model"


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("image", nargs="?", help="model image (.bin)")
    parser.add_argument("--blocks", type=int, default=3, help="seed blocks (default 3)")
    parser.add_argument("--out", help="output directory (default models/text-model/harness/<stem>)")
    parser.add_argument("--ratings", help="file of the user's ratings to score the proxies against")
    parser.add_argument("--ratings-only", action="store_true", help="only score the proxies")
    args = parser.parse_args()
    training, held = load_corpus()
    ref = Reference(training, held)
    scores = score_proxies(ref, read_ratings(args.ratings)) if args.ratings else None
    ceiling = {
        "event": mean(ref.event_nb.predict(split_text(t)[1]) == e for e, _, t in held),
        "lead": mean(ref.lead_nb.predict(split_text(t)[1]) == l for _, l, t in held),
        "emoji_lead": mean(ref.emoji_lead.get(split_text(t)[0]) == l for _, l, t in held),
    }
    if args.ratings_only:
        if not scores:
            parser.error("--ratings-only needs --ratings FILE")
        print(ratings_report(scores))
        print(f"\nClassifier ceilings on {len(held)} held-out lines: "
              + ", ".join(f"{k} {100 * v:.1f} %" for k, v in ceiling.items()))
        return
    if not args.image:
        parser.error("a model image is needed unless --ratings-only")
    image = pathlib.Path(args.image).resolve()
    out = pathlib.Path(args.out) if args.out else models_dir() / "harness" / image.stem
    out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        tools = build_tools(tmp)
        model_rows, bank_rows, dump = run_blocks(image, tools, ref, args.blocks)
        held_metrics = held_row(ref, tools["form"])
    summary = {"image": str(image), "blocks": args.blocks,
               "training_lines": len(training), "held_lines": len(held),
               "model": spread(model_rows), "bank": spread(bank_rows), "held": held_metrics,
               "ceiling": ceiling, "ratings": args.ratings, "proxies": scores, "pairs_needed_70_30": pairs_needed()}
    (out / "summary.json").write_text(json.dumps(summary, indent=1, ensure_ascii=False),
                                      encoding="utf-8")
    with open(out / "texts.tsv", "w", encoding="utf-8") as f:
        f.write("row\tblock\tgroup\tevent\tlevels\tseed\tend\tgate_or_lead\ttext\n")
        for r in dump:
            f.write("\t".join(map(str, r)) + "\n")
    print(report(summary))
    print(f"\nClassifier ceilings on {len(held)} held-out lines: "
          + ", ".join(f"{k} {100 * v:.1f} %" for k, v in ceiling.items()))
    if scores:
        print(ratings_report(scores))
    print(f"\nDecided blind pairs for a two-sided sign test to find a 70/30 preference "
          f"(alpha 0.05, power 0.8): {summary['pairs_needed_70_30']}")
    print(f"\nWritten: {out}")


if __name__ == "__main__":
    main()
