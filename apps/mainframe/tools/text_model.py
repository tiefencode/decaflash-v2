#!/usr/bin/env python3
"""Training of the on-device text model (add-text-model, design.md Decisions 9-11).

The network mirrors apps/mainframe/src/slm/llm.h op for op (PLE TinyLM: split-half
RoPE, erf-GELU, SiLU-SwiGLU, RMSNorm, per-layer embeddings), with a separate
output head over the output tokens of text_model_vocab.py. Run it with the
Python of /.venv-text-model/ (apps/mainframe/tools/text_model_requirements.txt).

A training example is (event, five mood levels, text), the text written as
"<emoji> <body>". Its token sequence is
  <START> <EVENT> <ENERGY_x> <ANNOYANCE_x> <ATTENTION_x> <LONELINESS_x> <DEPRESSION_x>
  emoji, words and punctuation ..., <END>
and the loss covers the predictions from the last mood token on: the emoji,
every word or punctuation mark and the end (word tokens, design.md Decision 6).

Usage:
  text_model.py train --layers 3 --dim 64 --seed 1 --smoke --out models/text-model/smoke.pt
  text_model.py train --layers 3 --dim 64 --seed 1 \
      --corpus apps/mainframe/text_model/corpus --out models/text-model/3x64.pt
The corpus layout is described in text_model_corpus.py.

Pretraining (design.md Decision 14): --pretrain trains on general German text,
one sentence per line (text_model_pretrain_data.py), as
  <START> words and punctuation ..., <END>
with the loss on every token and the end. A word outside the vocabulary cuts a
sentence there; runs of at least three known tokens are kept, and only the run
that reaches the sentence end gets <END>. A run longer than the sequence is cut
into parts without an end. --device mps trains on the GPU of the Mac. --init then
starts the corpus training from that checkpoint (same size):
  text_model.py train --layers 4 --dim 96 --seed 1 --steps 6000 \
      --pretrain models/text-model/pretrain/text.txt --out models/text-model/4x96-pre.pt
  text_model.py train --layers 4 --dim 96 --seed 1 --steps 1200 --lr 1e-3 \
      --init models/text-model/4x96-pre.pt --corpus apps/mainframe/text_model/corpus \
      --out models/text-model/4x96-pre-ft.pt

Lead-only conditioning (change improve-text-model, step 3): --lead-only keeps
the condition format but sets every mood level to middle except the line's lead
value (the value the line was written for, as the text bank groups it), which
is low or high. The firmware request does the same with the lead value of the
mood (TextBank::leadFor).

Verse text model: --words apps/mainframe/text_model/verse_words.txt uses that
closed word list (text_model_verse_data.py vocab) instead of the classic corpus
and book words; the list is stored in the checkpoint. --checkpoint-every N also
saves <out>-<step>.pt every N steps (steps counted from 1), and the validation
of a pretraining run is logged per token and per character (characters of the
text the target tokens spell, plus one for each <END>):
  text_model.py train --layers 6 --dim 128 --seed 1 --steps 10000 --device mps \
      --words apps/mainframe/text_model/verse_words.txt \
      --pretrain models/text-model/verse/text.txt --checkpoint-every 1000 \
      --out models/text-model/verse-w2500-6x128-pre.pt
"""

import argparse
import math
import pathlib
import random
import sys

import torch
import torch.nn as nn
import torch.nn.functional as F

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_corpus  # noqa: E402
import text_model_vocab as vocab_module  # noqa: E402

REPO = vocab_module.REPO
RMS_EPS = 1e-6
ROPE_THETA = 10000.0
MAX_OUTPUT_PIECES = 48  # loop bound for generation; the real limit is MAX_TEXT_CHARACTERS
MAX_TEXT_CHARACTERS = vocab_module.MAX_TEXT_CHARACTERS

# Settings per size of the growth ladder (design.md Decision 9). Head width 32,
# FFN three times the dimension and PLE width equal to the dimension, as in the
# reference image of the prototype (dim 128, 4 heads, ffn 384, ple 128).
SIZES = {
    (3, 64): dict(heads=2, ffn=192, ple=64),
    (4, 96): dict(heads=3, ffn=288, ple=96),
    (6, 128): dict(heads=4, ffn=384, ple=64),  # PLE 64 so 8,000 words fit (design.md Decision 6)
}
SEQ_LEN = 64   # 7 condition tokens + output tokens + end, rounded up
GROUP = 32     # 4-bit quantisation group


class Vocab:
    def __init__(self, words_file=None):
        self.words_file = words_file
        self.tokens = vocab_module.build_vocab(words_file)
        n = vocab_module.counts(self.tokens)
        self.output = n["output"]
        self.input = n["input"]
        self.start = n["start"]
        self.first_word = n["first_word"]
        self.first_event = n["first_event"]
        self.first_mood = n["first_mood"]
        self.first_emoji = n["first_emoji"]
        self.end = 0
        self.by_text = {t["text"]: t["id"] for t in self.tokens if 0 < t["id"] < self.output}
        self.events = vocab_module.EVENTS
        self.emoji = sorted((t["text"] for t in self.tokens if t["kind"] == "emoji"),
                            key=len, reverse=True)

    def encode_body(self, body):
        """Body text -> ids of its words and punctuation; KeyError on an unknown word."""
        return [self.by_text[token] for token in vocab_module.split_body(body)]

    def encode_text(self, text):
        """'<emoji> <body>' -> output ids: emoji, words and punctuation."""
        emoji = next((e for e in self.emoji if text.startswith(e)), None)
        if emoji is None:
            raise ValueError(f"text does not start with a curated emoji: {text!r}")
        try:
            body = self.encode_body(text[len(emoji):].strip())
        except KeyError as error:
            raise ValueError(f"word not in the vocabulary: {error} in {text!r}") from None
        return [self.by_text[emoji]] + body

    def cut(self, ids):
        """Output ids up to the hard length limit (design.md Decision 7)."""
        return vocab_module.cut(self.tokens, ids)

    def condition(self, event, moods):
        """event name (EVENTS), moods: five levels 0 low, 1 middle, 2 high."""
        ids = [self.start, self.first_event + self.events.index(event)]
        ids += [self.first_mood + 3 * value + level for value, level in enumerate(moods)]
        return ids

    def decode(self, ids):
        """Output ids -> '<emoji> <body>', cut at the hard length limit."""
        return vocab_module.decode(self.tokens, ids)


def fixed_text_examples():
    """Today's fixed reaction texts per event, in the model's order."""
    return text_model_corpus.fixed_texts()


def smoke_examples(seed, repeats=8):
    """Every fixed text with random mood levels, `repeats` times."""
    rng = random.Random(seed)
    examples = []
    for event, texts in fixed_text_examples().items():
        for text in texts:
            for _ in range(repeats):
                examples.append((event, [rng.randrange(3) for _ in range(5)], text))
    return examples


def lead_only(examples):
    """Examples with every level middle except the line's lead value (text bank
    grouping): 12 events x 10 lead values as conditions."""
    import text_bank
    lead_of = {}
    for (e, l), texts in text_bank.build().items():
        for panel in texts:
            words, _, emoji = panel.rpartition(" ")
            lead_of[(vocab_module.EVENTS[e], f"{emoji} {words}")] = l
    out = []
    for event, _, text in examples:
        lead = lead_of[(event, text)]
        levels = [1] * 5
        levels[lead // 2] = 0 if lead % 2 == 0 else 2
        out.append((event, levels, text))
    return out


def rmsnorm(x, weight):
    return weight * x * torch.rsqrt(x.pow(2).mean(-1, keepdim=True) + RMS_EPS)


class Layer(nn.Module):
    def __init__(self, dim, heads, ffn, ple):
        super().__init__()
        self.heads = heads
        self.attn_norm = nn.Parameter(torch.ones(dim))
        self.qkv = nn.Linear(dim, 3 * dim, bias=False)
        self.attn_proj = nn.Linear(dim, dim, bias=False)
        self.ffn_norm = nn.Parameter(torch.ones(dim))
        self.gate = nn.Linear(dim, ffn, bias=False)
        self.up = nn.Linear(dim, ffn, bias=False)
        self.down = nn.Linear(ffn, dim, bias=False)
        self.ple_gate = nn.Linear(dim, ple, bias=False)
        self.ple_proj = nn.Linear(ple, dim, bias=False)
        self.ple_norm = nn.Parameter(torch.ones(dim))

    def forward(self, x, ple_l, cos, sin):
        b, t, d = x.shape
        dh = d // self.heads
        q, k, v = self.qkv(rmsnorm(x, self.attn_norm)).split(d, dim=-1)
        q = q.view(b, t, self.heads, dh).transpose(1, 2)
        k = k.view(b, t, self.heads, dh).transpose(1, 2)
        v = v.view(b, t, self.heads, dh).transpose(1, 2)
        q = rope(q, cos, sin)
        k = rope(k, cos, sin)
        att = F.scaled_dot_product_attention(q, k, v, is_causal=True)
        x = x + self.attn_proj(att.transpose(1, 2).reshape(b, t, d))
        h = rmsnorm(x, self.ffn_norm)
        x = x + self.down(F.silu(self.gate(h)) * self.up(h))
        g = F.gelu(self.ple_gate(x)) * ple_l
        return x + rmsnorm(self.ple_proj(g), self.ple_norm)


def rope(x, cos, sin):
    half = x.shape[-1] // 2
    x1, x2 = x[..., :half], x[..., half:]
    return torch.cat([x1 * cos - x2 * sin, x2 * cos + x1 * sin], dim=-1)


class TextModel(nn.Module):
    def __init__(self, layers, dim, heads, ffn, ple, vocab_in, vocab_out, seq_len=SEQ_LEN):
        super().__init__()
        self.config = dict(layers=layers, dim=dim, heads=heads, ffn=ffn, ple=ple,
                           vocab_in=vocab_in, vocab_out=vocab_out, seq_len=seq_len)
        self.tok_emb = nn.Embedding(vocab_in, dim)
        self.ple_model_proj = nn.Linear(dim, layers * ple, bias=False)
        self.ple_proj_norm = nn.Parameter(torch.ones(ple))
        self.ple_table = nn.Embedding(vocab_in, layers * ple)
        self.layers = nn.ModuleList(Layer(dim, heads, ffn, ple) for _ in range(layers))
        self.out_norm = nn.Parameter(torch.ones(dim))
        self.head = nn.Linear(dim, vocab_out, bias=False)
        for p in self.parameters():
            if p.dim() == 2:
                nn.init.normal_(p, std=0.02)
        for p in [self.tok_emb.weight, self.ple_table.weight]:
            nn.init.normal_(p, std=1.0 / math.sqrt(dim))

    def forward(self, tokens):
        c = self.config
        b, t = tokens.shape
        layers, dim, ple = c["layers"], c["dim"], c["ple"]
        x = self.tok_emb(tokens)
        proj = self.ple_model_proj(x) / math.sqrt(dim)
        proj = rmsnorm(proj.view(b, t, layers, ple), self.ple_proj_norm)
        table = self.ple_table(tokens).view(b, t, layers, ple) * math.sqrt(ple)
        ple_in = (proj + table) * 0.70710678
        dh = dim // c["heads"]
        i = torch.arange(dh // 2, device=tokens.device, dtype=torch.float32)
        freq = ROPE_THETA ** (-2.0 * i / dh)
        pos = torch.arange(t, device=tokens.device, dtype=torch.float32)
        angle = pos[:, None] * freq[None, :]
        cos, sin = torch.cos(angle), torch.sin(angle)
        for index, layer in enumerate(self.layers):
            x = layer(x, ple_in[:, :, index, :], cos, sin)
        return self.head(rmsnorm(x, self.out_norm))


def build_model(layers, dim, vocab):
    size = SIZES.get((layers, dim))
    if size is None:
        size = dict(heads=max(1, dim // 32), ffn=3 * dim, ple=dim)
    return TextModel(layers, dim, size["heads"], size["ffn"], size["ple"],
                     vocab.input, vocab.output)


def sequences(examples, vocab):
    """Input ids and targets (-100 where no loss) per example."""
    rows = []
    for event, moods, text in examples:
        cond = vocab.condition(event, moods)
        out = vocab.encode_text(text)
        if len(vocab.cut(out)) < len(out) or len(cond) + len(out) + 1 > SEQ_LEN:
            raise ValueError(f"text longer than the hard length limit: {text!r}")
        inputs = cond + out
        targets = [-100] * (len(cond) - 1) + out + [vocab.end]
        rows.append((inputs, targets))
    return rows


def plain_sequences(lines, vocab, seq_len=SEQ_LEN):
    """Input ids and targets for pretraining sentences (module docstring);
    a word outside the vocabulary cuts a sentence there (runs of at least three
    known tokens are kept; only the run that reaches the sentence end gets <END>)."""
    rows = []
    room = seq_len - 1  # tokens after <START>
    for line in lines:
        try:
            pieces = vocab_module.split_body(line)
        except ValueError:
            continue
        # Runs of known tokens; a word outside the list ends a run without <END>.
        runs, run = [], []
        for piece in pieces:
            if piece in vocab.by_text:
                run.append(vocab.by_text[piece])
            else:
                runs.append((run, False))
                run = []
        runs.append((run, True))
        for ids, complete in runs:
            if len(ids) < 3:
                continue
            for start in range(0, len(ids), room):
                part = ids[start:start + room]
                if complete and start + room >= len(ids) and len(part) < room:
                    rows.append(([vocab.start] + part, part + [vocab.end]))
                elif len(part) > 1:
                    rows.append(([vocab.start] + part[:-1], part))
    return rows


def read_pretrain(path, fraction=0.02, seed="pretrain"):
    """(training, validation) sentences; every 50th line held out."""
    lines = [l for l in pathlib.Path(path).read_text(encoding="utf-8").splitlines() if l]
    step = round(1 / fraction)
    return ([l for i, l in enumerate(lines) if i % step],
            [l for i, l in enumerate(lines) if not i % step])


def batches(rows, batch_size, rng):
    order = list(range(len(rows)))
    while True:
        rng.shuffle(order)
        for start in range(0, len(order) - batch_size + 1, batch_size):
            chunk = [rows[i] for i in order[start:start + batch_size]]
            width = max(len(inp) for inp, _ in chunk)
            inputs = torch.zeros(len(chunk), width, dtype=torch.long)
            targets = torch.full((len(chunk), width), -100, dtype=torch.long)
            for r, (inp, tgt) in enumerate(chunk):
                inputs[r, :len(inp)] = torch.tensor(inp)
                targets[r, :len(tgt)] = torch.tensor(tgt)
            yield inputs, targets


def row_characters(rows, vocab):
    """Characters each row's targets spell: the joined text pieces plus one for <END>."""
    out = []
    for _, targets in rows:
        ids = [i for i in targets if i != -100]
        pieces = [vocab.tokens[i]["text"] for i in ids if i != vocab.end]
        out.append(len(vocab_module.join_body(pieces)) + ids.count(vocab.end))
    return out


@torch.no_grad()
def evaluate(model, rows, batch_size=64, per_row=False):
    """Mean loss over rows (validation), in eval mode."""
    was_training = model.training
    model.eval()
    total, count = 0.0, 0
    for start in range(0, len(rows), batch_size):
        chunk = rows[start:start + batch_size]
        width = max(len(inp) for inp, _ in chunk)
        inputs = torch.zeros(len(chunk), width, dtype=torch.long)
        targets = torch.full((len(chunk), width), -100, dtype=torch.long)
        for r, (inp, tgt) in enumerate(chunk):
            inputs[r, :len(inp)] = torch.tensor(inp)
            targets[r, :len(tgt)] = torch.tensor(tgt)
        device = next(model.parameters()).device
        inputs, targets = inputs.to(device), targets.to(device)
        logits = model(inputs)
        loss = F.cross_entropy(logits.reshape(-1, logits.shape[-1]), targets.reshape(-1),
                               ignore_index=-100, reduction="sum")
        total += loss.item()
        count += int((targets != -100).sum())
    model.train(was_training)
    if per_row:
        return total, count
    return total / max(1, count)


def validation_text(model, rows, characters):
    """'validation <per token>  per character <per character>' (nats)."""
    total, count = evaluate(model, rows, per_row=True)
    return (f"validation {total / max(1, count):.4f}  "
            f"per character {total / max(1, sum(characters)):.4f}")


def train(model, rows, steps, seed, lr=3e-3, batch_size=32, log=print, validation=None,
          characters=None, checkpoint_every=0, checkpoint=None):
    """characters: row_characters(validation) for the per-character loss;
    checkpoint(step): called after every checkpoint_every-th step (from 1)."""
    torch.manual_seed(seed)
    rng = random.Random(seed)
    batch_size = min(batch_size, len(rows))
    optimizer = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=0.01)
    schedule = torch.optim.lr_scheduler.OneCycleLR(optimizer, max_lr=lr, total_steps=steps)
    stream = batches(rows, batch_size, rng)
    model.train()
    loss = None
    for step in range(steps):
        inputs, targets = next(stream)
        device = next(model.parameters()).device
        inputs, targets = inputs.to(device), targets.to(device)
        logits = model(inputs)
        loss = F.cross_entropy(logits.reshape(-1, logits.shape[-1]), targets.reshape(-1),
                               ignore_index=-100)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        schedule.step()
        done = step + 1
        at_checkpoint = checkpoint_every and done % checkpoint_every == 0
        if log and (step % max(1, steps // 10) == 0 or step == steps - 1 or at_checkpoint):
            line = f"step {step:5d}  loss {loss.item():.4f}"
            if validation and characters:
                line += "  " + validation_text(model, validation, characters)
            elif validation:
                line += f"  validation {evaluate(model, validation):.4f}"
            log(line)
        if at_checkpoint and checkpoint:
            checkpoint(done)
            model.train()
    model.eval()
    return loss.item() if loss is not None else float("nan")


@torch.no_grad()
def greedy(model, condition, max_pieces=MAX_OUTPUT_PIECES, end=0):
    """Greedy output classes after the condition, as the host kernel picks them."""
    tokens = list(condition)
    out = []
    for _ in range(max_pieces + 1):
        logits = model(torch.tensor([tokens]))[0, -1]
        best = int(torch.argmax(logits))
        if best == end:
            break
        out.append(best)
        tokens.append(best)  # output ids equal input ids
    return out


def save(model, path, seed, extra=None):
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    torch.save({"config": model.config, "seed": seed, "state": model.state_dict(),
                "extra": extra or {}}, path)


def remap(model, tokens, vocab):
    """A copy of `model` for the current vocabulary: rows of the token tables
    (embedding, PLE table, output head) are carried over by token text, so a
    corpus edit that adds or removes words keeps a pretrained checkpoint
    usable; new tokens keep their fresh initialisation. Returns (model, kept)."""
    c = model.config
    fresh = TextModel(c["layers"], c["dim"], c["heads"], c["ffn"], c["ple"],
                      vocab.input, vocab.output, c["seq_len"])
    old = {text: i for i, text in enumerate(tokens)}
    pairs = [(i, old[t["text"]]) for i, t in enumerate(vocab.tokens) if t["text"] in old]
    state = model.state_dict()
    target = fresh.state_dict()
    for name, value in state.items():
        if name in ("tok_emb.weight", "ple_table.weight", "head.weight"):
            rows = vocab.output if name == "head.weight" else vocab.input
            for new, old_id in pairs:
                if new < rows and old_id < value.shape[0]:
                    target[name][new] = value[old_id]
        else:
            target[name] = value
    fresh.load_state_dict(target)
    return fresh, len(pairs)


def load(path):
    data = torch.load(path, map_location="cpu", weights_only=True)
    c = data["config"]
    model = TextModel(c["layers"], c["dim"], c["heads"], c["ffn"], c["ple"],
                      c["vocab_in"], c["vocab_out"], c["seq_len"])
    model.load_state_dict(data["state"])
    model.eval()
    return model, data


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    t = sub.add_parser("train")
    t.add_argument("--layers", type=int, required=True)
    t.add_argument("--dim", type=int, required=True)
    t.add_argument("--seed", type=int, required=True)
    t.add_argument("--steps", type=int, default=2000)
    t.add_argument("--lr", type=float, default=3e-3)
    t.add_argument("--batch", type=int, default=32)
    t.add_argument("--smoke", action="store_true", help="train on today's fixed texts")
    t.add_argument("--corpus", help="corpus directory (apps/mainframe/text_model/corpus)")
    t.add_argument("--pretrain", help="general text, one sentence per line")
    t.add_argument("--init", help="checkpoint to start from (pretrained model)")
    t.add_argument("--out", required=True)
    t.add_argument("--device", default="cpu", help="cpu or mps (GPU of the Mac)")
    t.add_argument("--words", help="closed word list (apps/mainframe/text_model/verse_words.txt)")
    t.add_argument("--lead-only", action="store_true",
                   help="condition on event and lead value only (non-lead levels middle)")
    t.add_argument("--checkpoint-every", type=int, default=0,
                   help="also save <out>-<step>.pt every N steps")
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    vocab = Vocab(args.words)
    if sum(map(bool, [args.smoke, args.corpus, args.pretrain])) != 1:
        parser.error("give one of --smoke, --corpus or --pretrain")
    validation = []
    if args.smoke:
        rows = sequences(smoke_examples(args.seed), vocab)
    elif args.corpus:
        examples, held = text_model_corpus.split(text_model_corpus.read_corpus(args.corpus))
        if args.lead_only:
            examples, held = lead_only(examples), lead_only(held)
            conditions = {(e, tuple(l)) for e, l, _ in examples}
            print(f"lead-only: {len(conditions)} conditions, "
                  f"{len(examples) / len(conditions):.1f} training lines each")
        validation = sequences(held, vocab)
        rows = sequences(examples, vocab)
        print(f"corpus: {len(examples)} training lines, {len(held)} validation lines")
    else:
        lines, held = read_pretrain(args.pretrain)
        rows, validation = plain_sequences(lines, vocab), plain_sequences(held, vocab)
        print(f"pretrain: {len(lines)} sentences ({len(rows)} rows), "
              f"{len(held)} validation sentences")
    if args.init:
        model, data = load(args.init)
        if (model.config["layers"], model.config["dim"]) != (args.layers, args.dim):
            parser.error(f"--init is {model.config['layers']} x {model.config['dim']}")
        tokens = data["extra"].get("tokens")
        current = [t["text"] for t in vocab.tokens]
        if tokens is None and (model.config["vocab_in"], model.config["vocab_out"]) != (
                vocab.input, vocab.output):
            parser.error("--init has another vocabulary and no token list to map it")
        if tokens is not None and tokens != current:
            model, kept = remap(model, tokens, vocab)
            print(f"init from {args.init}: vocabulary changed, {kept} of {len(current)} "
                  f"tokens carried over by text")
        else:
            print(f"init from {args.init}")
    else:
        model = build_model(args.layers, args.dim, vocab)
    model.to(args.device)
    characters = row_characters(validation, vocab) if args.pretrain else None
    source = "smoke" if args.smoke else str(args.corpus or args.pretrain)

    def extra_for(steps):
        extra = {"corpus": source, "steps": steps, "lead_only": bool(args.lead_only),
                 "tokens": [t["text"] for t in vocab.tokens]}
        if args.words:
            extra["words"] = str(args.words)
        if args.init:
            extra["init"] = str(args.init)
        if validation:
            total, count = evaluate(model, validation, per_row=True)
            extra["validation"] = total / max(1, count)
            if characters:
                extra["validation_per_character"] = total / max(1, sum(characters))
        return extra

    out = pathlib.Path(args.out)

    def checkpoint(step):
        path = out.with_name(f"{out.stem}-{step}{out.suffix}")
        device = next(model.parameters()).device
        extra = extra_for(step)
        model.to("cpu")
        save(model, path, args.seed, extra)
        model.to(device)
        print(f"saved {path}", flush=True)

    final = train(model, rows, args.steps, args.seed, lr=args.lr, batch_size=args.batch,
                  validation=validation, characters=characters,
                  checkpoint_every=args.checkpoint_every, checkpoint=checkpoint)
    extra = extra_for(args.steps)
    extra["loss"] = final
    model.to("cpu")
    save(model, args.out, args.seed, extra)
    print(f"saved {args.out}  loss {final:.4f}"
          + (f"  validation {extra['validation']:.4f}" if validation else "")
          + (f"  per character {extra['validation_per_character']:.4f}" if characters else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
