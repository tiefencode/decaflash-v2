#!/usr/bin/env python3
"""Host check of the ported kernel against the reference "Barista" model.

Language model prototype, docs/slm-prototype.md. Encodes an ASCII question with
the model's byte-level BPE tokenizer, runs apps/mainframe/tools/slm_generate.cpp
on the model image and prints the answer as words plus the raw ids, which serve
as the fixed input and reference output for the device.

Usage: slm_barista.py <folder with model.bin, tokenizer.json, vocab.json,
       layout.json> "<question>" [<packed.bin>]
With a third argument it also writes the model image followed by the prompt
block of apps/mainframe/src/slm/slm_prompt.h (prompt, class table, the output
classes of this host run); that file is what gets flashed to the model partition.
Needs a C++ compiler (CXX or c++). The model files are not part of the repo.
"""
import json
import os
import re
import struct
import subprocess
import sys
import tempfile

# GPT-2 pre-tokenisation, restricted to ASCII input.
PIECES = re.compile(r"'s|'t|'re|'ve|'m|'ll|'d| ?[A-Za-z]+| ?[0-9]+| ?[^\sA-Za-z0-9]+|\s+(?!\S)|\s+")


def encode(tokenizer, text):
    if any(ord(c) >= 0x80 for c in text):
        sys.exit("ASCII input only")
    vocab = tokenizer["model"]["vocab"]
    merges = tokenizer["model"]["merges"]
    rank = {tuple(m.split(" ")) if isinstance(m, str) else tuple(m): i for i, m in enumerate(merges)}
    ids = []
    for piece in PIECES.findall(text):
        # Byte-level alphabet: space and control bytes are shifted by 0x100.
        parts = [chr(b + 0x100) if b <= 0x20 or b == 0x7F else chr(b) for b in piece.encode("ascii")]
        while len(parts) > 1:
            best = min(range(len(parts) - 1), key=lambda i: rank.get((parts[i], parts[i + 1]), 1 << 30))
            if (parts[best], parts[best + 1]) not in rank:
                break
            parts[best:best + 2] = [parts[best] + parts[best + 1]]
        ids.extend(vocab[p] for p in parts)
    return ids


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    folder, question = sys.argv[1], sys.argv[2]
    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    with open(os.path.join(folder, "tokenizer.json")) as f:
        tokenizer = json.load(f)
    with open(os.path.join(folder, "vocab.json")) as f:
        words = [t["token"] for t in json.load(f)["tokens"]]
    with open(os.path.join(folder, "layout.json")) as f:
        out2in = json.load(f)["out2in"]
    prompt = encode(tokenizer, question)
    with tempfile.TemporaryDirectory() as tmp:
        binary = os.path.join(tmp, "slm_generate")
        subprocess.run(
            [os.environ.get("CXX", "c++"), "-std=c++17", "-O2", "-I", os.path.join(repo, "apps/mainframe/src"),
             os.path.join(repo, "apps/mainframe/tools/slm_generate.cpp"), "-o", binary],
            check=True)
        table = os.path.join(tmp, "out2in.txt")
        with open(table, "w") as f:
            f.write("\n".join(str(i) for i in out2in))
        result = subprocess.run(
            [binary, os.path.join(folder, "model.bin"), table, str(words.index("<bos>")),
             str(words.index("<eos>")), "48"] + [str(i) for i in prompt],
            check=True, capture_output=True, text=True)
    classes = [int(line) for line in result.stdout.split()]
    answer = ""
    for c in classes:
        word = words[c]
        answer += word if (len(word) == 1 and word in ".,:;?") or not answer else " " + word
    print("question:      ", question)
    print("prompt ids:    ", " ".join(str(i) for i in prompt))
    print("bos input id:  ", out2in[words.index("<bos>")])
    print("output classes:", " ".join(str(c) for c in classes))
    print("answer:        ", answer)
    if len(sys.argv) == 4:
        bos, eos = out2in[words.index("<bos>")], words.index("<eos>")
        block = [0x504D4C53, 1, bos, eos, len(prompt), len(out2in), len(classes)] + prompt + out2in + classes
        with open(os.path.join(folder, "model.bin"), "rb") as f:
            image = f.read()
        with open(sys.argv[3], "wb") as f:
            f.write(image + struct.pack("<%dI" % len(block), *block))
        print("packed:        ", sys.argv[3], len(image) + 4 * len(block), "bytes")


if __name__ == "__main__":
    main()
