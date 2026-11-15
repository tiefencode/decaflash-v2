# Ideen

Sammelstelle für Dinge, die noch nicht entschieden sind. Nichts in dieser Datei gilt als Regel oder als Auftrag.

## Regel-Kandidaten

Mögliche Arbeitsregeln für `CLAUDE.md`. Ein Kandidat ist keine Regel. Den Ablauf beschreibt der Skill `rule-candidates`.

Noch keine.

## Vorgemerkt

Vorhaben, die feststehen, aber auf etwas anderes warten.

### mise statt nvm

- **Wann:** nach dem neuen Node. Die OpenSpec-Einrichtung ist seit 2026-11-15 auf `main`.
- **Was:** nvm raus, mise rein. Eine `mise.toml` im Repo legt Node fest, wahrscheinlich auch Python und PlatformIO (passend zur Pinning-Regel). Die Befehle in der README werden angepasst.
- **Stand 2026-11-15:** nvm ist global installiert (`~/.nvm`, Node v20.15.1), mise noch nicht. `main` hat jetzt eine `.nvmrc` (Node 22) für OpenSpec, und die README sagt `nvm use`. Node brauchen im Projekt das Root-`package.json` (OpenSpec) und `workers/decaflash` (wrangler). Beim Umstieg ersetzt `mise.toml` die `.nvmrc`.
- **Achtung:** nvm zu entfernen ändert globale Shell-Dateien außerhalb des Repos (`~/.zshrc`, `~/.nvm`). Vorher nachfragen.
