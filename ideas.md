# Ideen

Sammelstelle für Dinge, die noch nicht entschieden sind. Nichts in dieser Datei gilt als Regel oder als Auftrag.

## Regel-Kandidaten

Mögliche Arbeitsregeln für `CLAUDE.md`. Ein Kandidat ist keine Regel. Den Ablauf beschreibt der Skill `rule-candidates`.

- Gerätetests so kurz planen, wie die Aussage es braucht; keine Handphasen, deren Dauer keinen Mehrwert bringt oder deren Daten schon vorliegen. (17.11.)

## Vorgemerkt

Vorhaben, die feststehen, aber auf etwas anderes warten.

### Augen-Grundtakt ohne Text oder Low-Level-Stufe

- **Warum:** Entscheidung des Nutzers vom 2026-11-17: nach `reduce-panel-frame-rate` als eigener Change prüfen. Gemessen ohne Text: 40 ms statt 25 ms senkt die Last von Core 1 von 58,8 % auf 47,1 % (`docs/project-context.md`, Abschnitt E).
- **Was:** entweder den Grundtakt senken oder eine feste Low-Level-Stufe (z. B. 40 ms ohne Text, 67 ms mit Text), die über bekannte Zustände eingeschaltet wird, nicht über gemessene Last. Werte und Auslöser im Interview klären.
- **Reserve:** Aussage des Nutzers vom 2026-11-17: 40 ms als allgemeiner Takt des Auges, auch ohne Text, ist eine Option, die wir bei CPU-Engpässen ziehen können (zum Beispiel für das Sprachmodell). Gemessen im Sweep: Audio-Lücke ohne Text 51 statt 55 ms, mit Text bei 40 ms 49 statt 92 ms.
- **Wann:** nach dem Merge von `reduce-panel-frame-rate`; passt zum Sprachmodell-Prototyp.
- **Prüfen:** Look auf dem Gerät, Messung mit `mainframe-panel-rate-sweep`.

### Sprachmodell: Vortraining auf Deutsch mit größerem Modell

- **Warum:** Entscheidung des Nutzers vom 2026-11-18: Erst mit Wort-Token ein funktionierendes Modell bauen, danach den Ansatz „erst Deutsch lernen, dann den Korpus“ mit einem größeren Modell noch einmal versuchen. Bei der ersten Messung hat das Vortraining nicht geholfen (`openspec/changes/add-text-model/design.md`, Decision 14).
- **Was:** Mehr Vortrainingstext, gern Grufti-Prosa des 19. Jahrhunderts; alte Rechtschreibung ist erlaubt, ein „th“ ist sogar erwünscht. Ein Wortschatz über den Korpus hinaus, damit die Bücher nicht auf die wenigen Sätze aus Korpus-Wörtern schrumpfen. Ein größeres Modell (4 × 96 oder 6 × 128), weil 3 × 64 neben allgemeinem Deutsch kaum noch Platz für unseren Ton hat. Mehrere Seeds je Variante.
- **Heute:** Werkzeug `apps/mainframe/tools/text_model_pretrain_data.py` (4 Gutenberg-Bücher, Download mit Prüfsumme) und `text_model.py train --pretrain` / `--init`.
- **Wann:** nach einem Wort-Token-Modell, das die Bewertung besteht (Task 6.2).
- **Prüfen:** Fehler pro Validierungstext gegen das Wort-Token-Modell, dann die 30 Bewertungsfälle.

### Depressions-Gedichte vom eigenen Sprachmodell

- **Warum:** Entscheidung des Nutzers vom 2026-11-17 im Interview zu `add-text-model`: Das Modell schreibt zuerst nur die Reaktionstexte; die Gedichte bleiben vorerst die feste Liste, sollen aber später auch vom Modell kommen.
- **Was:** Gedichte als eigene Ereignisart im selben Modell; Emoji-zuerst und Wort-für-Wort-Anzeige wie bei den Reaktionstexten. Länge, Ton und ob die Stimmung einfließt im Interview klären.
- **Wann:** nach `add-text-model`, wenn Modellgröße und Qualität der Reaktionstexte feststehen.
- **Prüfen:** Formprüfung und Bewertung wie in `add-text-model` (design.md, Decision 9), Gerätetest mit `mainframe-slm-capture`.

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

### Präsenzbus auf 400 kHz

- **Stand:** Vorschlag aus der Messung vom 2026-11-17, vom Nutzer noch nicht entschieden.
- **Warum:** Der Präsenzsensor (STHS34PF80) wird mit 100 kHz abgefragt und belegt damit 4,4–4,5 % von Core 1. Mit 400 kHz waren es in der Messung 1,7–1,9 %: eine Abfrage ohne Sample 0,31–0,37 ms statt 0,69–0,75 ms, eine mit Sample 1,04–1,10 ms statt 3,05–3,12 ms, ohne Lesefehler.
- **Was:** eine Konstante, `kBusHz` in `apps/mainframe/src/presence_sensor.cpp`.
- **Grenzen:** Bei 400 kHz wurden nur etwa 65 s am jetzigen Grove-Kabel gemessen. Das Datenblatt (DS13916, Abschnitt 3.1.2) nennt I2C bis 1 MHz.
- **Prüfen:** `sh tests/run_all.sh`, `pio run -e mainframe`, danach auf dem Gerät den Lesefehler-Zähler über einen längeren Lauf beobachten.
- **Quelle:** Messumgebung `mainframe-loop-load-trial` auf dem Branch `claude/blocked-sections-analysis-a46e53` (nicht gemergt).
- **Nicht Teil davon:** das Auslesen des Bewegungssensors über den FIFO des BMI270. Es brächte alle 100 Samples pro Sekunde statt 43–65 zum Klassifikator, aber die Klopf-Schwellen sind auf die heutigen Daten eingestellt, und die Erkennung aus FIFO-Daten ist nicht geprüft.

### Hauptschleife des Mainframes in Tasks aufteilen

- **Stand:** Vorschlag vom 2026-11-17, vom Nutzer als Vorhaben notiert, Umfang und Aufteilung noch nicht entschieden.
- **Warum:** Heute läuft alles des Mainframes nacheinander in `loop()` auf Core 1. Ein langer Durchlauf (Auge, Panel) verzögert das Abholen der Audioblöcke und der IMU-Samples. In der Sprachmodell-Messung vom 2026-11-17 (`docs/slm-prototype.md`, Ergebnisse zu 6.4) wurden die größten Lücken mit rechnendem Modell um 2 bis 11 ms länger (Text-Zustand: Audio 67 statt 56 ms, IMU 50 statt 39 ms), obwohl das Modell auf Core 0 rechnet. Ein eigener Task mit höherer Priorität für Audio und IMU würde diese Lücken vom Auge entkoppeln.
- **Was:** Audio-Abholung, Tempo und IMU (eventuell Show-Takt) in einen Task über `loop()`; Auge, Panel, Persönlichkeit und Präsenz bleiben in `loop()`. Gemeinsame Daten ohne Blockieren austauschen.
- **Grenzen:** Nicht gemessen, ob es hilft: Core 1 wird mit dem Modell auch langsamer, ohne dass dort mehr Code läuft; falls das Speicher- oder Cache-Konkurrenz ist, hilft ein eigener Task dagegen nicht. Großer Umbau am produktiven Audio- und Tempo-Pfad.
- **Wann:** nach der Klärung der BPM-Erkennung, als eigener OpenSpec-Change mit Interview.
- **Prüfen:** Vorher-Nachher auf dem Gerät mit `mainframe-loop-load` bzw. `mainframe-slm-capture` (Audio- und IMU-Lücken, Beat-Verspätung), `sh tests/run_all.sh`.
- **Verwandt:** „Präsenzbus auf 400 kHz“ spart etwa 2,6 Prozentpunkte von Core 1, „Augen-Grundtakt ohne Text“ etwa 7 bis 11 Punkte; beides kleiner und ohne Umbau.
- **Nicht gemeint:** `apps/mainframe/src/main.cpp` nur in Dateien aufzuteilen; das ändert am Timing nichts.
