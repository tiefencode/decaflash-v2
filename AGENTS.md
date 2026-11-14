# AGENTS.md

Working rules for every coding agent in the Decaflash V2 repository.

This file holds rules only. Current state belongs in [README.md](README.md), decisions and analysis in [docs/MAINFRAME_V2.md](docs/MAINFRAME_V2.md), and product behaviour in the docs now and in `openspec/specs` later. Do not add status notes, history or product behaviour here.

## Orientation

- Read [README.md](README.md) first for the current state and the commands.
- [docs/MAINFRAME_V2.md](docs/MAINFRAME_V2.md) holds decisions, code analysis and hardware limits. It is large, so read only the section the task needs. Parts of it are older than the code. When the document and the code disagree, the code and `git log` win; report the mismatch instead of following the document.
- Worker instructions are in [workers/decaflash/README.md](workers/decaflash/README.md). `docs/v1-outline.md` is historical.
- Entry points: Mainframe `apps/mainframe/src/main.cpp`, Node `apps/node/src/main.cpp`, protocol `shared/include/protocol.h`, ESP-NOW transport `shared/include/espnow_transport.h`.
- Do not read or hand-edit `apps/mainframe/src/panel_text_sprites.h`. It is an embedded sprite atlas of about 7,000 lines.
- The central device is called Mainframe in all versioned sources and documents.

## Boundaries

- Work only in this V2 repository. Do not modify the old Decaflash project and do not share mutable source files with it.
- `reference/m5atom_controller` is a local, ignored copy of the historical M5Atom controller. Read it when needed; never build, edit or version it. The original project and its Git history are canonical.
- Keep `apps/mainframe`, `apps/node` and `shared/include` as the structure. Both the Mainframe and the Node firmware may change.
- Take code over from V1 selectively, and work in small steps that can each be verified.
- The show must work without an access point, internet or AI services. Audio and Node communication take priority over display and cloud, and display or cloud load must not starve them.
- ESP-NOW runs on Wi-Fi channel 1 (`kWifiChannel`). A Wi-Fi association can move the radio to another channel, so any Wi-Fi feature needs the user's decision on channel policy before implementation.

## Git workflow

- Do all work in its own Git worktree on its own branch. Only small changes, such as a one-line doc fix, a `.gitignore` entry or rewording a commit message, may be committed directly on `main`.
- Merge a worktree branch into `main` only after the user agrees.
- Write every commit message as a Conventional Commit, `type(scope): summary`, in English.

## Radio identity

- V2 uses protocol magic `0x44434632` (DCF2); V1 uses `0x4443464C`. V1 and V2 must reject each other's packets for every message type.
- Every sender uses the shared V2 header from `protocol.h`, and every receive path checks it. A new message type needs a case in `tests/protocol_identity.cpp`.
- A change to the magic, payload version or packet layout affects both firmwares. Build `mainframe` and `node`, and say that both device types need flashing.

## Secrets, cloud and paid services

- `include/wifi_credentials.h`, `include/cloud_config.h` and `workers/decaflash/.dev.vars` stay local and ignored. Only their example files are tracked. Never read out, print or commit their contents.
- Deployment is not part of the current work. Do not deploy the Worker, and do not reuse or overwrite the V1 Cloudflare Worker, its bindings or its KV data. The Worker README describes the path for a later V2 deployment.
- Do not call paid external services (AudD, OpenAI) unless the user asks for it.

## Dependencies

- Platform and library versions are pinned in `platformio.ini`. Do not change a pin as part of unrelated work.
- Update one dependency at a time: build it, flash a designated test device with the user's approval, and verify the physical behaviour before changing the pin.
- FastLED is for Node output only.

## Verification

- Run `sh tests/run_all.sh` after every change to firmware, shared headers or tests.
- Build each affected environment with `pio run -e <env>`. A change under `shared/include` affects `mainframe` and `node`.
- A successful build or host test is not a device test. Report separately what was verified on hardware and what was not. Do not name a cause or claim a fix for behaviour seen on hardware, such as the open scene problem, without a measurement on the device.
- Flashing (`pio run -t upload`) and reading the serial port need the user's explicit approval every time.
- Check the port with `pio device list` before using it. `pio device monitor` needs an interactive terminal; in a session without a TTY, use a plain read-only serial client and do not change the firmware to get output.
- The `mainframe-eye-*`, `mainframe-beat-tracker` and `mainframe-*-capture` environments are for measurement only. Measure with audio analysis running, and flash the normal `mainframe` build again afterwards.
- Audio analysis depends on loop timing and signal level. Before a change to capture, codec gain, loop load or tempo estimation replaces the productive path, compare old and new behaviour on the device. Do not swap an algorithm silently.
