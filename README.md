# Decaflash V2

Independent, offline-capable light installation with an AtomS3R Mainframe, Atomic Voice Base and M5Atom Nodes. The original Decaflash project and its hardware remain independent; V2 has its own Mainframe and Node sources.

## Status

The Mainframe processes Voice Base audio for tempo (BPM) and the iris VU, controls scenes and clock over ESP-NOW and shows an interactive low-poly eye. The show clock adopts the confirmed tempo of the PCM tempo tracker; its hardware acceptance is pending. Local personality and motion logic influence the eye and the Node output. The Node firmware supports flashlight and RGB output.

V2 uses its own radio identity `DCF2` (`0x44434632`). The host tests check the separation of V1 and V2 for all protocol packets.

## Orientation

| Location | Content |
| --- | --- |
| [apps/mainframe/src](apps/mainframe/src) | AtomS3R Mainframe |
| [apps/node/src](apps/node/src) | M5Atom Node firmware |
| [shared/include](shared/include) | Protocol, transport and scenes |
| [docs/MAINFRAME_V2.md](docs/MAINFRAME_V2.md) | Working document: confirmed detailed state, open questions and further work |
| [bugs.md](bugs.md) | Known defects in code and documentation |
| [workers/decaflash/README.md](workers/decaflash/README.md) | Optional cloud Worker |
| [CLAUDE.md](CLAUDE.md), [.claude/settings.json](.claude/settings.json) | Working rules and permissions |

## Local commands

From the repository root, with PlatformIO installed:

```bash
pio run -e mainframe
pio run -e node
sh tests/run_all.sh
```

The Mainframe builds for the AtomS3R; the Node for `m5stack-atom` with FastLED 3.10.3. `tests/run_all.sh` runs all host tests under `tests/`; individual tests run via their respective `tests/run_*.sh`. A successful build or host test does not replace a device check.

List serial devices and monitor a confirmed port:

```bash
pio device list
pio device monitor -e mainframe --port <PORT>
pio device monitor -e node --port <PORT>
```

Local configuration stays outside Git:

- `include/wifi_credentials.h`, template: `include/wifi_credentials.example.h`
- `include/cloud_config.h`, template: `include/cloud_config.example.h`
- Worker secrets: see the [Worker README](workers/decaflash/README.md)
