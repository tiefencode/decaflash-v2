# Ideen

Sammelstelle für Dinge, die noch nicht entschieden sind. Nichts in dieser Datei gilt als Regel oder als Auftrag.

## Regel-Kandidaten

Mögliche Arbeitsregeln für `CLAUDE.md`. Ein Kandidat ist keine Regel. Den Ablauf beschreibt der Skill `rule-candidates`.

Noch keine.

## Vorgemerkt

Vorhaben, die feststehen, aber auf etwas anderes warten.

### mise statt nvm

- **Wann:** nach dem neuen Node und der OpenSpec-Einrichtung.
- **Was:** nvm raus, mise rein. Eine `mise.toml` im Repo legt Node fest, wahrscheinlich auch Python und PlatformIO (passend zur Pinning-Regel). Die Befehle in der README werden angepasst.
- **Stand 2026-11-14:** nvm ist global installiert (`~/.nvm`, Node v20.15.1), mise noch nicht. `main` hat keine `.nvmrc`. Node braucht im Projekt nur `workers/decaflash` (wrangler).
- **Achtung:** nvm zu entfernen ändert globale Shell-Dateien außerhalb des Repos (`~/.zshrc`, `~/.nvm`). Vorher nachfragen.
