#!/usr/bin/env python3
"""Confidence of the text model in its own texts (add-text-model, evaluation of
a confidence gate). Run it with the Python of /.venv-text-model/.

A confidence gate would reject a sampled text whose confidence value is below
a threshold, and the firmware would then show the text bank line instead. This
tool measures whether such a value separates usable from unusable texts.

  text_model_confidence.py score <checkpoint.pt> <texts.tsv> [--out values.tsv]
      texts.tsv: event, levels, text and optionally a mark (1 usable, 0 not)
      per line, tab separated. Prints the confidence values of every text for
      the float model and for the 4-bit twin, and, when marks are given, how
      well each value separates the marks (AUC and a threshold table).

  text_model_confidence.py sample <checkpoint.pt> [--cases cases.tsv] [--seed N]
                           [--repeat N] [--form-check <text_model_form_check>]
                           [--out texts.tsv]
      Samples one text per case (event, levels) from the 4-bit twin, as
      text_model_try.py does, and prints it with its confidence values, the
      form check result, whether it is a verbatim corpus line, and its words
      that are not in the corpus.

Confidence values, all from the log-probability of each chosen token (emoji,
words, punctuation; the end token only when the text was not cut at the length
limit):
  mean / min         under the sampling distribution (first-token mask, top-k,
                     temperature), which is what the firmware can compute
  raw mean / min     under the masked distribution at temperature 1 without
                     top-k; finite also for a token outside the top-k
  "body" variants    leave out the emoji token
"""

import argparse
import math
import pathlib
import subprocess
import sys
import tempfile

import torch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model  # noqa: E402
import text_model_check  # noqa: E402
import text_model_corpus as corpus  # noqa: E402
import text_model_export  # noqa: E402
import text_model_try  # noqa: E402  (its sample() is the sampling the user chose)
import text_model_vocab as vocab_module  # noqa: E402

CASES = vocab_module.REPO / "apps/mainframe/text_model/rating_cases.tsv"
VALUE_NAMES = ["mean", "mean_body", "min", "min_body",
               "raw_mean", "raw_mean_body", "raw_min", "raw_min_body"]


@torch.no_grad()
def token_log_probs(model, vocab, condition, ids, ended, temperature, top_k):
    """[(sampling log-probability, raw log-probability)] for every id of the
    text, plus the end token when `ended`. The sampling value is -inf for a
    token outside the top-k."""
    targets = list(ids) + ([vocab.end] if ended else [])
    logits = model(torch.tensor([list(condition) + list(ids)]))[0]
    out = []
    for step, target in enumerate(targets):
        row = logits[len(condition) - 1 + step].clone()
        if step == 0:
            row[:vocab.first_emoji] = float("-inf")
        else:
            row[vocab.first_emoji:] = float("-inf")
        raw = float(torch.log_softmax(row, dim=-1)[target])
        values, indices = torch.topk(row, top_k)
        sampled = torch.log_softmax(values / temperature, dim=-1)
        hit = (indices == target).nonzero()
        out.append((float(sampled[int(hit[0])]) if len(hit) else float("-inf"), raw))
    return out


def summarise(pairs):
    """Confidence values of one text from its per-token log-probabilities."""
    result = {}
    for prefix, column in (("", 0), ("raw_", 1)):
        every = [p[column] for p in pairs]
        body = every[1:] or every
        result[prefix + "mean"] = sum(every) / len(every)
        result[prefix + "mean_body"] = sum(body) / len(body)
        result[prefix + "min"] = min(every)
        result[prefix + "min_body"] = min(body)
    return result


def was_cut(vocab, text):
    """True when the text has no room for another piece, so the generation may
    have been stopped by the length limit instead of the end token."""
    body = text.split(" ", 1)[1] if " " in text else ""
    return len(body) >= vocab_module.MAX_TEXT_CHARACTERS - 1


def score_text(models, vocab, event, levels, text, temperature, top_k):
    condition = vocab.condition(event, levels)
    ids = vocab.encode_text(text)
    ended = not was_cut(vocab, text)
    return {name: summarise(token_log_probs(model, vocab, condition, ids, ended,
                                            temperature, top_k))
            for name, model in models.items()}


def auc(usable, unusable):
    """Probability that a usable text has a higher value than an unusable one."""
    wins = sum((a > b) + 0.5 * (a == b) for a in usable for b in unusable)
    return wins / (len(usable) * len(unusable))


def separation(rows, key):
    """AUC and, for every threshold between two neighbouring values, how many
    unusable texts are rejected and how many usable ones are lost."""
    usable = [r[key] for r in rows if r["mark"] == 1]
    unusable = [r[key] for r in rows if r["mark"] == 0]
    values = sorted(set(v for v in usable + unusable if math.isfinite(v)))
    table = []
    for low, high in zip(values, values[1:]):
        threshold = (low + high) / 2
        table.append((threshold, sum(v < threshold for v in unusable),
                      sum(v < threshold for v in usable)))
    return auc(usable, unusable), table


def load_models(checkpoint):
    twin = text_model_check.int8_activations(
        text_model_export.dequantized_copy(text_model.load(checkpoint)[0], text_model.GROUP))
    return {"float": text_model.load(checkpoint)[0], "twin": twin}


def read_cases(path):
    cases = []
    for line in pathlib.Path(path).read_text(encoding="utf-8").splitlines():
        if line.strip():
            parts = line.split("\t")
            cases.append((parts[0], parts[1], corpus.parse_levels(parts[1]), parts[2:]))
    return cases


def form_results(form_check, texts, training):
    """Form check result per text ("ok" or the failure), with the corpus as
    training texts for the copy check."""
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


def command_score(args):
    vocab = text_model.Vocab()
    models = load_models(args.checkpoint)
    rows = []
    for number, (event, levels_text, levels, rest) in enumerate(read_cases(args.texts), 1):
        scores = score_text(models, vocab, event, levels, rest[0],
                            args.temperature, args.top_k)
        row = {"number": number, "text": rest[0],
               "mark": int(rest[1]) if len(rest) > 1 and rest[1] != "" else None}
        for name, values in scores.items():
            for key, value in values.items():
                row[f"{name}_{key}"] = value
        rows.append(row)
    keys = [f"{m}_{v}" for m in models for v in VALUE_NAMES]
    header = "number\tmark\t" + "\t".join(keys) + "\ttext"
    lines = [header] + [
        f"{r['number']}\t{'' if r['mark'] is None else r['mark']}\t"
        + "\t".join(f"{r[k]:.3f}" for k in keys) + f"\t{r['text']}" for r in rows]
    print("\n".join(lines))
    if args.out:
        pathlib.Path(args.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    marked = [r for r in rows if r["mark"] is not None]
    if marked and 0 < sum(r["mark"] for r in marked) < len(marked):
        total_bad = sum(r["mark"] == 0 for r in marked)
        total_good = len(marked) - total_bad
        print(f"\nSeparation on {len(marked)} marked texts "
              f"({total_good} usable, {total_bad} not usable); AUC 0.5 = no separation")
        for key in keys:
            area, table = separation(marked, key)
            print(f"\n{key}: AUC {area:.2f}")
            if args.thresholds:
                for threshold, rejected, lost in table:
                    print(f"  below {threshold:7.3f}: rejects {rejected:2d} of {total_bad} "
                          f"not usable, loses {lost:2d} of {total_good} usable")
    return 0


def command_sample(args):
    vocab = text_model.Vocab()
    models = load_models(args.checkpoint)
    generator = torch.Generator().manual_seed(args.seed)
    cases = read_cases(args.cases) * args.repeat
    training = [text for _, _, text in corpus.read_corpus()]
    corpus_words = set(vocab_module.corpus_words())
    texts = []
    for event, _, levels, _ in cases:
        ids = text_model_try.sample(models["twin"], vocab.condition(event, levels),
                                    args.temperature, args.top_k, generator,
                                    first_emoji=vocab.first_emoji)
        texts.append(vocab.decode(ids))
    forms = (form_results(args.form_check, texts, training) if args.form_check
             else [""] * len(texts))
    copies = set(vocab_module.corpus_bodies())   # the copy check compares bodies

    def body_of(text):
        return text.split(" ", 1)[1] if " " in text else ""
    lines = []
    for (event, levels_text, levels, _), text, form in zip(cases, texts, forms):
        values = score_text(models, vocab, event, levels, text,
                            args.temperature, args.top_k)["twin"] if text else {}
        foreign = [w for w in vocab_module.split_body(body_of(text))
                   if w not in vocab_module.PUNCTUATION and w not in corpus_words]
        lines.append("\t".join([event, levels_text, text, form,
                                "copy" if body_of(text) in copies else "",
                                " ".join(foreign)]
                               + [f"{values.get(k, float('nan')):.3f}"
                                  for k in ("mean", "mean_body", "min", "min_body")]))
    print("\n".join(lines))
    print(f"{len(texts)} texts: {sum(f == 'ok' for f in forms)} pass the form check, "
          f"{sum(body_of(t) in copies for t in texts)} verbatim corpus lines, "
          f"{len(set(texts))} different texts")
    if args.out:
        pathlib.Path(args.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("score", "sample"):
        p = sub.add_parser(name)
        p.add_argument("checkpoint")
        if name == "score":
            p.add_argument("texts")
            p.add_argument("--thresholds", action="store_true",
                           help="print the threshold table of every value")
        else:
            p.add_argument("--cases", default=str(CASES))
            p.add_argument("--seed", type=int, default=2026)
            p.add_argument("--repeat", type=int, default=1)
            p.add_argument("--form-check")
        p.add_argument("--temperature", type=float, default=0.8)
        p.add_argument("--top-k", type=int, default=8)
        p.add_argument("--out")
    args = parser.parse_args()
    return command_score(args) if args.command == "score" else command_sample(args)


if __name__ == "__main__":
    sys.exit(main())
