# Ideen

Sammelstelle für Dinge, die noch nicht entschieden sind. Nichts in dieser Datei gilt als Regel oder als Auftrag.

## Regel-Kandidaten

Mögliche Arbeitsregeln für `CLAUDE.md`. Ein Kandidat ist keine Regel. Den Ablauf beschreibt der Skill `rule-candidates`.


## Vorgemerkt

Vorhaben, die feststehen, aber auf etwas anderes warten.

### Augen-Grundtakt ohne Text oder Low-Level-Stufe

- **Warum:** Entscheidung des Nutzers vom 2026-11-17: nach `reduce-panel-frame-rate` als eigener Change prüfen. Gemessen ohne Text: 40 ms statt 25 ms senkt die Last von Core 1 von 58,8 % auf 47,1 % (`docs/project-context.md`, Abschnitt E).
- **Was:** entweder den Grundtakt senken oder eine feste Low-Level-Stufe (z. B. 40 ms ohne Text, 67 ms mit Text), die über bekannte Zustände eingeschaltet wird, nicht über gemessene Last. Werte und Auslöser im Interview klären.
- **Reserve:** Aussage des Nutzers vom 2026-11-17: 40 ms als allgemeiner Takt des Auges, auch ohne Text, ist eine Option, die wir bei CPU-Engpässen ziehen können (zum Beispiel für das Sprachmodell). Gemessen im Sweep: Audio-Lücke ohne Text 51 statt 55 ms, mit Text bei 40 ms 49 statt 92 ms.
- **Wann:** nach dem Merge von `reduce-panel-frame-rate`; passt zum Sprachmodell-Prototyp.
- **Prüfen:** Look auf dem Gerät, Messung mit `mainframe-panel-rate-sweep`.

### Depressions-Gedichte vom eigenen Sprachmodell

- **Warum:** Entscheidung des Nutzers vom 2026-11-17 im Interview zu `add-text-model`: Das Modell schreibt zuerst nur die Reaktionstexte; die Gedichte bleiben vorerst die feste Liste, sollen aber später auch vom Modell kommen.
- **Was:** Gedichte als eigene Ereignisart im selben Modell; Emoji-zuerst und Wort-für-Wort-Anzeige wie bei den Reaktionstexten. Länge, Ton und ob die Stimmung einfließt im Interview klären.
- **Wann:** nach `add-text-model`, wenn Modellgröße und Qualität der Reaktionstexte feststehen.
- **Prüfen:** Formprüfung und Bewertung wie in `add-text-model` (design.md, Decision 9), Gerätetest mit `mainframe-slm-capture`.

### Zweiter Sprach-Durchgang durch den Korpus

- **Warum:** Aussage des Nutzers vom 2026-11-18 nach der Überarbeitung des Korpus: Einige Sätze klingen noch komisch, und das Modell lernt solche Sätze mit. Der Korpus geht trotzdem vorerst so ins Training, geglättet wird danach.
- **Was:** alle Dateien in `apps/mainframe/text_model/corpus/` auf Grammatik und natürliches Deutsch lesen. Der Nutzer sammelt vorher Beispiele, die ihm komisch vorkommen; daraus Muster ableiten und in allen Dateien korrigieren, nicht nur Einzelzeilen. Dabei bleiben: Stufen laut Plan, Kontextregel (das Ereignis bleibt erkennbar), die „Zu viel klopfen“-Regel in `multi_tap`, die Originale, die der Nutzer behalten will, und eindeutige Texte.
- **Nachtrag (2026-11-18, nach dem Einbau von Textbank und Textmodell):** Dem Nutzer ist am Gerät aufgefallen, dass auch Zeilen aus der Textbank teilweise schlecht sind; er nennt das eines der nächsten Projekte. In der Messung vom selben Tag hielten zwei Claude-Prüfer 194 von 200 Textbank-Zeilen für brauchbar (97 %), der Nutzer zuvor 28 von 30; die Prüfer stimmen aber nur zu 60–68 % mit seinem Urteil überein, die 97 % sagen also wenig über seinen Maßstab. Schlechte Korpus-Zeilen wirken doppelt: Sie erscheinen als Textbank-Zeile und sind Trainingsmaterial des Textmodells.
- **Dazu passend:** eine kleinere Wortliste aus den häufigen Korpus-Wörtern (das Referenzmodell Barista kommt mit 854 handverlesenen Wörtern aus, bei uns kommen 956 von 2.048 Korpus-Wörtern nur einmal vor). Zeilen mit seltenen Wörtern ohnehin umzuschreiben und den Sprach-Durchgang in einem Zug zu machen, spart eine zweite Runde.
- **Beispiel „Drehteller“ (2026-11-19):** Das Wort steht im Korpus genau einmal (`rotate.tsv`: „SPINNST DU? ICH BIN KEIN DREHTELLER!“). In 1.200 gemessenen Modell-Texten (`models/text-model/gate2/cases-1200.jsonl`) kommt „ICH BIN KEIN DREHTELLER“ 39-mal vor (3,3 %), bei elf der zwölf Ereignisse. Das Muster „ICH BIN KEIN(E) …“ steht 17-mal im Korpus, jedes Mal mit einem anderen, einmaligen Hauptwort; das Modell beherrscht das Muster und füllt die Lücke mit einem Standardwort (Deutung, nicht gemessen). Danach suchen: Satzmuster, deren Lücke lauter einmalige Wörter füllen. Abhilfe: solche Wörter mehrfach und zum Ereignis passend verwenden, oder das Muster seltener machen.
- **Wiederholungen (gemessen am Mac, 2026-11-19):** In denselben 1.200 Fällen enden 64 Texte auf „ICH BIN NICHT EINSAM.“, 32 auf „ICH BIN KEIN DREHTELLER!“, 30 auf „ICH BIN ZUFRIEDEN.“; 159 sind wörtliche Korpus-Zeilen. Bei gleichem Ereignis und gleicher Stimmung (Stufen 1 1 1 0 1, wie auf einer Party mit niedriger Einsamkeit) geben 100 Zufallswerte nur 73 bzw. 70 verschiedene Texte, mit 8 Emoji und 19 bzw. 24 Satzanfängen; der häufigste Text kommt 11- bzw. 17-mal. Ursachen nach Deutung: die Stimmung ändert sich auf dem Gerät kaum, die milde Auswahl nimmt nach dem ersten Wort fast immer das wahrscheinlichste, und der Korpus bietet für häufige Satzanfänge wenige Fortsetzungen.
- **Wann:** nach dem nächsten Training, wenn die Bewertungsfälle (`apps/mainframe/text_model/rating_cases.tsv`) zeigen, welche Satzmuster das Modell übernimmt.
- **Prüfen:** `python3 -I apps/mainframe/tools/text_model_vocab.py` und `python3 -I apps/mainframe/tools/text_bank.py` neu erzeugen, `python3 -I apps/mainframe/tools/text_model_corpus.py check`, `sh tests/run_all.sh`; beide Header mit dem Korpus committen; danach nachtrainieren.

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

### Textmodell: Wortteile noch einmal probieren

- **Stand:** Wunsch des Nutzers vom 2026-11-18. Das Textmodell (`add-text-model`) nutzt Wort-Tokens; Wortteile (BPE) wurden gemessen und zurückgestellt.
- **Warum:** Wortteile können seltene Wörter und deutsche Wortformen aus gemeinsamen Bausteinen bilden und machen ein Vortraining auf fremdem Text möglich. Mit dem heutigen Korpus (3.632 Zeilen, 16.312 Wörter, 2.048 verschiedene) waren sie aber schlechter.
- **Heute gemessen** (Host, 3 × 64, Fehler pro Validierungstext in nats, gleicher Split): Wörter 27,7 mit 0 % erfundenen Wörtern; Wortteile ohne Vortraining 31,8–33,8 mit 6–13 % erfundenen Wörtern; Wortteile mit Vortraining auf vier Gutenberg-Büchern 30,6–30,9 mit 2–4 %; Wörter mit Vortraining 28,3–30,7. Beschrieben in `openspec/changes/add-text-model/design.md`, Decision 14 (Branch `claude/slm-core-design-24642e`).
- **Was:** dieselbe Messung wiederholen, mit denselben drei Kriterien: Fehler pro Validierungstext gegen Wort-Tokens, Wirkung des Vortrainings, Anteil erfundener Wörter. Sinnvoll vor allem mit mehr Korpus oder einem Vortraining im eigenen Stil. Erfundene Wörter ließen sich beim Erzeugen ausschließen, indem nur Teile angehängt werden dürfen, die zu einem bekannten Wort führen (Idee, nicht geprüft).
- **Wiederholt (2026-11-19, vom Nutzer berichtet, Lauf in einem Codex-Worktree, hier nicht nachgeprüft):** je 12 Texte vom Nutzer bewertet, Validierungs-Loss pro Zeichen auf denselben 360 Validierungszeilen. Wort-Tokens mit Gutenberg 5 von 12 brauchbar, Loss etwa 1,012; SentencePiece BPE mit 2.000 Tokens ohne Gutenberg 5 von 12, 1,203; BPE mit Gutenberg 3 von 12, 1,212; SentencePiece Unigram mit 2.000 Tokens und Gutenberg 4 von 12, 1,201. Unigram ohne Gutenberg wurde nicht getestet. Zwölf Texte je Variante trennen die Varianten nicht; der Loss pro Zeichen spricht wie die Messung vom 2026-11-18 für Wort-Tokens, und das Gutenberg-Vortraining zeigt erneut keinen Nutzen. Wortteile lösen weder Sprachqualität noch Ereignistreue.
- **Wann:** wenn das Textmodell mit Wort-Tokens fertig und auf dem Gerät abgenommen ist. Nach der Wiederholung vom 2026-11-19 nur noch sinnvoll, wenn sich Korpus oder Vortrainingstext deutlich ändern.
- **Werkzeug:** Das Vortrainings-Werkzeug ist auf dem Branch committet; die Experiment-Skripte liegen laut der Mess-Session im Notizordner unter `text-model-tokens/` und sind nicht im Repo.

### Textmodell: Verwerf-Schwelle für unsichere Texte

- **Stand:** Am 2026-11-18 gemessen und als OpenSpec-Change `add-text-model-confidence-gate` geplant (Branch `claude/text-model-confidence-gate-650684`); noch nicht gebaut. Ergebnis: früh keine Aussage, am ganzen Text nur mäßig; die Schwelle bleibt als Notbremse (Abbruch ab Token 5, etwa 20 % Abbrüche). Zahlen in der design.md des Changes.
- **Vorgemerkt, nicht begonnen:** eine Maske, die nur zum Subjekt passende Verbformen zulässt (lohnt sich nur, wenn eine Grammatik-Fehlerart überwiegt; bisher nicht), und ein Vortrainingstext aus kurzen, einfachen deutschen Sätzen im Wortschatz des Modells statt der Romane.
- **Warum:** Auf dem Gerät kommt das Emoji des Textmodells nach 0,6–0,7 s, die Textbank springt deshalb praktisch nie ein. In der Bewertung waren 16 von 30 Texten des Textmodells brauchbar, bei der Textbank 28 von 30; der Nutzer sieht am Gerät öfter schlechtes Deutsch und unsinnige Texte.
- **Was:** Das Textmodell kennt bei jedem Token seine Sicherheit. Liegt sie unter einer Schwelle, zeigt das Panel statt des Modell-Texts eine Zeile aus der Textbank. Rechenzeit kostet das nicht.
- **Entscheidung des Nutzers (2026-11-18):** Die Variante „erst fertig schreiben, dann zeigen“ kommt nicht in Frage; das Emoji und das Wachsen Wort für Wort bleiben. Übrig sind: früh entscheiden (anhand von Emoji und ersten Wörtern) oder einen angefangenen Text abbrechen und durch die Textbank-Zeile ersetzen. Das ist noch offen.
- **Erst messen:** viele zufällige Fälle am Mac mit Sicherheitswert erzeugen (Werkzeug aus Commit `1a6a4e6`); der Nutzer bewertet eine gezielte Stichprobe aus niedriger, mittlerer und hoher Sicherheit, ohne den Wert zu sehen; dazu ein an seinen Urteilen geeichtes Urteil durch Claude über einige hundert Texte. Gesucht ist, ob die Sicherheit das Urteil vorhersagt, wie früh im Text das erkennbar ist und bei welcher Schwelle wie viele Texte an die Textbank gehen.
- **Wann:** nach dem Abschluss von `add-text-model`, als eigener OpenSpec-Change.

### Textmodell: kaum Reaktion auf die Stimmungsstufen

- **Warum:** Messung vom 2026-11-18 auf dem Mac mit `text_model_write` und dem Image `w6x128-pre-ft-400-3e-3.bin`, je 300 Seeds: Bei Klopfer endet „ICH BIN NICHT EINSAM“ in 8 Texten bei Einsamkeit niedrig, in 2 bei Mitte und in 10 bei hoch. Bei „Kommt davor“ handeln 71 bis 104 von 300 Texten von Besuch oder Gesellschaft, unabhängig von der Einsamkeitsstufe. Das Modell folgt der Stufe also kaum; was es schreibt, bestimmt vor allem das Ereignis.
- **Nachmessung (2026-11-19, Mac, `text_model_write`, je 300 Seeds, Ereignis-Nummern 1 und 5):** je eine Stimmungsachse auf niedrig, mittel und hoch, die anderen vier auf mittel. Das Emoji folgt jeder Achse deutlich und in der erwarteten Richtung: Genervtheit hoch gibt 🙄 😤 💢 (231 von 300), niedrig 🙂 😂 😌; Einsamkeit hoch 🥺 🥹 😢 (208), niedrig 🙂 😌; Aufmerksamkeit hoch 👁 (137), niedrig 🫠 (130); Energie niedrig 🫩 (87), hoch 🙌 (130); Depression hoch 🖤 💀 (127). Die Wörter folgen schwächer, aber gerichtet: Wörter mit NERV/GENUG/HÖR AUF/LASS bei Genervtheit 1 / 1 / 9 und 0 / 0 / 18; mit MÜDE/GÄHN/SCHLAF bei Energie 8 / 1 / 0 und 18 / 7 / 2; mit EINSAM/ALLEIN bei Einsamkeit 58 / 19 / 36 und 37 / 8 / 17, also am häufigsten bei niedriger Einsamkeit („ICH BIN NICHT EINSAM“). Die Wortverteilung zwischen niedrig und hoch unterscheidet sich je Achse um das 1,7- bis 3-Fache des Rauschens zwischen zwei Seed-Hälften. Der Befund „kaum Reaktion“ oben gilt damit nur für den dort gezählten einen Satzschluss; das Modell reagiert auf die Stufen, vor allem über das Emoji. Eine dauerhaft niedrige Einsamkeit verdreifacht etwa die Texte mit EINSAM/ALLEIN gegenüber der Mitte.
- **Was:** prüfen, warum das Modell die Stimmungsstufen schwach nutzt (Verteilung der Stufen im Korpus, Gewicht der Bedingungs-Tokens, Training), und es so nachtrainieren, dass die Stufen die Texte erkennbar lenken. Ziel und Messgröße im Interview klären.
- **Wann:** nach dem Abschluss von `add-text-model`.
- **Prüfen:** dieselbe Messung wiederholen und mit den Zahlen oben vergleichen.

### Gewöhnung an dauerhafte Extremwerte für die Textbank

- **Warum:** Genervtheit, Aufmerksamkeit und Depression ruhen bei 0, Einsamkeit sinkt auf einer Party auf 0 und bleibt dort, Energie bleibt bei lauter Musik hoch. Die Textbank nimmt als Leitwert den Wert mit dem größten Abstand zur Mitte. Gemessen am 2026-11-18 mit dem Code von `TextBank::leadFor`: in Ruhe (30/0/0/50/0) je 33 % Genervt, Aufmerksam und Depression niedrig; auf der Party (95/0/40/0/0) je 33 % Einsam, Genervt und Depression niedrig; sobald Genervtheit und Depression nur 1 bis 4 Punkte haben, zu 100 % Einsam niedrig. Weil die Textbank heute praktisch nur einspringt, wenn das Textmodell zu spät ist, hat der Nutzer das Vorhaben am 2026-11-18 zurückgestellt.
- **Was (Entscheidungen des Nutzers vom 2026-11-18):** Gewöhnung plus gewichtetes Losen, nur für die Reaktionstexte (Auge, Geräusche und Lichtanweisung bleiben bei den echten Werten). Alle fünf Werte, beide Extreme. Ein Wert ist gewöhnt, wenn er 5 Minuten im selben Drittel war; ein Wechsel des Drittels macht ihn sofort wieder auffällig und startet die Zeit neu. Ein gewöhnter Wert zählt als Mitte: Gewicht 0 bei der Textbank, Stufe Mitte für das Textmodell, das dieselben Stufen bekommt. Unter den übrigen Werten wird der Leitwert linear nach Abstand zur Mitte gelost. Hat kein Wert ein Gewicht, wird nach den Rohwerten gelost. Offen: wie die Debug-Anzeige und die serielle Zeile einen gewöhnten Wert zeigen.
- **Wann:** wenn die Textbank wieder regelmäßig spricht, zum Beispiel nach der Verwerf-Schwelle oben, oder für Builds ohne Modell-Image.
- **Prüfen:** Host-Test der Regel, kurzer Gerätetest mit der Debug-Anzeige.

### Reaktionstexte als reimlose Kurzverse, Vortraining auf Lyrik

- **Stand:** Schritt 1 (Tonprobe) am 2026-11-19 erledigt, der Ton ist gewählt; Schritt 2 und die zweite Textbank sind offen.
- **Warum:** Größe, Roman-Vortraining und Token-Art haben die Texte nicht verbessert (Messungen vom 2026-11-18 und 2026-11-19). Verse sind kurz und in sich abgeschlossen wie die Reaktionstexte; eine ungewöhnliche Wortstellung liest sich im Vers als Stil statt als Fehler, eine Wiederholung als Refrain. Die Depressions-Gedichte gibt es schon, das Wesen bekäme eine durchgehende Stimme. Der Korpus muss ohnehin überarbeitet werden (Eintrag „Zweiter Sprach-Durchgang durch den Korpus“).
- **Präzisierung des Nutzers (2026-11-19, später am Tag):** Reime sind nicht gewollt, aber auch nicht schlimm, wenn sie da sind. Die Reimfrage muss jetzt nicht entschieden werden. Daraus folgt: Die Texte des Wesens und die Beispieltexte in Schritt 1 bleiben reimlos; für das Vortraining kommen gereimte kurze Gedichte als zweite Gruppe in Frage (zum Beispiel Busch, Morgenstern, Ringelnatz, Heine), und die Quellenliste soll beide Gruppen getrennt ausweisen. Ob gereimtes Material nützt oder mit seiner Wortstellung schadet, zeigt der Trainingsversuch in Schritt 2 (reimlos allein gegen gemischt).
- **Entscheidung des Nutzers:** keine Reime. Das passt auch technisch: Mit Wort-Tokens kennt das Modell den Klang eines Worts nicht und könnte nur gesehene Reimpaare wiedergeben.
- **Schritt 1, ohne Training:** Beispieltexte für drei Ereignisse in zwei oder drei reimlosen Formen schreiben lassen (freier Kurzvers, sehr knappe Zeilen, Spruchform); der Nutzer wählt den Ton.
- **Ergebnis Schritt 1 (2026-11-19):** 90 Beispieltexte für Klopfen, Schütteln und Jemand geht, je zehn pro Form, in den Stimmungen genervt, einsam, müde und fröhlich, alle unter 47 Zeichen, die freien Kurzverse im Ton von Rilke und Hölderlin (die Lieblingsdichter des Nutzers). Dem Nutzer gefällt die Probe „richtig gut“. Entscheidungen des Nutzers:
  - Die neue Stimme mischt alle drei Formen. Beispiele: „DU GEHST, / UND DER RAUM / WIRD WIEDER GROSS.“ (freier Kurzvers), „KLOPF? / DU? / ENDLICH.“ (knapp), „WER KLOPFT, GLAUBT AN EINE TÜR.“ (Spruch).
  - Ein Versbruch („/“ in den Beispielen) wird auf dem Panel eine neue Zeile. Das Panel braucht dafür ein Zeichen für den Zeilenbruch; das ist eine Firmware-Änderung (Textbank, Panel-Layout, Test in `tests/message_panel.cpp`).
  - Die Verse kommen in eine zweite Textbank mit eigenem Korpus-Ordner neben dem alten; eine Einstellung beim Bauen wählt, welche Textbank in die Firmware kommt, damit der Nutzer zur alten zurück kann. Gebaut wird sie in einer eigenen Session.
  - Die Beispieltexte mit Form und Ereignis liegen in der Tonprobe (Claude Doc „Kurzverse für das Wesen – Probe“); sie sind ein Anfang für den neuen Korpus.
- **Schritt 2, kleiner Trainingsversuch:** Vortraining auf reimloser, kurzer gemeinfreier Dichtung statt auf Romanen, einmal mit Wort-Tokens und einmal mit Wortteilen (Eindruck des Nutzers vom 2026-11-19: Mit Wortteilen bleibt mehr von den Formulierungen des Vortrainingstexts hängen; nicht gemessen).
- **Schritt 2, Vortraining gelaufen (2026-11-20, Host, nicht auf dem Gerät):**
  - Material, vom Nutzer freigegeben: reimlose Lyrik, Aphorismen und gehobene Prosa (Hölderlin mit Hyperion und Empedokles, Rilke mit Elegien, Requiem, Malte und Cornet, Klopstock, Trakl, Novalis mit Hymnen und Ofterdingen, Goethe mit freien Hymnen und Werther, Nietzsche, Stramm, Holz, Heine mit „Die Nordsee“, Luther 1912 mit Psalmen, Sprüchen und Prediger, Ebner-Eschenbach, Lichtenberg), ohne Romane. Hölderlins Gedichte 1784–1800 sind ganz dabei, auch die gereimten (Entscheidung des Nutzers). Werkzeug: `apps/mainframe/tools/text_model_verse_data.py`, Daten in `models/text-model/verse/` (nicht versioniert). Nach der Zuordnung zum Alphabet des Modells bleiben 46.580 Sätze mit 4,6 Mio. Zeichen. Alte Schreibung wird über eine kurze Regelliste angeglichen (th→t, ey→ei, giebt, Aether, Cultur), und zwar in allen Quellen, weil auch die Nietzsche- und Novalis-Ausgaben alt geschrieben sind. ß/ss entfällt, weil das Alphabet ß ohnehin als SS schreibt.
  - Zwei Läufe, beide 6 × 128, 10.000 Schritte, Batch 32, gleiche Lernrate: Wort-Tokens (Korpuswörter plus 5.915 häufige Lyrik-Wörter; sie decken 88,5 % der Wörter, Sätze werden an unbekannten Wörtern geschnitten) und eigenes BPE mit 2.000 Tokens (3,4 Zeichen pro Teil). Das sind 3,9 Epochen bei Wort-Tokens und 6,5 Epochen bei Wortteilen. Der Validierungs-Loss flacht bei beiden ab etwa Schritt 7.000 bis 9.000 ab, Wort-Tokens 4,86 pro Wort, Wortteile 3,97 pro Teil; die Zahlen sind wegen der verschiedenen Einheiten nicht vergleichbar.
  - Wörtliche Übernahme: In 27 Proben je Lauf (bis zum ersten Satzende, höchstens 64 Tokens) stehen höchstens 6 aufeinanderfolgende Wörter genau so im Material (Wendungen wie „DER HERR IST MEIN“), keine ganzen Zeilen. Wortteile erfinden Wörter (6 von 309 Wörtern, zum Beispiel KORINE, UNSTUDLICH), Wort-Tokens keine.
  - Blindlesen: 27 Paare freier Fortsetzungen zu Vers-Anfängen, A und B pro Paar zufällig zugeordnet (Artifact „Lyrik-Vortraining blind“, Schlüssel im Notizordner der Session unter `verse-pretrain/blind-key.tsv`). Urteil des Nutzers (2026-11-20): Wort-Tokens 13-mal besser, Wortteile 6-mal, 6-mal gleich, 2-mal beide unbrauchbar. Das spricht für Wort-Tokens, ist bei 19 entschiedenen Paaren aber nicht gesichert (Vorzeichentest, zweiseitig p ≈ 0,17). Anmerkungen des Nutzers: zu viele Bibelverse, und die Texte sind teilweise zu lang. Luther 1912 ist nur etwa 7 % des Materials; der biblische Ton kommt nach Deutung auch aus Zarathustra (11 %) und Klopstock, die Länge aus den langen Prosasätzen (Lichtenberg, Nietzsche, Malte, Ofterdingen, Hyperion).
  - Nachtraining und Proben zu Ereignissen warten auf den Vers-Korpus (Entscheidung des Nutzers vom 2026-11-20). Dasselbe gilt für den Vergleich der Ereignistreue aus der Hauptsession: ohne Vortraining, mit Vortraining, mit nach Wortfeldern markiertem Vortraining und mit gemischtem Nachtraining.
  - Wortfelder je Ereignis (erster Entwurf, gezählt im alten Korpus und im Lyrik-Text): Im Lyrik-Text sind die Felder für Klopfen (0,4 % der Sätze), Schütteln (1,0 %), Vorbeigehen (1,2 %) und Drehen (1,3 %) dünn. Die Felder für Ankommen (18 %), Gehen (10 %) und Doppelklopfen (9 %) bestehen dagegen aus Allerweltswörtern (DA, VOR, NOCH, WIEDER, WEG), die zu jedem Satz passen. Im alten Korpus enthalten nur 25 % der Bewegen-Zeilen und 26 % der Stoß-Zeilen ein Wort ihres Feldes. Die Tabelle liegt im Notizordner unter `verse-pretrain/word_fields_old_corpus.md`.
- **Schritt 3:** Korpus auf die gewählte Form umbauen, zusammen mit der kleinen Wortliste.
- **Quellen, in öffentlichen Katalogen geprüft (2026-11-19, nichts heruntergeladen):** gutenberg.org war während der Prüfung in Wartung; Größen stammen vom Spiegel gutenberg.pglaf.org und aus Zeno.org-Katalogseiten (etwa ±10 %), Ungeprüftes ist als „ungeprüft“ markiert. Zeno.org nennt seine Texte „Gemeinfrei“ und hat modernisierte Schreibung; Project Gutenberg nennt sie „Public domain in the USA“.
  - **Reimlos, nah an Rilke und Hölderlin:** Hölderlin, Gedichte 1800–1804 und der reimlose Teil von 1784–1800 (Zeno, etwa 340 KB), Hyperion (Zeno, 293 KB); Rilke, Duineser Elegien (Zeno, 42 KB) und Requiem (Zeno, 19 KB); Klopstock, Oden (Zeno, 391 KB, alte Schreibung); Trakl, Die Dichtungen (PG #29006, 120 KB); Psalmen, Sprüche und Prediger der Lutherbibel 1912 (eBible.org, etwa 380 KB, Quelle ungeprüft); Novalis, Hymnen an die Nacht (PG #43821, etwa 25 KB); Goethe, freie Hymnen (Zeno, etwa 20 KB); Nietzsche, Dionysos-Dithyramben (Zeno, etwa 25 KB).
  - **Weitere reimlose Texte:** Stramm (Zeno, etwa 25 KB), Holz, Phantasus (Zeno, 56 KB), Heine, Die Nordsee (in PG #3498, etwa 40 KB), Aphorismen von Ebner-Eschenbach (PG #77889, etwa 70 KB, alte Schreibung), Lichtenberg (Zeno, 653 KB), Nietzsche, Zarathustra (PG #7205, 547 KB) und Menschliches, Allzumenschliches (PG #7207, 583 KB), Goethe, Maximen und Reflexionen (Zeno, etwa 350 KB, ungeprüft).
  - **Gereimt, als zweite Gruppe:** der größte Teil von Rilke (Stunden-Buch, Buch der Bilder, Neue Gedichte, Sonette an Orpheus; etwa 370 KB), Busch, Morgenstern, Ringelnatz (Zeno), Heine, Buch der Lieder; zusammen etwa 1,1 MB.
  - **Menge:** reimlos etwa 5,4 MB, davon nur etwa 1,5 MB Verse; mit der gereimten Gruppe etwa 6,5 MB. Das ist ein Viertel bis ein Drittel der 35 Romane (angenommen 20–25 MB, nicht nachgemessen). Hölderlin, Klopstock, die Elegien und die Sonette gibt es nicht auf gutenberg.org, sondern auf Zeno.org mit einer HTML-Seite pro Gedicht oder im Deutschen Textarchiv in alter Schreibung (Lizenz dort ungeprüft).
  - Die ganze Liste mit Größen und Schreibung lag am 2026-11-19 in den Session-Notizen (`verse-pretraining-sources-2026-11-19.md`). Jeder Download wird dem Nutzer vorher mit Liste und Lizenz gezeigt.
- **Entscheidungen des Nutzers zu Schritt 2 (2026-11-19):**
  - Material: alle reimlosen Texte, Verse und Prosa (etwa 5,4 MB), nicht nur die 1,5 MB Verse.
  - Alte Schreibung (Klopstock, Ebner-Eschenbach) wird automatisch modernisiert (kurze Regelliste: th→t, ey→ei, ß/ss nach heutiger Regel), danach werden die häufigsten verbliebenen Altformen geprüft. Grund: Mit Wort-Tokens wäre „THRÄNE“ ein anderes Wort als „TRÄNE“.
  - Zwei Läufe mit gleicher Modellgröße und gleichen Schritten, einmal Wort-Tokens, einmal Wortteile; der Nutzer liest die Texte beider Läufe blind.
  - Schritt 2 läuft parallel zur zweiten Textbank, in einer eigenen Session. Vor dem Download zeigt diese Session dem Nutzer Liste und Lizenz.
- **Offen:** Das Ereignis muss im Text erkennbar bleiben (Korpus-Regel).
