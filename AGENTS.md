# AGENTS.md

Repository-specific guidance for independent Decaflash V2 development.

## Canonical context

- Read [README.md](README.md) first for current state and operational commands.
- Read [docs/MAINFRAME_V2.md](docs/MAINFRAME_V2.md) for confirmed decisions, detailed code analysis, hardware limits and implementation order.
- Keep README concise; maintain detailed product context in MAINFRAME_V2.md rather than duplicating it.
- Worker-specific instructions belong in [workers/decaflash/README.md](workers/decaflash/README.md). `docs/v1-outline.md` is historical.

## Project boundaries

- Work only in this V2 repository. Do not modify the old Decaflash project or share mutable source files with it.
- Keep `apps/mainframe`, `apps/node` and `shared/include` as the starting structure. V2 may change both Mainframe and Node firmware.
- Reuse existing code selectively; proceed in small, verifiable steps. `reference/m5atom_controller` is a local ignored copy of historical M5Atom code for reading only; the original project and Git history are canonical.
- V2 must use its own radio identity. V1 and V2 must reject each other's SceneSelect, ClockSync, MainframeHello and NodeText packets. V2 now uses magic `0x44434632` (DCF2), while V1 uses `0x4443464C`; keep the isolation tests passing.
- The observed scene problem has no confirmed cause or verified hardware fix.

## Architecture anchors (relative to this repository)

- Mainframe: `apps/mainframe/src/main.cpp`
- Node: `apps/node/src/main.cpp`
- Protocol: `shared/include/protocol.h`
- ESP-NOW transport: `shared/include/espnow_transport.h`

## Hardware and implementation

- `mainframe` targets AtomS3R and includes its validated bring-up plus the first offline ESP-NOW scene/clock path. `node` remains the M5Atom/FastLED firmware; FastLED is retained only for Node output.
- Pin PlatformIO platforms and hardware libraries after a successful device test. Update one dependency at a time, build it, flash a designated test device and verify its physical behavior before changing the pinned version. Do not use an unpinned dependency update as part of unrelated firmware work.
- Run `sh tests/run_protocol_identity.sh` for host-side identity checks and build the affected PlatformIO environments.
- AtomS3R and Atomic Voice Base are connected according to the user; confirm actual variants before porting. TMOS is absent and not currently needed.
- Voice Base requires an I²S/codec port of the existing PDM input. The old Matrix UI is 5×5; the target display is 128×128.
- Offline audio and Node communication take priority over display and cloud. ESP-NOW currently uses channel 1; Wi-Fi association can change that channel. Existing cloud jobs pause ESP-NOW.
- Morse output is a BPM/beat-clock scheduling problem, not just a visual effect.
- Audio analysis depends on loop timing and signal level; validate codec/gain and timing effects when porting.

## Secrets and cloud

- Keep `include/wifi_credentials.h` and `include/cloud_config.h` local and ignored; their `.example.h` files are tracked templates.
- Never print or commit local secrets, including Worker `.dev.vars`.
- Do not carry over the old Cloudflare deployment connection, Worker or KV resources automatically. Deployment is not part of the current work.
- When deployment is requested later, the normal path is Git/Cloudflare integration; do not assume local Wrangler is installed or needed.

## Deferred audio work

- Recording already requests up to 12 seconds of µ-law audio, with RAM-dependent shortening; do not describe 8–12-second support as absent.
- `musicPresent()` is a coarse local gate, not a reliable recognition-readiness signal.
- Improve clip quality/readiness before spending further AudD calls; communication and the hardware port come first.
