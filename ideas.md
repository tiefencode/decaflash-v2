# Ideen

Sammelstelle für Dinge, die noch nicht entschieden sind. Nichts in dieser Datei gilt als Regel oder als Auftrag.

## Regel-Kandidaten

Mögliche Arbeitsregeln für `CLAUDE.md`. Ein Kandidat ist keine Regel. Den Ablauf beschreibt der Skill `rule-candidates`.

Noch keine.

## Vorgemerkt

Vorhaben, die feststehen, aber auf etwas anderes warten.

### Node-`main.cpp` aufteilen

- **Warum:** `apps/node/src/main.cpp` hat rund 1.660 Zeilen und ist zu groß. Zum Vergleich: `apps/mainframe/src/main.cpp` hat rund 500.
- **Was:** Teile in eigene Dateien unter `apps/node/src` auslagern. Kandidaten nach dem heutigen Aufbau: Knopf-Gesten, Morse-/Text-Overlay, Demo-Takt und Beat-Rendering, Konfiguration im NVS (Node-Art, Rolle), serielle Befehle (Hilfe, Status, Programme). Reine Umstrukturierung, kein neues Verhalten.
- **Wann:** nach dem End-to-End-Test mit einem geflashten V2-Node (Regel in `CLAUDE.md`), damit es einen geprüften Stand zum Vergleichen gibt.
- **Prüfen:** `sh tests/run_all.sh`, `pio run -e node`, danach auf dem Gerät gegen den alten Stand vergleichen.

### Stummschaltung am Flashlight-Node entfernen

- **Warum:** Entscheidung des Nutzers vom 2026-11-16: Die Stummschaltung per langem Druck hat keinen erkennbaren Nutzen und ist vermutlich ein Überbleibsel aus V1. Ein zweiter langer Druck hebt sie nicht auf; unter Mainframe-Steuerung endet sie erst mit einem Szenenwechsel, einem Hello oder einem seriellen `mode`-/`role`-Befehl.
- **Heute:** beschrieben in `openspec/specs/node-device/spec.md`, Requirements „Long press per Node kind“ und „Output mute“.
- **Was:** `outputMuted` und den Mute-Zweig in `serviceButton()` entfernen; der lange Druck am Flashlight hat danach keine Funktion. Hilfetext (`long press -> turn output off`) und `status` (`muted=`) anpassen. Damit entfällt auch BUG-021 („Node text overlay lights a muted flashlight“) in `bugs.md`.
- **Wann:** nach dem End-to-End-Test mit einem geflashten V2-Node (Regel in `CLAUDE.md`), als eigener OpenSpec-Change; passt zum Aufteilen der Node-`main.cpp`.
- **Prüfen:** `sh tests/run_all.sh`, `pio run -e node`, auf dem Gerät langer Druck am Flashlight in Demo und unter Mainframe-Steuerung.

### Host-Tests für Szenen und Node-Ausgabe

- **Warum:** Weder `shared/include/scene_programs.h` noch die Node-Ausgabe in `apps/node/src` haben einen Host-Test. Die node-output-Spec (`openspec/specs/node-output/spec.md`) beschreibt das Verhalten, prüft es aber nicht.
- **Was:** Host-Tests für die reine Logik: Motivwahl und Parameter des Flash-Variationsprofils (deterministisch je Takt), Trigger-Takte und -Beats, Morse-Kodierung des Text-Overlays mit Lücken von 1/2/4 Einheiten. Renderer mit FastLED oder GPIO bleiben außen vor, solange sie nicht ohne Hardware laufen.
- **Wann:** offen; sinnvoll vor oder zusammen mit „Node-`main.cpp` aufteilen“, weil die Morse-Logik dafür aus `main.cpp` herausgelöst werden muss.
- **Prüfen:** `sh tests/run_all.sh`.

### RGB-Muster vereinheitlichen

- **Warum:** Das RGB-Muster PulseRow (eine Reihe kurzer Pulse, auf einem Beat ausgelöst) nutzt keine Szene, ebenso seine Vorlagen `heartbeat()` und `triplePulse()` in `shared/include/scene_programs.h`. Die Muster Heartbeat und RiserPulse sind im Kern Pulsreihen mit fest eingebauter Form. Beschrieben in `openspec/specs/node-output/spec.md`.
- **Was:** PulseRow so erweitern, dass es Heartbeat und RiserPulse abbilden kann: ein Ausklang nach dem letzten Puls, Zeiten in Beat-Anteilen statt Millisekunden, Berechnung aus der Takt-Position statt als Ereignis (damit es einen verpassten Beat oder einen Override übersteht), Stärke je Puls. Danach Heartbeat und RiserPulse als PulseRow-Vorlagen und die Sonder-Renderer entfernen, oder PulseRow samt Vorlagen löschen, falls es nicht gebraucht wird.
- **Wann:** offen; zusammen mit dem endgültigen Lichtdesign der Szenen und erst nach dem End-to-End-Test mit einem geflashten V2-Node (Regel in `CLAUDE.md`).
- **Prüfen:** `sh tests/run_all.sh`, `pio run -e node`, auf dem Gerät alten und neuen Heartbeat bzw. Riser vergleichen; die node-output-Spec ändert sich mit.

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
