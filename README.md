# Decaflash V2

Eigenständige Weiterentwicklung der Decaflash-Lichtinstallation für AtomS3R und Atomic Voice Base. Die Lichtshow soll vollständig offline funktionieren; Internet und KI sind optionale Erweiterungen. Audio und Node-Kommunikation haben Vorrang vor Display und Cloud.

## Aktueller Stand

Der aktive V2-Startpunkt ist ein AtomS3R-Mainframe mit validiertem Speicher- und Voice-Base-I²C-Bring-up sowie erstem Offline-ESP-NOW-Kern: Frontbutton startet die Show oder wechselt die Szene, der Mainframe sendet Szene und Clock bei festen 120 BPM. Die Flashlight-/RGB-Node-Firmware, gemeinsame Szenen und ESP-NOW-Kommunikation wurden übernommen. Eine lokale, ignorierte Kopie des alten M5Atom-Codes liegt unter `reference/m5atom_controller`; das ursprüngliche Projekt und seine Git-Historie bleiben die maßgebliche Quelle.

**Die V2-Funkkennung ist getrennt:** `DCF2` (`0x44434632`) statt V1 `DCFL`. Lokale Protokolltests prüfen gegenseitige Ablehnung aller vier Pakettypen. Der Mainframe ist mit dieser Kennung geflasht und meldet `radio=READY`; die Funkprüfung mit einem ebenfalls geflashten V2-Node steht aus. Die Ursache des gemeldeten Szenenproblems ist weiterhin ungeklärt.

AtomS3R und Atomic Voice Base sind angeschlossen. Der geflashte Mainframe bestätigt 8 MB Flash, knapp 8 MB PSRAM mit 64-KiB-Lesetest und I²C-ACKs der Voice Base an `0x18` und `0x43`. Visuelle Display-/Buttonprüfung sowie Codec und Audiofunktion stehen noch aus. Der spätere U185-Präsenzsensor ist noch nicht vorhanden und aktuell nicht erforderlich.

Das alte Projekt und seine Hardware bleiben unabhängig. V2 enthält eigene Mainframe- und Node-Quellen; es gibt keine gemeinsam veränderlichen Quelldateien.

## Orientierung

| Ort | Zuständigkeit |
| --- | --- |
| [apps/mainframe/src](apps/mainframe/src) | Aktiver AtomS3R-Mainframe: Hardware-Bring-up und Offline-Szene/Clock über ESP-NOW |
| [apps/node/src](apps/node/src) | Gemeinsame Node-Logik und Flashlight-/RGB-Ausgabe |
| [shared/include](shared/include) | Protokoll, Transport, Typen und einkompilierte Szenen |
| [docs/MAINFRAME_V2.md](docs/MAINFRAME_V2.md) | Verbindliche V2-Entscheidungen, Codeanalyse, Hardwaregrenzen und weitere Reihenfolge |
| [workers/decaflash/README.md](workers/decaflash/README.md) | Optionaler Cloud-Worker: lokale Entwicklung und API-Vertrag |
| [AGENTS.md](AGENTS.md) | Arbeitsregeln für dieses Repository |
| [docs/v1-outline.md](docs/v1-outline.md) | Historischer V1-Entwurf, keine aktuelle Planung |

Diese README ist der kurze Einstieg mit Betriebsbefehlen. Ausführliche Analyse und Produktplanung werden ausschließlich im Mainframe-Kontext gepflegt. Die vorhandene Struktur mit `apps/mainframe`, `apps/node` und `shared/include` bleibt Ausgangspunkt.

## Lokale Befehle

Aus dem Repository-Root, mit installiertem PlatformIO:

```bash
pio run -e mainframe
pio run -e node
sh tests/run_protocol_identity.sh
```

`mainframe` baut für AtomS3R und enthält Hardware-Bring-up sowie die Offline-Showsteuerung über ESP-NOW. Es prüft Speicher, Display, Frontbutton und I²C-Erreichbarkeit der Voice Base. `node` bleibt auf `m5stack-atom` und verwendet FastLED 3.10.3 für die RGB-Ausgabe; FastLED ist keine Mainframe-Abhängigkeit. Version 3.10.5 erzeugte auf dem angeschlossenen Node nur weißlich-statische Ausgabe, während 3.10.3 Demo und Mainframe-Szenen korrekt ausgibt. Die Befehle bauen bzw. testen lokal; ein erfolgreicher Build bestätigt keine Hardwarefunktion. Ablauf und Grenzen stehen im [V2-Kontext](docs/MAINFRAME_V2.md#minimaler-s3r-hardwaretest).

Plattform- und Hardwarebibliotheksversionen bleiben nach erfolgreichem Gerätetest festgesetzt. Ein Update erfolgt einzeln und erst nach Build, Flash und sichtbarer Hardwareprüfung auf einem Testgerät.

Serielle Geräte auflisten und einen zuvor identifizierten Port überwachen:

```bash
pio device list
pio device monitor -e node --port <PORT>
```

Für den S3R-Test entsprechend `-e mainframe` verwenden. Alte feste USB-Portzuordnungen sind nicht übertragbar. Der Mainframe wurde am AtomS3R über `/dev/cu.usbmodem101` erfolgreich geflasht; einen Port immer erst mit `pio device list` bestätigen.

Lokale Konfiguration bleibt außerhalb von Git:

- `include/wifi_credentials.h`, Vorlage: `include/wifi_credentials.example.h`
- `include/cloud_config.h`, Vorlage: `include/cloud_config.example.h`
- Worker-Secrets: siehe [Worker-README](workers/decaflash/README.md)

Die übernommenen Cloud-Jobs pausieren ESP-NOW; WLAN kann den Funkkanal wechseln. Der Cloud-Pfad erfüllt die geforderte kontinuierliche Node-Steuerbarkeit daher noch nicht. Eine V2-Cloudflare-Verknüpfung oder ein Deployment ist nicht eingerichtet bzw. bestätigt.

## Nächster Schritt

Einen V2-Node flashen und danach die Mainframe-/Node-Kommunikation einschließlich Funktrennung und Szenenwechsel auf Hardware prüfen. Weitere Prioritäten stehen im [V2-Kontext](docs/MAINFRAME_V2.md).
