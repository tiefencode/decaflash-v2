#!/usr/bin/env python3
"""Exporter check for the text model (add-text-model, task 4.2).

For fixed conditions (every event with five mood combinations) it compares the
greedy output pieces of
  - the float model,
  - its twin with the exported 4-bit weights and int8 activations, the
    arithmetic of the kernel (slm_kernel.h sets LLM_INT8_ACT),
  - the host kernel on the exported image (text_model_generate).
The kernel must equal the twin in every case; float against kernel shows what
quantisation changes and is reported.

Usage:
  text_model_check.py <checkpoint.pt> <model.bin> <text_model_generate> [--group 32]
Exit status 1 if the kernel and the twin differ in any case.
"""

import argparse
import pathlib
import subprocess
import sys

import torch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model  # noqa: E402
import text_model_export  # noqa: E402

MOOD_CASES = [
    [0, 0, 0, 0, 0],
    [1, 1, 1, 1, 1],
    [2, 2, 2, 2, 2],
    [2, 0, 1, 0, 2],
    [0, 2, 0, 2, 1],
]


def int8_activations(model):
    """Quantise every linear layer's input as quantize_act in llm.h does."""
    def hook(_module, args):
        x = args[0]
        peak = x.abs().amax(dim=-1, keepdim=True).clamp_min(1e-8)
        q = torch.clamp(torch.round(x * (127.0 / peak)), -127, 127)
        return (q * (peak / 127.0),)
    for module in model.modules():
        if isinstance(module, torch.nn.Linear):
            module.register_forward_pre_hook(hook)
    return model


def kernel_greedy(generator, image, condition):
    run = subprocess.run([generator, str(image), str(text_model.MAX_OUTPUT_PIECES)]
                         + [str(i) for i in condition],
                         capture_output=True, text=True, check=True)
    return [int(line) for line in run.stdout.split()]


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("checkpoint")
    parser.add_argument("image")
    parser.add_argument("generator")
    parser.add_argument("--group", type=int, default=text_model.GROUP)
    args = parser.parse_args()

    vocab = text_model.Vocab()
    model, _ = text_model.load(args.checkpoint)
    twin = int8_activations(text_model_export.dequantized_copy(model, args.group))

    cases = equal_twin = equal_float = equal_first = 0
    for event in vocab.events:
        for moods in MOOD_CASES:
            condition = vocab.condition(event, moods)
            float_out = text_model.greedy(model, condition)
            twin_out = text_model.greedy(twin, condition)
            kernel_out = kernel_greedy(args.generator, args.image, condition)
            cases += 1
            equal_twin += kernel_out == twin_out
            equal_float += kernel_out == float_out
            equal_first += bool(kernel_out) and bool(float_out) and kernel_out[0] == float_out[0]
            if kernel_out != twin_out:
                print(f"kernel differs from twin: {event} {moods}\n"
                      f"  twin   {vocab.decode(twin_out)}\n  kernel {vocab.decode(kernel_out)}")
    print(f"{cases} cases: kernel = twin {equal_twin}, kernel = float {equal_float}, "
          f"same first piece as float {equal_first}")
    return 0 if equal_twin == cases else 1


if __name__ == "__main__":
    sys.exit(main())
