# CLAUDE.md

Working rules for the Decaflash V2 repository.

This file holds rules only. Current state belongs in [README.md](README.md), decisions and analysis in [docs/project-context.md](docs/project-context.md), and product behaviour in the docs now and in `openspec/specs` later.

## Rule candidates

- Do not add a rule to this file on your own. Follow the `rule-candidates` skill; the candidates are in [ideas.md](ideas.md).

## Orientation

- [README.md](README.md) has the current state and the commands, and [workers/decaflash/README.md](workers/decaflash/README.md) covers the Worker. `docs/archive/` holds historical documents.
- [docs/project-context.md](docs/project-context.md) is large, so read only the section the task needs. Parts of it are older than the code. When the document and the code disagree, the code and `git log` win; report the mismatch instead of following the document.
- Do not read or hand-edit `apps/mainframe/src/panel_text_sprites.h`. It is a sprite atlas of about 7,000 lines, converted once from an image.

## Boundaries

- Work only in this V2 repository. Do not modify the old Decaflash project and do not share mutable source files with it.
- `reference/m5atom_controller` is a local, ignored copy of the historical M5Atom controller. Read it when needed; never build, edit or version it.
- The show must work without an access point, internet or AI services.
- ESP-NOW runs on Wi-Fi channel 1, and a Wi-Fi association can move the radio to another channel. Any Wi-Fi feature needs the user's decision on channel policy before implementation.

## Git workflow

- Work in a Git worktree on its own branch. Small changes may go directly on `main`.
- Commit finished steps in the worktree without asking. Push only when the user asks.
- Merge a worktree branch into `main` only after the user agrees.
- Write every commit message as a Conventional Commit, `type(scope): summary`, in English.

## Radio identity

- V2 uses protocol magic `0x44434632` (DCF2); V1 uses `0x4443464C`. V1 and V2 must reject each other's packets for every message type. A new message type needs a case in `tests/protocol_identity.cpp`.

## Secrets, cloud and paid services

- `include/wifi_credentials.h`, `include/cloud_config.h` and `workers/decaflash/.dev.vars` stay local and ignored. Never read out, print or commit their contents.
- Do not deploy the Worker, and do not reuse or overwrite the V1 Cloudflare Worker, its bindings or its KV data.
- Do not call paid external services (AudD, OpenAI) unless the user asks for it.

## Dependencies

- Versions are pinned in `platformio.ini`. Change a pin only as its own step: one dependency at a time, built, flashed to a test device and checked there.

## Verification

- After a change to firmware, shared headers or tests, run `sh tests/run_all.sh` and build each affected environment with `pio run -e <env>`. A change under `shared/include` affects `mainframe` and `node`, and both device types then need flashing.
- A successful build or host test is not a device test. Report separately what was verified on hardware and what was not. Do not name a cause or claim a fix for behaviour seen on hardware without a measurement on the device.
- Flashing (`pio run -t upload`) needs the user's explicit approval. Reading the serial port does not.
- Until the end-to-end test with a flashed V2 Node has run on the device (start, scene change, clock, isolation from V1), do not start new feature work on radio or Node output. Remove this rule once the test has passed.
- Find the serial port with `pio device list`. `pio device monitor` needs an interactive terminal; without a TTY, use a plain read-only serial client and do not change the firmware to get output.
- The `mainframe-eye-*`, `mainframe-beat-tracker` and `mainframe-*-capture` environments are for measurement only. Measure with audio analysis running, and flash the normal `mainframe` build again afterwards.
- Do not replace the productive audio or tempo path without comparing old and new behaviour on the device.
