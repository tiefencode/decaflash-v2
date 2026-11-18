#!/usr/bin/env python3
"""Training of the on-device text model (add-text-model, design.md Decisions 9-11).

The network mirrors apps/mainframe/src/slm/llm.h op for op (PLE TinyLM: split-half
RoPE, erf-GELU, SiLU-SwiGLU, RMSNorm, per-layer embeddings), with a separate
output head over the output tokens of text_model_vocab.py. Run it with the
Python of /.venv-text-model/ (apps/mainframe/tools/text_model_requirements.txt).

A training example is (event, five mood levels, text), the text written as
"<emoji> <words>". Its token sequence is
  <START> <EVENT> <ENERGY_x> <ANNOYANCE_x> <ATTENTION_x> <LONELINESS_x> <DEPRESSION_x>
  emoji, characters ..., <END>
and the loss covers the predictions from the last mood token on: the emoji,
every character and the end.

Usage:
  text_model.py train --layers 3 --dim 64 --seed 1 --smoke --out models/text-model/smoke.pt
The corpus reader (--corpus) follows the file layout agreed in task 5.1.
"""

import argparse
import math
import pathlib
import random
import re
import sys

import torch
import torch.nn as nn
import torch.nn.functional as F

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model_vocab as vocab_module  # noqa: E402

REPO = vocab_module.REPO
PANEL_CPP = REPO / "apps/mainframe/src/message_panel.cpp"
RMS_EPS = 1e-6
ROPE_THETA = 10000.0
MAX_OUTPUT_PIECES = 48  # text_model_form.h kMaxOutputPieces

# Settings per size of the growth ladder (design.md Decision 9). Head width 32,
# FFN three times the dimension and PLE width equal to the dimension, as in the
# reference image of the prototype (dim 128, 4 heads, ffn 384, ple 128).
SIZES = {
    (3, 64): dict(heads=2, ffn=192, ple=64),
    (4, 96): dict(heads=3, ffn=288, ple=96),
    (6, 128): dict(heads=4, ffn=384, ple=128),
}
SEQ_LEN = 64   # 7 condition tokens + 48 output pieces + end, rounded up
GROUP = 32     # 4-bit quantisation group


class Vocab:
    def __init__(self):
        self.tokens = vocab_module.build_vocab()
        n = vocab_module.counts(self.tokens)
        self.output = n["output"]
        self.input = n["input"]
        self.start = n["start"]
        self.first_event = n["first_event"]
        self.first_mood = n["first_mood"]
        self.first_emoji = n["first_emoji"]
        self.end = 0
        self.by_text = {t["text"]: t["id"] for t in self.tokens if t["id"] < self.output}
        self.events = vocab_module.EVENTS
        self.max_token_bytes = max(len(t.encode("utf-8")) for t in self.by_text)

    def encode_text(self, text):
        """'<emoji> <words>' -> output ids: emoji, characters (no reading space)."""
        ids = []
        at = 0
        while at < len(text):
            for length in range(min(8, len(text) - at), 0, -1):
                piece = text[at:at + length]
                if piece in self.by_text and piece != "<END>":
                    ids.append(self.by_text[piece])
                    at += length
                    break
            else:
                raise ValueError(f"character not in the vocabulary: {text[at]!r} in {text!r}")
        if not ids or ids[0] < self.first_emoji:
            raise ValueError(f"text does not start with a curated emoji: {text!r}")
        body = ids[1:]
        space = self.by_text[" "]
        while body and body[0] == space:
            body.pop(0)
        while body and body[-1] == space:
            body.pop()
        return [ids[0]] + body

    def condition(self, event, moods):
        """event name (EVENTS), moods: five levels 0 low, 1 middle, 2 high."""
        ids = [self.start, self.first_event + self.events.index(event)]
        ids += [self.first_mood + 3 * value + level for value, level in enumerate(moods)]
        return ids

    def decode(self, ids):
        """Output ids -> '<emoji> <words>' (the reading space after the emoji)."""
        text = "".join(self.tokens[i]["text"] for i in ids)
        if ids and ids[0] >= self.first_emoji:
            return self.tokens[ids[0]]["text"] + " " + text[len(self.tokens[ids[0]]["text"]):]
        return text


def fixed_text_examples():
    """Today's fixed reaction texts per event, in the model's order."""
    source = PANEL_CPP.read_text(encoding="utf-8")
    sets = {
        "MOVE": "kMove", "TAP": "kTap", "DOUBLE_TAP": "kDoubleTap", "MULTI_TAP": "kMultiTap",
        "IMPACT": "kImpact", "ROTATE": "kRotate", "REPOSITION": "kReposition",
        "SHAKE": "kShake", "ARRIVED_IN_FRONT": "kArrived", "WALKED_PAST": "kWalkedPast",
        "LEFT": "kLeft", "STANDS_FAR": "kStandsFar",
    }
    v = Vocab()
    emoji = sorted((t["text"] for t in v.tokens if t["kind"] == "emoji"), key=len, reverse=True)
    out = {}
    for event, name in sets.items():
        block = re.search(name + r"\[\] = \{(.*?)\};", source, re.S).group(1)
        texts = re.findall(r'u8"([^"]*)"', block)
        model_order = []
        for text in texts:
            last = next(e for e in emoji if text.endswith(e))
            model_order.append(last + " " + text[: -len(last)].rstrip())
        out[event] = model_order
    return out


def smoke_examples(seed, repeats=8):
    """Every fixed text with random mood levels, `repeats` times."""
    rng = random.Random(seed)
    examples = []
    for event, texts in fixed_text_examples().items():
        for text in texts:
            for _ in range(repeats):
                examples.append((event, [rng.randrange(3) for _ in range(5)], text))
    return examples


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
        if len(out) > MAX_OUTPUT_PIECES:
            raise ValueError(f"text longer than {MAX_OUTPUT_PIECES} pieces: {text!r}")
        inputs = cond + out
        targets = [-100] * (len(cond) - 1) + out + [vocab.end]
        rows.append((inputs, targets))
    return rows


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


def train(model, rows, steps, seed, lr=3e-3, batch_size=32, log=print):
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
        logits = model(inputs)
        loss = F.cross_entropy(logits.reshape(-1, logits.shape[-1]), targets.reshape(-1),
                               ignore_index=-100)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        schedule.step()
        if log and (step % max(1, steps // 10) == 0 or step == steps - 1):
            log(f"step {step:5d}  loss {loss.item():.4f}")
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
    t.add_argument("--out", required=True)
    args = parser.parse_args()

    torch.manual_seed(args.seed)
    vocab = Vocab()
    if not args.smoke:
        parser.error("only --smoke exists until the corpus layout is agreed (task 5.1)")
    rows = sequences(smoke_examples(args.seed), vocab)
    model = build_model(args.layers, args.dim, vocab)
    final = train(model, rows, args.steps, args.seed, lr=args.lr, batch_size=args.batch)
    save(model, args.out, args.seed, {"corpus": "smoke", "steps": args.steps, "loss": final})
    print(f"saved {args.out}  loss {final:.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
