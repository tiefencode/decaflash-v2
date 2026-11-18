# Proposal

## Why

The creature's reactions to gestures and presence come from fixed lists of two to four texts per event, and they ignore its mood. The language model prototype (`docs/slm-prototype.md`) showed on the device that a small transformer runs on core 0 next to the show: 44 ms per step for 4 layers × dimension 96, no audio drops, no resets, no refused ESP-NOW packets, and with continuous generation every limit held the tolerant limit when the eye ran at 40 ms (measured 2026-11-17). The user wants the Mainframe's own on-device language model to write the reaction texts, starting small and growing only if text quality needs it (user decision, 2026-11-17).

## What Changes

- A small on-device text model writes the reaction texts for motion and presence events. It sees the event kind and the five mood values in three levels each, so the text answers the mood of that moment. It is only the creature's "mouth": mood, light, sounds, tempo and radio stay deterministic and are not influenced by it.
- The model computes on demand, only when an event asks for a text, on core 0. It writes the emoji first and the words after it; the panel shows the emoji at once and adds the text word by word.
- If no emoji arrives within a wait time, if the model image is missing, or while the panel is hidden (sleep, debug screen), the text comes from today's fixed list. Without a model image the show behaves as today, apart from the new emoji position.
- **BREAKING (layout):** every panel message, fixed texts and poems included, shows its emoji large and centred in the first row with the text below, instead of after the text.
- Display time and scrolling follow a growing text: the display time starts when the text is complete, and a text that grows past four rows moves up while it is written.
- Our own model: German, capital letters, today's tone; one token per word of the corpus or curated emoji (first planned per character; changed after a measured comparison); training texts written by a Claude Code session and reviewed by the user; training on the user's Mac. Start size 3 layers × dimension 64; the model grows one step (4 × 96, then 6 × 128) only if it fails the form check or the user's rating.
- First in an own product env with the model partition; after the device acceptance it moves into the normal `mainframe` build.
- The depression poems stay a fixed list; poems from the model are noted in `ideas.md` for later.

## Capabilities

### New Capabilities

- `text-model`: the on-device text model: when it runs, what it reads, what it may output, how a request is cancelled, and that it stays off without a valid image and never touches anything but the panel text.

### Modified Capabilities

- `message-panel`: model texts for motion and presence events with the fixed lists as fallback, emoji in the first row for all messages, word-by-word growth, display time from completion, scrolling while a text grows, no model use while the panel is hidden.
- `personality`: the mood snapshot gains one reader, the text model request; the mood rules are unchanged.

## Impact

- Mainframe code: `apps/mainframe/src/message_panel.{h,cpp}`, `eye_renderer.cpp` (panel drawing), `main.cpp` (requests, fallback), the prototype runtime in `apps/mainframe/src/slm/` (sampling, output masks, streaming, cancel, request interface), a new word tokenizer built from the corpus and `panel_glyphs`.
- Host tools and data: training and export scripts in Python (new local dependency, installed only after the user's yes), the training corpus as text files in the repo, the form check as a host test. Model images stay in an ignored local folder, as in the prototype.
- Build: new env with `apps/mainframe/partitions_slm.csv`; at the end the normal `mainframe` env takes that partition table (no second app slot for OTA, no SPIFFS; the Mainframe uses neither today). The model image is written with its own flash command.
- Not changed: `shared/include`, the radio protocol, the Node firmware, dependency pins, the mood rules. No access point, internet or external service at runtime.
- Docs: `README.md` (env, flashing the model image), `docs/project-context.md` (device results), `ideas.md` (poems later).
- Builds on the prototype's kernel and measurements; does not need its open tasks 7.2 / 7.3 (worker on core 1).
