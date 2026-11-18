#!/usr/bin/env python3
"""Exports a trained text model to the kernel's image format (add-text-model,
design.md Decision 11): header version 1, group-wise 4-bit tensors with fp16
scales, fp32 norm weights, separate output head, in the tensor order of
apps/mainframe/src/slm/llm.h (llm_load). Run it with the Python of
/.venv-text-model/.

Usage:
  text_model_export.py <checkpoint.pt> <model.bin> [--group 32]
Prints the image size and its SHA-256.
"""

import argparse
import copy
import hashlib
import pathlib
import struct
import sys

import numpy as np
import torch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model  # noqa: E402

MAGIC = 0x00454C50
VERSION = 1
HEADER_BYTES = 56
FLAGS = 0  # separate output head


def quantize(weight, group):
    """weight [rows, cols] float -> (codes int8 [rows, cols] in -7..7,
    scales float16 [rows, groups])."""
    w = weight.detach().to(torch.float32).cpu().numpy()
    rows, cols = w.shape
    groups = (cols + group - 1) // group
    codes = np.zeros((rows, cols), dtype=np.int8)
    scales = np.zeros((rows, groups), dtype=np.float16)
    for g in range(groups):
        block = w[:, g * group:(g + 1) * group]
        peak = np.abs(block).max(axis=1)
        scale = (peak / 7.0).astype(np.float16)
        scale[scale == 0] = np.float16(6e-8)  # smallest fp16 step instead of zero
        q = np.rint(block / scale.astype(np.float32)[:, None])
        codes[:, g * group:(g + 1) * group] = np.clip(q, -7, 7).astype(np.int8)
        scales[:, g] = scale
    return codes, scales


def dequantize(codes, scales, group):
    rows, cols = codes.shape
    full = np.repeat(scales.astype(np.float32), group, axis=1)[:, :cols]
    return codes.astype(np.float32) * full


def pack(codes, scales, group):
    """Group prefix, row-aligned nibbles (value + 8, low nibble first), fp16 scales."""
    rows, cols = codes.shape
    nibbles = (codes.astype(np.int16) + 8).astype(np.uint8)
    if cols % 2:
        nibbles = np.concatenate([nibbles, np.zeros((rows, 1), dtype=np.uint8)], axis=1)
    packed = (nibbles[:, 0::2] | (nibbles[:, 1::2] << 4)).astype(np.uint8)
    return struct.pack("<i", group) + packed.tobytes() + scales.astype("<f2").tobytes()


def tensors(model):
    """(kind, tensor) in llm_load order; kind 'q' quantised, 'f' fp32."""
    c = model.config
    out = [("q", model.tok_emb.weight), ("q", model.ple_model_proj.weight),
           ("f", model.ple_proj_norm), ("q", model.ple_table.weight)]
    for layer in model.layers:
        out += [("f", layer.attn_norm), ("q", layer.qkv.weight), ("q", layer.attn_proj.weight),
                ("f", layer.ffn_norm), ("q", layer.gate.weight), ("q", layer.up.weight),
                ("q", layer.down.weight), ("q", layer.ple_gate.weight),
                ("q", layer.ple_proj.weight), ("f", layer.ple_norm)]
    out += [("f", model.out_norm), ("q", model.head.weight)]
    assert len(out) == 4 + 10 * c["layers"] + 2
    return out


def image_bytes(model, group):
    c = model.config
    header = struct.pack("<IIII", MAGIC, VERSION, HEADER_BYTES, FLAGS)
    header += struct.pack("<II", c["vocab_in"], c["vocab_out"])
    header += struct.pack("<7i", c["dim"], c["layers"], c["heads"], c["ffn"], c["ple"],
                          c["seq_len"], group)
    header += struct.pack("<f", text_model.ROPE_THETA)
    assert len(header) == HEADER_BYTES
    body = bytearray(header)
    for kind, tensor in tensors(model):
        if kind == "q":
            codes, scales = quantize(tensor, group)
            body += pack(codes, scales, group)
        else:
            body += tensor.detach().to(torch.float32).cpu().numpy().astype("<f4").tobytes()
    return bytes(body)


def dequantized_copy(model, group):
    """The float model with every quantised tensor replaced by its 4-bit value,
    so its greedy output should equal the kernel's on the exported image."""
    twin = copy.deepcopy(model)
    with torch.no_grad():
        for kind, tensor in tensors(twin):
            if kind == "q":
                codes, scales = quantize(tensor, group)
                tensor.copy_(torch.from_numpy(dequantize(codes, scales, group)))
    twin.eval()
    return twin


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("checkpoint")
    parser.add_argument("image")
    parser.add_argument("--group", type=int, default=text_model.GROUP)
    args = parser.parse_args()
    model, _ = text_model.load(args.checkpoint)
    data = image_bytes(model, args.group)
    path = pathlib.Path(args.image)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(f"{path}: {len(data)} bytes, sha256 {hashlib.sha256(data).hexdigest()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
