# Decaflash V2

Eigenständige, offline-fähige Lichtinstallation mit einem AtomS3R-Mainframe, Atomic Voice Base und M5Atom-Nodes. Das ursprüngliche Decaflash-Projekt und seine Hardware bleiben unabhängig; V2 hat eigene Mainframe- und Node-Quellen.

## Stand

Der Mainframe verarbeitet Voice-Base-Audio für Tempo (BPM) und Iris-VU, steuert Szenen und Clock über ESP-NOW und zeigt ein interaktives Low-Poly-Auge. Die Show-Clock übernimmt das bestätigte Tempo des PCM-Tempo-Trackers; dessen Hardwareabnahme steht aus. Lokale Personality- und Bewegungslogik beeinflussen Auge und Node-Ausgabe. Die Node-Firmware unterstützt Flashlight- und RGB-Ausgabe.

V2 verwendet die eigene Funkkennung `DCF2` (`0x44434632`). Die Host-Tests prüfen die Trennung von V1 und V2 für alle Protokollpakete.

## Orientierung

| Ort | Inhalt |
| --- | --- |
| [apps/mainframe/src](apps/mainframe/src) | AtomS3R-Mainframe |
| [apps/node/src](apps/node/src) | M5Atom-Node-Firmware |
| [shared/include](shared/include) | Protokoll, Transport und Szenen |
| [docs/MAINFRAME_V2.md](docs/MAINFRAME_V2.md) | Arbeitsdatei: bestätigter Detailstand, offene Fragen und weitere Arbeit |
| [workers/decaflash/README.md](workers/decaflash/README.md) | Optionaler Cloud-Worker |
| [AGENTS.md](AGENTS.md) | Arbeitsregeln für alle Coding-Agents |
| [CLAUDE.md](CLAUDE.md), [.claude/settings.json](.claude/settings.json) | Claude-Code-Ergänzungen und Berechtigungen |

## Lokale Befehle

Aus dem Repository-Root, mit installiertem PlatformIO:

```bash
pio run -e mainframe
pio run -e node
sh tests/run_all.sh
```

Der Mainframe baut für AtomS3R; der Node für `m5stack-atom` mit FastLED 3.10.3. `tests/run_all.sh` führt alle Host-Tests unter `tests/` aus; einzelne Tests laufen über ihr jeweiliges `tests/run_*.sh`. Ein erfolgreicher Build oder Host-Test ersetzt keine Geräteprüfung.

Serielle Geräte auflisten und einen bestätigten Port überwachen:

```bash
pio device list
pio device monitor -e mainframe --port <PORT>
pio device monitor -e node --port <PORT>
```

Lokale Konfiguration bleibt außerhalb von Git:

- `include/wifi_credentials.h`, Vorlage: `include/wifi_credentials.example.h`
- `include/cloud_config.h`, Vorlage: `include/cloud_config.example.h`
- Worker-Secrets: siehe [Worker-README](workers/decaflash/README.md)
