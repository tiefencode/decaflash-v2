# Ideen

Sammelstelle für Dinge, die noch nicht entschieden sind. Nichts in dieser Datei gilt als Regel oder als Auftrag.

## Regel-Kandidaten

Mögliche Arbeitsregeln für `CLAUDE.md`. Ein Kandidat ist keine Regel. Den Ablauf beschreibt der Skill `rule-candidates`.

- Die Freigabe zum Flashen ist die Berechtigungsabfrage für den Upload-Befehl; nicht zusätzlich im Chat fragen. (15.11., 16.11.)

## Vorgemerkt

Vorhaben, die feststehen, aber auf etwas anderes warten.

### Node-`main.cpp` aufteilen

- **Warum:** `apps/node/src/main.cpp` hat rund 1.660 Zeilen und ist zu groß. Zum Vergleich: `apps/mainframe/src/main.cpp` hat rund 500.
- **Was:** Teile in eigene Dateien unter `apps/node/src` auslagern. Kandidaten nach dem heutigen Aufbau: Knopf-Gesten, Morse-/Text-Overlay, Demo-Takt und Beat-Rendering, Konfiguration im NVS (Node-Art, Rolle), serielle Befehle (Hilfe, Status, Programme). Reine Umstrukturierung, kein neues Verhalten.
- **Wann:** nach dem End-to-End-Test mit einem geflashten V2-Node (Regel in `CLAUDE.md`), damit es einen geprüften Stand zum Vergleichen gibt.
- **Prüfen:** `sh tests/run_all.sh`, `pio run -e node`, danach auf dem Gerät gegen den alten Stand vergleichen.

### Blinzeln im Ruhezustand

- **Warum:** Laut Anforderung in `docs/project-context.md` B.2 gehört Blinzeln zum Ruheverhalten des Auges. Umgesetzt ist es nicht; die Lider schließen sich heute nur beim Schlafen (`drawSleepLids()` in `apps/mainframe/src/eye_renderer.cpp`).
- **Was:** gelegentliches kurzes Blinzeln, solange das Auge wach ist. Naheliegend ist, die vorhandenen Schlaf-Lider kurz zu schließen und wieder zu öffnen. Ob Stimmung (zum Beispiel `attention`) die Häufigkeit beeinflusst, ist offen.
- **Wann:** offen; hängt nicht am End-to-End-Test, weil es weder Funk noch Node-Ausgabe betrifft.
- **Danach:** die eye-iris-Spec (`openspec/specs/eye-iris/spec.md`) um ein Requirement ergänzen.

### Message-Panel: nichts ungesehen verbrauchen

- **Wann:** nach der Bestandsaufnahme der Specs, als eigener OpenSpec-Change.
- **Heute:** Im Schlaf und bei eingeblendeter Debug-Anzeige zeichnet der Renderer das Panel nicht. Texte und Gedichte werden trotzdem ausgelöst und gelten als gezeigt: Die Sperrzeit der Bewegungsart startet, das Gedicht wird aus der Liste genommen und das Zwei-Minuten-Intervall beginnt neu. Während der Boot-Animation zeichnet der Renderer das Panel dagegen, soll es aber nicht (BUG-010). Beschrieben in `openspec/specs/message-panel/spec.md`, Requirement „Panel hidden while sleeping or in debug view“.
- **Was:** Texte auf Bewegungen werden in diesen Zuständen und während der Boot-Animation verworfen, ohne die Sperrzeit zu starten; weckt die Bewegung das Wesen, kommt der Text wie bisher. Ein fälliges Gedicht wartet, bis das Panel wieder sichtbar ist, wie bei belegtem Panel.
- **Wo:** `apps/mainframe/src/main.cpp` (Aufruf von `showMotion` und `serviceDepressionPoetry`) und `apps/mainframe/src/eye_renderer.cpp` (Boot-Prüfung beim Zeichnen), nicht im Panel selbst. Die Spec ändert sich mit.
