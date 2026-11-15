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

### Message-Panel: nichts ungesehen verbrauchen

- **Wann:** nach der Bestandsaufnahme der Specs, als eigener OpenSpec-Change.
- **Heute:** Im Schlaf und bei eingeblendeter Debug-Anzeige zeichnet der Renderer das Panel nicht. Texte und Gedichte werden trotzdem ausgelöst und gelten als gezeigt: Die Sperrzeit der Bewegungsart startet, das Gedicht wird aus der Liste genommen und das Zwei-Minuten-Intervall beginnt neu. Während der Boot-Animation zeichnet der Renderer das Panel dagegen, soll es aber nicht (BUG-010). Beschrieben in `openspec/specs/message-panel/spec.md`, Requirement „Panel hidden while sleeping or in debug view“.
- **Was:** Texte auf Bewegungen werden in diesen Zuständen und während der Boot-Animation verworfen, ohne die Sperrzeit zu starten; weckt die Bewegung das Wesen, kommt der Text wie bisher. Ein fälliges Gedicht wartet, bis das Panel wieder sichtbar ist, wie bei belegtem Panel.
- **Wo:** `apps/mainframe/src/main.cpp` (Aufruf von `showMotion` und `serviceDepressionPoetry`) und `apps/mainframe/src/eye_renderer.cpp` (Boot-Prüfung beim Zeichnen), nicht im Panel selbst. Die Spec ändert sich mit.
