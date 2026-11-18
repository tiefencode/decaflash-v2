#!/usr/bin/env python3
"""Try a text model by hand (add-text-model). Run it with the Python of
/.venv-text-model/.

  text_model_try.py <checkpoint.pt> [--image model.bin --generator text_model_generate]

Pick an event and five mood levels; it prints the greedy text (from the host
kernel on the image when given, otherwise from the 4-bit twin) and three
sampled texts from the 4-bit twin with temperature and top-k, as a preview of
the sampling the firmware will use. Temperature and top-k are not chosen yet.
"""

import argparse
import pathlib
import subprocess
import sys

import torch

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import text_model  # noqa: E402
import text_model_check  # noqa: E402
import text_model_export  # noqa: E402

NAMES = ["Getragen", "Klopfer", "Doppelklopfer", "3+ Klopfer", "Stoß", "Gedreht",
         "Hingestellt/umgedreht", "Geschüttelt", "Kommt davor", "Läuft vorbei",
         "Geht weg", "Steht weiter weg"]
VALUES = ["Energie", "Genervt", "Aufmerksam", "Einsam", "Depression"]


@torch.no_grad()
def sample(model, condition, temperature, top_k, generator, end=0, first_emoji=47):
    tokens = list(condition)
    out = []
    for step in range(text_model.MAX_OUTPUT_PIECES + 1):
        logits = model(torch.tensor([tokens]))[0, -1].clone()
        if step == 0:
            logits[:first_emoji] = float("-inf")       # first piece: an emoji
        else:
            logits[first_emoji:] = float("-inf")       # then text and end only
        values, indices = torch.topk(logits, top_k)
        probs = torch.softmax(values / temperature, dim=-1)
        best = int(indices[int(torch.multinomial(probs, 1, generator=generator))])
        if best == end:
            break
        out.append(best)
        tokens.append(best)
    return out


def ask(prompt, valid):
    while True:
        answer = input(prompt).strip()
        if answer in ("q", "quit", "exit"):
            raise SystemExit(0)
        if valid(answer):
            return answer
        print("  Ungültig, nochmal.")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("checkpoint")
    parser.add_argument("--image")
    parser.add_argument("--generator")
    parser.add_argument("--temperature", type=float, default=0.8)
    parser.add_argument("--top-k", type=int, default=8)
    args = parser.parse_args()

    vocab = text_model.Vocab()
    model, _ = text_model.load(args.checkpoint)
    twin = text_model_check.int8_activations(
        text_model_export.dequantized_copy(model, text_model.GROUP))
    generator = torch.Generator().manual_seed(1)

    print(f"Modell: {args.checkpoint}")
    print("Ereignisse: " + ", ".join(f"{i + 1} {n}" for i, n in enumerate(NAMES)))
    print("Stimmung als fünf Ziffern in der Reihenfolge " + ", ".join(VALUES)
          + " (0 niedrig, 1 mittel, 2 hoch), z. B. 21000. Beenden mit q.")
    while True:
        event = int(ask("\nEreignis (1-12): ",
                        lambda a: a.isdigit() and 1 <= int(a) <= 12)) - 1
        moods = ask("Stimmung (5 Ziffern 0-2): ",
                    lambda a: len(a) == 5 and all(ch in "012" for ch in a))
        levels = [int(ch) for ch in moods]
        condition = vocab.condition(vocab.events[event], levels)
        label = ", ".join(f"{VALUES[i]} {['niedrig', 'mittel', 'hoch'][l]}"
                          for i, l in enumerate(levels))
        print(f"  {NAMES[event]} | {label}")
        if args.image and args.generator:
            greedy = text_model_check.kernel_greedy(args.generator, args.image, condition)
            print(f"  Kernel, wahrscheinlichstes Zeichen: {vocab.decode(greedy)}")
        else:
            print(f"  4-Bit, wahrscheinlichstes Zeichen: "
                  f"{vocab.decode(text_model.greedy(twin, condition))}")
        for _ in range(3):
            ids = sample(twin, condition, args.temperature, args.top_k, generator,
                         first_emoji=vocab.first_emoji)
            print(f"  Gesampelt (T {args.temperature}, top-k {args.top_k}): "
                  f"{vocab.decode(ids)}")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyboardInterrupt, EOFError):
        print()
