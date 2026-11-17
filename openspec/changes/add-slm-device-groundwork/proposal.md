# Proposal

## Why

The creature's panel texts are fixed lists. A tiny on-device language model could generate them from mood, motion and presence, but nobody knows yet whether such a model can run on the Mainframe (AtomS3R, 8 MB flash, 8 MB PSRAM) without hurting beat detection, the show clock and ESP-NOW. Core 1 is already 75 % busy while a text is visible, with audio block gaps of up to 94 ms (measured on the device, 2026-11-17). This change builds and measures the groundwork on the device before any training data, training or generated text is planned.

## What Changes

- The eye draws at a lower frame rate in frames where the message panel is drawn over it. This becomes product behaviour in the normal `mainframe` build, independent of the language model. The rate is chosen by the user after a device measurement of several candidates.
- A new measurement build `mainframe-slm-capture` adds a language model runtime: a model image in its own flash partition, memory-mapped instead of copied, working memory in PSRAM, and generation in a low-priority background task on core 0 that yields regularly. Its output never reaches the panel, the Nodes or any show path.
- The measurement build uses its own partition table: the second app slot and SPIFFS are given up for one model partition (user decision). The normal `mainframe` build keeps `default_8MB.csv`.
- Placeholder models, in this order (user decision): first the reference project's English "Barista" image, to check that the ported runtime computes correctly; then locally generated random-weight images of the reference size and smaller sizes, for load measurements.
- Device measurements before and after every step, with the acceptance limit "not worse than today" (user decision). Core 0 alone is measured first, an extra worker on core 1 as a separate step (user decision).
- No trained German model, no training data, no generated panel text, no change to the message-panel or personality behaviour.

Open and blocking for the runtime part: whether the firmware stays on the pinned Arduino core 2.0.17 or moves to a newer core. The user wants this discussed in its own session; see design.md, Decision 8.

## Capabilities

### New Capabilities

- `language-model-runtime`: the on-device language model runtime of the Mainframe: where the model image lives, how it is loaded, where and with which priority it computes, and its isolation from the show. Later changes add what it generates and how texts reach the panel.

### Modified Capabilities

- `eye-iris`: adds a reduced frame rate for frames in which the message panel is drawn.

## Impact

- `apps/mainframe/src/eye_renderer.{h,cpp}`: frame interval depends on whether the panel is drawn (normal build).
- New `apps/mainframe/src/slm/` (runtime, compiled only with a build flag), a new partition table file, new env `mainframe-slm-capture` in `platformio.ini`, new host tests for the runtime kernel under `tests/`.
- Needs the loop-load instrumentation from branch `claude/loop-load-measurement-f3ac43` (env `mainframe-loop-load`, not merged); merging it needs the user's agreement.
- Third-party code: the inference code of https://github.com/slvDev/esp32-ai (MIT) is ported, with its licence notice. The Barista image is downloaded only after the user's yes at that step and is not committed.
- `shared/include`, the radio protocol and the Node firmware are not changed. No dependency pin changes in this change.
- Docs: `README.md` (new env), `docs/project-context.md` (measurement results), `openspec/specs/message-panel` and `personality` unchanged.
