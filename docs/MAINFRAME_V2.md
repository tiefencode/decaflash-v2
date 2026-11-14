# Mainframe v2 – Projektkontext

Stand: 25.09.2026, zuletzt am 14.11.2026 mit dem Code abgeglichen (Commit `2786c11`). Der Abgleich betraf die Tempo-/BPM-Arbeit vom 01.–03.10.2026, den aktiven Modulstand in A.1 und den Soll-/Ist-Vergleich in B.5; er war eine reine Lese- und Dokumentationsarbeit mit Hosttests, ohne Firmware-Build, Flash oder Gerätemessung. Eigenständiger V2-Ausgangspunkt: Commit `ed5dc7a` (`initial commit. copy from decaflash original`). Historische Ausgangsanalyse vom 22.09.2026: Commit `f9cabdf1d04d314f240faecbc239ec706aa4da84` im bisherigen Projekt mit anschließend beschriebenen Kommunikationsänderungen. Diese Änderungen belegen keine Lösung des gemeldeten Szenenproblems.

Dieses Dokument trennt **aus dem Code verifizierten IST-Zustand**, **vom Nutzer geplante v2** und **offene bzw. experimentelle Entscheidungen**. Es ist keine Implementierung oder Freigabe einer Implementierung. README bleibt der Einstieg für Betriebsbefehle; dieses Dokument bündelt den ausdrücklich angefragten v2-Kontext.

Der aktive zentrale Controller heißt **Mainframe**. Alle versionierten V2-Quellen und Dokumente verwenden diese Bezeichnung. Nur die lokale ignorierte Kopie des historischen M5Atom-Codes behält ihre alten Bezeichner.

Prüfumfang der historischen Ausgangsanalyse: sämtliche versionierten Firmware-Quellen und Header, gemeinsames Protokoll und Szenen, PlatformIO-Konfiguration, Cloudflare-Worker einschließlich Konfiguration, vorhandene Dokumentation und Konfigurationsvorlagen. Lokale Geheimnisse, generierte Build-Artefakte und Fremdbibliotheken sind kein Bestandteil der Quellcodeprüfung. Damals wurden Controller und Node nach dem Kommunikationsfix gebaut; keine Firmware wurde geflasht, keine Cloud-Aufrufe oder Hardwaremessungen wurden durchgeführt. Der aktuelle V2-Stand einschließlich Flash und Hardwareprüfung steht verbindlich in Abschnitt 0.

## 0. Verbindlicher V2-Stand vom 25.09.2026

Dieser Abschnitt und die nachfolgend korrigierten Angaben ersetzen widersprechende ältere Planungsannahmen. Der oben beschriebene vollständige Prüfumfang und die Builds gehören zur historischen Analyse. Im ersten Prüfschritt wurden Projektübernahme, Git-Konfiguration, ausgewählte Codepfade und USB-Inventar lesend geprüft; dabei wurde ausschließlich dieses Dokument aktualisiert. Anschließend wurde die V2-Dokumentation konsolidiert: README als kurzer Einstieg, dieser Kontext für Analyse/Entscheidungen, Worker-README für Cloud-Details und V1-Outline als historischer Entwurf. Diese ersten Dokumentationsschritte enthielten keine Builds oder Firmwareänderungen. Im folgenden Umsetzungsschritt wurden die Funkkennung geändert, ein isolierter Hardwaretest ergänzt, der Mainframe geflasht und der erste Offline-Funkpfad aktiviert. Keine Cloud-Aufrufe oder Deployments.

### Eigenständiges Projekt und Übernahme

- Arbeitsverzeichnis und eigener Git-Root: `/Users/tiefencode/Projekte/decaflash-v2`, eigenes `.git`; konfiguriertes Fetch-/Push-Ziel: `git@github.com:tiefencode/decaflash-v2.git`. Die Erreichbarkeit und der Remote-Inhalt wurden nicht geprüft. Der Arbeitsbaum war vor dieser Dokumentänderung sauber.
- Manuell übernommener Stand: M5Atom-Controller mit PDM-Audio/BPM, Audio-Follow, Matrix-VU/Text und optionaler Cloud-Anbindung; Node-Firmware mit Flashlight-/RGB-Ausgabe; gemeinsame Szenen, Clock-/Textprotokoll und ESP-NOW-Transport; Worker und Dokumentation.
- Beim initialen Vergleich entsprachen die geprüften Dateien in `apps`, `shared`, `include`, `workers` und `docs` dem alten Projekt bytegleich (generierte Worker-Abhängigkeiten und `.wrangler` ausgenommen). Dort wurden keine Symlinks oder mit dem alten Gegenstück gemeinsam genutzten Datei-Inodes gefunden. Secrets wurden nicht ausgegeben; die lokalen WLAN-/Cloud-Konfigurationsdateien sind ignoriert und nur ihre Vorlagen versioniert.
- Alte Hardware und altes Projekt bleiben unabhängig funktionsfähig und unverändert. Keine gemeinsam veränderlichen Quellcodedateien. V2 umfasst bei Bedarf eigene Mainframe- **und** Node-Firmware; gezielte Übernahme statt pauschalem Rewrite.
- Die README und Arbeitsregeln werden für das eigenständige V2-Repo gepflegt. Operative Verweise auf das alte Projekt und die alte Worker-Deployment-Anleitung wurden im Konsolidierungsschritt entfernt; historische Analysereferenzen bleiben als solche erhalten.
- Der aktive `mainframe`-Build zielt auf AtomS3R und enthält den validierten Bring-up für Speicher, Display, Button und Voice-Base-I²C sowie den Offline-ESP-NOW-Kern: Show-Start und Szenenwahl über den Frontbutton, Szenenbroadcast und Clock-Sync. Die Show startet mit 120 BPM; seit dem 03.10.2026 übernimmt die Show-Clock das bestätigte Tempo des PCM-Tempo-Trackers (siehe unten). Voice-Base-Audioanalyse, Auge/VU, Personality, IMU-Gesten und Textpanel sind enthalten (aktiver Modulstand in A.1). WLAN und Cloud sind im V2-Mainframe weiterhin nicht enthalten. `node` bleibt die M5Atom/FastLED-Firmware. Eine lokale ignorierte Kopie des früheren M5Atom-Controllers liegt unter `reference/m5atom_controller`; V2 übernimmt daraus bei Bedarf einzelne Bausteine, versioniert die Kopie aber nicht. Das ursprüngliche Projekt und seine Git-Historie sind maßgeblich.
- Der Worker wurde als Quellcode mitkopiert. Eine Übernahme seiner alten Cloudflare-Deployment-Verknüpfung ist nicht beauftragt; eine externe V2-Verknüpfung wurde nicht geprüft oder eingerichtet.

### Iris-VU auf Basis von Commit `0501020`

Der aktuelle Mainframe enthält Voice-Base-Capture und den prozeduralen Low-Poly-EyeRenderer. `BeatAnalyzer` und `AudioFollower` waren zum Zeitpunkt dieses Abschnitts der Tempopfad; seit Commit `30ac9a7` sind sie nicht mehr angebunden (siehe folgender Abschnitt). Die Iris nutzt jetzt den bereits DC-bereinigten mittleren Absolutpegel jedes 256-Sample-Blocks (16 ms), unabhängig vom Music-Gate. Eine eigene Display-Hüllkurve hat sofortigen Attack und maximal etwa 160 ms Release. Die vorläufigen absoluten Schwellen sind 80 (schwarz) und 2400 (Vollaussteuerung); Codec/Gain und Raumgeräusch müssen am Gerät geprüft werden. Bei fehlenden Audioblöcken wird das VU nach 250 ms schwarz, bei fehlgeschlagenem Capture sofort.

Nach dem ersten Hardwaretest (VU funktioniert laut Nutzer, Darstellung zu grob) besteht die Iris aus 36 unabhängigen, den vollen Iris-Kreis füllenden VU-Polygonen. Wie beim alten Brain-Meter bestimmt eine gemischte Reihenfolge die aktiven Flächen; alle 220 ms wechseln vier aktive/inaktive Plätze. Jede Fläche blendet separat auf/ab und durchläuft Schwarz → Dunkelblau `(8,34,110)` → Hellblau `(40,212,255)` → Magenta `(255,30,230)`. Auch bei Stille bleibt der vollständige Kreis in einem sichtbaren dunklen Blau. Der Audiopegel bestimmt für alle Flächen dieselbe Grundfarbe. Eine deterministische Helligkeitsabweichung von höchstens 25 Prozent wird erst danach auf jeden RGB-Kanal angewandt: Sie erzeugt facettierte hellere und dunklere Varianten derselben Grundfarbe, ohne die Farbskala pro Polygon zu verschieben. Die Iris- und Pupillenlogik bleibt davon unabhängig. Der Augapfel wird mit derselben bestätigten 16-Segment-/Zwei-Ring-Facettierung prozedural und flach schattiert in einen einzelnen 128×128-RGB565-Sclera-Buffer gezeichnet. Dieser Layer wird bewusst nur alle 125 ms aktualisiert; es gibt keine 45 vorberechneten Vollbild-Sprites mehr. Der Blick besteht weiterhin aus kurzen, s-förmig beschleunigten Sakkaden zu zufällig gewählten Zielen und anschließenden Fixationen; er kreist nicht dauerhaft. Bei hoher `attention` werden die Ziele deutlich weiter, die Sakkaden schneller und die Fixationen häufiger; bei 100 reicht der Blick bis ±22 bzw. ±12 Pixel und wechselt nach etwa 350–850 ms. Ab 90 `annoyance` legt sich ein schwarzes, mittig schweres Low-Poly-Oberlid über die Kugel und die Iris wechselt auf ein gesättigtes Dunkelrot → Rot → rotes Orangerot; dieselbe Grenze löst auch das Node-Lichtprofil aus. Ab 90 `loneliness` entsteht ein schwarzes Oberlid, das mittig spitz nach oben zuläuft, und der Blick sinkt leicht ab. Der bestehende 128×128-Ausgabe-Sprite bleibt erhalten. Die VU-Auswertung benötigt nur konstante Ganzzahlarbeit pro Audioblock und ist vom Tempopfad unabhängig.

Host-Prüfung: `sh tests/run_iris_vu.sh` prüft Gate, monotonen Pegel, Sättigung, Facettenanzahl, Release, Capture-Ausfall und Zeitüberlauf. Zusätzlich sind alle Hosttests über `sh tests/run_all.sh` auszuführen sowie `pio run -e mainframe`. Visuelle Abnahme, Pegelkalibrierung und Laufzeit-/Funkmessungen unter Audiolast stehen für diese Änderung noch aus.

### PCM-Tempo-Tracker: vom Experiment zur Show-Clock (30.09.–03.10.2026, Hardwareabnahme offen)

**Kurzfassung (Stand `2786c11`):** Die Show-Clock wird von `V2TempoTracker` (PCM-basiert) über die Bestätigungsstufe `V2TempoAdoption` gesteuert. `BeatAnalyzer` und `AudioFollower` sind seit Commit `30ac9a7` nicht mehr mit `main.cpp` verbunden; sie liegen noch im Quellbaum und werden nur von ihren Hosttests verwendet. Für den produktiven Pfad nach dieser Umstellung liegt in diesem Dokument und in den lokalen Gerätelogs **keine** Messung vor.

#### Verlauf laut Git-Historie

| Datum | Commits | Schritt |
| --- | --- | --- |
| 30.09. | `5a025c0` | Experimenteller `TempoTracker` (PCM, Vierband-Flux, Autokorrelation, Tempo-Lock) läuft parallel; `BeatAnalyzer` + `AudioFollower` steuern weiter die Clock. Sample-Zeitbasis und 8-KiB-PCM-Ring in `VoiceBaseInput`. |
| 01.10. | `2e04ca1`, `c018767` | Änderung an der AudioFollower-Reaktionszeit, unmittelbar revertiert. |
| 01.10. | `f37a876`–`cc7312e` | `TempoTracker` samt seinem Hosttest gelöscht. Neustart mit `BpmTracker`: ganzzahlig, arbeitet auf 16-ms-Pegel-Frames statt PCM, 80–180 BPM, ohne Songwechsel-Lock. Weiterhin nur Diagnosekandidat neben dem produktiven Pfad. |
| 01.–03.10. | `548cdbf`–`5f6b5cb` | Trace-Aufzeichnung vom Gerät (`BPM_TRACE3`, `SPECTRAL_TRACE`), eingecheckte Referenzfenster unter `tests/fixtures/bpm*` und Offline-Reports. |
| 03.10. | `30ac9a7` | Der PCM-Schätzer kehrt als `V2TempoTracker` zurück – derselbe Onset- und Korrelationskern wie `TempoTracker`, aber ohne Lock-Zustand – und wird Quelle der Show-Clock. `BeatAnalyzer`, `AudioFollower` und `BpmTracker` werden aus `main.cpp`/`VoiceBaseInput` entfernt. |
| 03.10. | `67c4f96` | `V2TempoAdoption`: Die Show übernimmt ein neues Tempo erst nach zwei übereinstimmenden Auswertungen. |
| 03.10. | `c2cc4aa`–`1588d7d` | `V2_TRACE`-Aufzeichnung, `feedOnsetTrace()` für Replays, Debug-Anzeige `Show-BPM/Kandidat`. |

`a986f20` (`tracker-v2`) ist der Merge dieses Zweigs und ändert keinen Inhalt.

#### Was die Show-Clock jetzt steuert

1. **Capture und Zeitbasis (`voice_base_input.*`):** Die Analysezeit stammt aus einem kontinuierlichen 16-kHz-Samplezähler, nicht aus der Main-Loop-Verarbeitungszeit. Der Capture-Callback meldet die zwei M5Unified-Aufnahmepuffer sofort wieder an und kopiert jeden fertigen 256-Sample-Block (16 ms) in einen 16-Frame-PCM-Ring (8 KiB PSRAM). Die Auswertung läuft im Main-Loop. Ein voller Ring wird gezählt; der folgende Sequenzsprung setzt den Tracker zurück, statt aus lückenhaftem PCM ein Tempo zu behaupten.
2. **Onset-Stärke (`v2_tempo_tracker.cpp`, `feed`):** Pro Block das Maximum aus breitbandigem Pegelanstieg, Bassanstieg (Hochpass nahe 40 Hz, zwei Tiefpassstufen nahe 180 Hz) und log-skaliertem Vierband-Spektralfluss (einpolige Grenzen bei etwa 200, 800, 2500 und 6000 Hz). Die Anstiege sind auf den aktuellen Pegel normiert; es gibt kein Music-Gate und keine absolute Onsetschwelle wie im alten Scanner. Kein FFT, keine zusätzliche Bibliothek.
3. **Einzel-Onsets:** lokales Maximum über `Mittelwert + 2,2 × mittlere Abweichung`, Refraktärzeit 96 ms; die letzten 16 Onset-Zeitstempel bleiben erhalten.
4. **Tempoauswertung (`updateTempo`, alle 250 ms):** erst ab 384 Frames (6,14 s) lückenloser Historie. Normierte Autokorrelation der bis zu 512 Onset-Frames (8,2 s), auf 32 ms dezimiert; Grobsuche 70–190 BPM in 2-BPM-Schritten, danach Verfeinerung um ±2 BPM. Das doppelte Tempo ersetzt den Korrelationssieger nur, wenn die Einzel-Onsets es um mindestens 3 Punkte besser stützen und seine Korrelation mindestens 35 % des Siegers erreicht. Ein direkter Mittelwert aus bis zu acht letzten Onset-Abständen (300–900 ms, jeweils höchstens 8 % Abweichung) ersetzt den Wert, wenn er ihm, seinem Doppelten oder seiner Hälfte auf ±4 BPM entspricht.
5. **Confidence:** `0,72 × Periodizität + 0,28 × Abstand zum besten Konkurrenten (mindestens 4 BPM entfernt)`, als 0–100; bei Übereinstimmung mit dem direkten Onset-Tempo mindestens 55. Das ist ein heuristischer Wert auf einer anderen Skala als die BeatAnalyzer-Confidence, keine kalibrierte Wahrscheinlichkeit.
6. **Kein Lock im Tracker:** `estimate().bpm` ist immer der aktuelle Rohwert. Der in `5a025c0` beschriebene Lock (vier nahe Schätzungen zum Erfassen, zwölf Gegenbelege zum Wechsel, Halb-/Doppeltempo als eine Familie) existiert nicht mehr. Ein Zeitsprung über 40 ms zwischen zwei Blöcken setzt den Tracker vollständig zurück.
7. **Übernahme in die Show (`v2_tempo_adoption.h`, `applyV2Tempo` in `main.cpp`):** nur bei laufender Show und frischem Audio (letzter Block höchstens 250 ms alt). Jede neue Auswertung wird genau einmal betrachtet. Verworfen wird ein Wert außerhalb 80–180 BPM, mit Confidence unter 30, ohne Onset oder mit letztem Onset älter als 1500 ms; das löscht auch einen angefangenen Kandidaten. Ein vom Showtempo abweichender Wert wird Kandidat; liegt die nächste Auswertung höchstens 4 BPM daneben, wird der gerundete Mittelwert beider übernommen. Das kostet etwa 500 ms und filtert einen einzelnen Ausreißer.
8. **Wirkung der Übernahme:** `currentBpm` wird gesetzt, der Beat springt auf 1 des nächsten Takts, die Phase wird auf `letzter Onset + Beatintervall` gelegt, und es geht sofort ein `ClockSync` an die Nodes. Dazwischen läuft die Clock frei: Jeder Beat plant den nächsten als `jetzt + Beatintervall`; `ClockSync` folgt auf Beat 1 jedes Takts. Ohne Tempoänderung gibt es keine Phasenkorrektur gegen die Musik.
9. **Kein Follow-Zustand:** `Searching`/`Locked`/`Holdover`, Music-Gate und die 1-BPM-Schrittbegrenzung des AudioFollowers entfallen. Bei Stille, Break oder fehlendem Audio behält die Show ihr letztes Tempo; sie fällt nicht auf 120 BPM zurück. Die Übernahmestufe kennt keine Tempofamilien: Zwei übereinstimmende Auswertungen im halben oder doppelten Tempo würden übernommen.
10. **Weitere Abnehmer:** Die Personality erhält BPM, Confidence und letzten Onset direkt aus dem Tracker (siehe B.6). Das Debug-Overlay (langer Frontbutton-Druck) zeigt oben `Show-BPM/Kandidat` neben dem weiter sichtbaren Beat-Dot; die Confidence steht nur in der USB-Telemetrie.

#### Im Quellbaum vorhanden, aber nicht produktiv angebunden

`build_src_filter` übersetzt alle Dateien unter `apps/mainframe/src/`; die folgenden Module werden deshalb mitkompiliert, aber von keinem produktiven Pfad aufgerufen:

- `beat_analyzer.*`, `audio_follower.*`: früherer Tempo-/Follow-Pfad (portierter V1-Scanner aus A.4). Nur noch `tests/run_beat_analyzer.sh` und `tests/run_audio_follower.sh`.
- `bpm_tracker.*`: Pegel-Frame-Schätzer vom 01.10. Nur noch Hosttest und die beiden Trace-Reports.
- `spectral_onset_features.*`: zwölf Goertzel-Bänder (125 Hz bis 6 kHz) je Block; ausschließlich im Environment `mainframe-spectral-capture` aktiv. `V2TempoTracker` verwendet diese Werte nicht.

Ob diese Module bleiben, als Referenz dienen oder entfernt werden, ist nicht entschieden.

#### Diagnose-Environments

Alle erweitern `mainframe` und sind nur für Messungen gedacht; danach wieder den normalen `mainframe`-Build flashen.

| Environment | Zusätzliche USB-CDC-Ausgabe |
| --- | --- |
| `mainframe-beat-tracker` | einmal pro Sekunde `BEAT_V2`: Tracker-BPM/Confidence, Rohwert, direktes Onset-Tempo, Periodizität, Doppeltempo-Kandidat, `show_bpm`, Zahl der Auswertungen, mittlere/maximale Tracker-Laufzeit, Ring-Drops, Requeue-Fehler, Ring-High-Water, freier PSRAM/interner RAM. |
| `mainframe-bpm-capture` | zusätzlich je 16-ms-Block `BPM_TRACE3`: Sequenz, Audiozeit, Pegel, perkussiver Pegel und vier Bandpegel (Bass, Low-Mid, Mid, High). Zeilen werden bei vollem Sendepuffer ausgelassen statt die Aufnahme zu blockieren. |
| `mainframe-spectral-capture` | zusätzlich je Block `SPECTRAL_TRACE`: Sequenz, Audiozeit, Pegel, perkussiver Pegel und zwölf Bandenergien; gleiches Auslassen bei vollem Puffer. |
| `mainframe-v2-capture` | je Block `V2_TRACE`: Sequenz, Audiozeit und Onset-Stärke des `V2TempoTracker` in Promille; ohne `BEAT_V2`-Telemetrie. |

Die Kommentare zu diesen Environments in `platformio.ini` beschreiben teilweise noch den Stand vor `30ac9a7` (BeatAnalyzer als produktiver Pfad, Spektralwerte als V2-Eingang).

#### Fixtures und Hosttests

| Skript | Prüft | Art |
| --- | --- | --- |
| `sh tests/run_v2_tempo_tracker.sh` | Synthetische 16-kHz-Kickspuren mit 100, 117, 126, 128, 160 und 180 BPM (je höchstens 2 BPM Abweichung, Confidence ≥ 45), Reset nach Capture-Lücke, Übergang 100→120 BPM ohne Neustart, Replay einer synthetischen Onset-Spur über `feedOnsetTrace()`. | Assertions |
| `sh tests/run_v2_tempo_adoption.sh` | Einzelner Ausreißer ändert das Showtempo nicht, dieselbe Auswertung zählt nur einmal, niedrige Confidence löscht den Kandidaten. | Assertions |
| `sh tests/run_bpm_tracker.sh` | `BpmTracker` (nicht produktiv) mit synthetischen Pulsen bei 100/120/160/180 BPM und Stille, mit UB-Sanitizer. | Assertions |
| `sh tests/run_spectral_onset_features.sh` | Bandtrennung und Anstiegswerte der zwölf Goertzel-Bänder. | Assertions |
| `sh tests/run_bpm_trace_report.sh` | Spielt `tests/fixtures/bpm/atom-*.csv` (Pegel und perkussiver Pegel, 01.10.) durch `BpmTracker`. | Report, schlägt nur bei unlesbaren Daten fehl |
| `sh tests/run_bpm_band_trace_report.sh` | Spielt `tests/fixtures/bpm-bands/atom-*.csv` (zusätzlich vier Bänder, 03.10.) in sieben Eingangskombinationen durch `BpmTracker`. | Report, schlägt nur bei unlesbaren Daten fehl |

Die Fixtures sind je fünf 12-Sekunden-Fenster (750 Frames) echter Geräte-Features zu handgetappten Referenzen 98, 100, 120, 160 und 180 BPM. Beide Reports bewerten den **nicht produktiven** `BpmTracker`; die Fixture-README unter `bpm-bands` nennt ihn „V2-Kern“. Sie enthalten kein PCM und keine Onset-Stärke und können `V2TempoTracker` deshalb nicht bewerten. Der erste Report zählt für `BpmTracker` zum Beispiel bei 98 BPM 0 von 16 und bei 180 BPM 0 von 16 korrekte Auswertungen (6 im halben Tempo); er ist ein Befund über diesen verworfenen Kandidaten, kein Regressionstest. Eine vom Gerät aufgezeichnete `V2_TRACE`-Referenz ist nicht eingecheckt: `feedOnsetTrace()` wird nur gegen eine synthetisch erzeugte Spur geprüft.

`sh tests/run_all.sh` führt alle 15 Hosttests aus; sie bestehen am Stand `2786c11`.

#### Messungen: was belegt ist und was nicht

Die folgenden Gerätewerte vom 30.09.2026 stammen vom Vorgänger `TempoTracker` mit Lock, als er parallel zum damaligen produktiven Pfad lief. Onset- und Korrelationsstufe sind im heutigen `V2TempoTracker` unverändert; die Aussagen über gelockte Werte gelten für ihn nicht.

- Speicher und Laufzeit: 0 Ring-Drops, 0 Requeue-Fehler, maximal acht gepufferte Frames, 8.103.323 Byte freier PSRAM und 257.372 Byte freier interner RAM bei laufender Musik. Tempoauswertung maximal 2,51 ms je 250-ms-Aktualisierung, gemittelt rund 0,33 ms je 16-ms-Block. Der PCM-Ring belegt 8 KiB PSRAM, die Onset-Historie rund 2 KiB und ihre dezimierte Korrelationsansicht 1 KiB; kein zusätzlicher Task-Stack.
- Buildgrößen vom 30.09.: Produktionsbuild 54.780 Byte statisches RAM und 1.059.417 Byte Flash. Für den heutigen Stand wurden sie nicht neu erhoben.
- Referenztracks: 126 BPM über 28 Sekunden durchgehend 125–127 BPM. Handgetappter 124-BPM-Track nach der Vierband-Änderung 123–128 BPM. Bei 180 BPM pendelte der Rohwert zwischen 178/180 und 89/90; der damalige Lock hielt 178. Im A/B-Test hielt der 100-BPM-Lock Rohwert-Ausreißer zurück, der reale 120-BPM-Track lieferte keinen stabilen 120er-Kandidaten.

Die lokalen, nicht versionierten Gerätelogs unter `logs/` enden für die `BEAT_V2`-Telemetrie am 03.10.2026 um 17:14 Uhr und damit vor der Umstellung der Clock-Quelle (`30ac9a7`, 17:30 Uhr); ihr Format mit `legacy_bpm`/`v2_bpm` gehört zum `BpmTracker`-Diagnosestand. Keine Logzeile enthält das Feld `show_bpm` des heutigen Formats. Das einzige Log nach der Umstellung ist eine `V2_TRACE`-Aufzeichnung von 348 Zeilen (rund 5,6 s), kürzer als die 384 Frames, die der Tracker für seine erste Auswertung braucht.

ESP-DSP/FFT bleibt bewusst unintegriert: Die offizielle Apache-2.0-Bibliothek ist ein ESP-IDF-Component, nicht eine sauber unterstützte Arduino/PlatformIO-Abhängigkeit; ein Frameworkwechsel ist dafür nicht vorgesehen.

#### Offene Hardwareprüfungen für den produktiven Tempopfad

1. Feststellen, welcher Stand auf dem AtomS3R geflasht ist; ein Flash von `2786c11` ist nirgends protokolliert.
2. Referenztracks (mindestens 98, 100, 120, 124, 126, 160, 180 BPM) mit `mainframe-beat-tracker` aufzeichnen: `bpm`, `raw_bpm`, `confidence`, `show_bpm`, Zeit bis zur ersten Übernahme, Ausreißer. Insbesondere der am 30.09. instabile reale 120-BPM-Track.
3. Halb-/Doppeltempo am Gerät: Springt `show_bpm` bei 160/180 BPM zwischen den Oktaven, weil die Übernahmestufe keine Tempofamilien kennt?
4. Verhalten bei Break, Stille, Sprache und Songwechsel: Die Show behält das letzte Tempo ohne Holdover-Zustand; Dauer bis zur Übernahme eines neuen Tempos messen.
5. Phasenlage: Abstand des Beat-Dots und der Node-Beats zur Musik über mehrere Minuten bei unverändertem Tempo, da die Phase nur bei einer Tempoübernahme neu gesetzt wird.
6. Wirkung des Beat-/Takt-Sprungs bei jeder Übernahme auf laufende Node-Szenen.
7. Laufzeit und Ringverhalten des heutigen Trackers unter Display-, IMU- und Funklast (`tracker_max_us`, `analysis_drops`, `backlog_max`) neu messen.
8. Personality-Schwellen: `personality.cpp` verlangt für Beat-Bonus und Tempovertrauen weiterhin Confidence ≥ 68, ein Wert aus der BeatAnalyzer-Skala. Ob `V2TempoTracker` ihn mit realer Musik erreicht, ist ungeprüft (Hosttest: 55–83 bei synthetischen Kicks).
9. Geräte-`V2_TRACE` in ausreichender Länge aufzeichnen und als Fixture einchecken, falls der produktive Tracker gegen echte Spuren regressionsgeprüft werden soll.

### Creature-Sound-Audition vor der Analyse (28.09.2026)

Mikrofon und Speaker teilen auf der Atomic Voice Base denselben I2S-Pfad. Daher hat der Speaker nur vor der Analyse Besitz dieses Pfads: Nach einem Neustart bleibt das Mikrofon aus und ein kurzer eigener Handheld-Adventure-Jingle läuft. Die Animation zeigt dazu drei einfache, frontal wachsende und abnehmend hüpfende Kreise für Sclera, Iris und Pupille; alle drei nutzen denselben starken Ease-in-Skalierungsfaktor vierter Ordnung und beschleunigen gemeinsam erst unmittelbar vor dem Blitz. Die einfache Iris ist bewusst entsättigtes Nachtblau. Vier versetzte Sclera-Kreise simulieren das spätere Licht von oben rechts ohne einen teuren Pixelverlauf. Ein weißer Vollbild-Blitz blendet über rund 550 ms gleichmäßig über der bereits gerenderten detaillierten Augenansicht aus. Im Idle atmet das vollständige Auge über fünf Sekunden zwischen 95 und 100 Prozent seiner Größe; der vorhandene Sclera-Sprite wird dafür nur skaliert, Iris und Pupille verwenden denselben Faktor. Die früher vollständige Phrase-Audition war nur ein Debug-Schritt und ist entfernt. Loneliness besitzt bewusst keine DOWN-Phrase.

Der erste kurze Frontbutton-Klick startet weiterhin die Show und fordert die Analyse an. Läuft die Audition noch, endet sie zuerst vollständig; danach startet VoiceBaseInput genau einmal. Ab dann bleibt das Mikrofon aktiv. Es gibt keine Creature-Sounds, keinen I2S-Wechsel und keine Pause für VU, BPM, Bass oder Soundanalyse während des Betriebs.

Die Crossing-Policy ist nach der Start-Audition bis zum ersten Frontbutton-Klick aktiv: Annoyance, Depression, Attention und Loneliness haben UP-Schwellen bei 10, 30, 50, 70 und 90. Annoyance, Depression und Attention haben dieselben fünf DOWN-Schwellen; Loneliness bleibt beim Fallen stumm. Energy reagiert nur bei 20 abwärts und 80 aufwärts. Jeder Stimmungstyp besitzt seinen eigenen Cooldown von 10000 ms. Während eine PCM-Phrase läuft, werden andere Crossings verworfen und nicht nachgeholt. Mit dem ersten Frontbutton-Klick endet die PCM-Ausgabe vor dem Start des Mikrofons; danach läuft die Audioanalyse ununterbrochen.

Vor dem ersten Analyse-Klick ist Energy zusätzlich ein sichtbarer Creature-Akku statt eines Musikwerts: Er startet bei 100 und fällt langsam auf 0. Bei 20 startet ein müder 10-Sekunden-Timer; ohne IMU-Ereignis schläft die Creature anschließend ein, bei 0 unmittelbar. Während des Schlafs laufen die übrigen Stimmungen weiter, Schwellen-Phrasen werden verworfen und nicht nachgeholt; der Akku lädt sich dabei langsam wieder auf. Bis Energy 25 erreicht sind, bleibt der Schlaf durch Ereignisse unweckbar; danach weckt das nächste IMU-Ereignis sie wie bisher. Erreicht der Akku 100, wacht sie zusätzlich selbstständig auf. Die Anzeige schließt beziehungsweise öffnet dazu zwei gebogene schwarze Lider; im vollständig geschlossenen Zustand bleibt eine deutlich lesbare Augenlinie. Der freie Speaker erzeugt im Schlaf in deterministischen Abständen von 30–65 Sekunden ein leises Schnarchen. Dieser Ersatzpfad endet mit dem Analyse-Klick und beeinflusst den musikgetriebenen Energy-Pfad nicht. Depression bleibt derselbe gemeinsame Wert: Einsamkeit oberhalb 80 erhöht ihn langsam, Attention oberhalb 80 senkt ihn gleich schnell; nach dem Analyse-Klick kommen Tempo und Bass als Musikanteile hinzu.

### Funktrennung im Code umgesetzt, Hardwareprüfung offen

V2 verwendet Magic `0x44434632` (`DCF2`), V1 unverändert `0x4443464C` (`DCFL`). Payload-Version `13`, Paketlayouts und ESP-NOW-Kanal `1` bleiben gleich. Alle Sender erzeugen den gemeinsamen V2-Header; alle vier Node-Empfangspfade prüfen ihn vor dem Vormerken eines Pakets. Die Prüffunktion liegt nun im hardwareunabhängigen `protocol.h` und wird vom Transport weiterverwendet.

`sh tests/run_protocol_identity.sh` prüft mit den echten V2-Paketkonstruktoren und dem eingefrorenen V1-Headervertrag die gegenseitige Ablehnung von Szene, Clock, Hello und Text, einschließlich Text-Cancel für Flashlight/RGB. Außerdem werden gültige V2-Header, falsche Version/Typ/Kennung und unveränderte Paketgrößen geprüft. Diese Host-Tests belegen den Headervertrag, nicht Paketempfang oder Ausführung auf Hardware. V1-Quellen bleiben unverändert; beide V2-Gerätetypen müssen später mit der neuen Kennung geflasht werden. Die Kennung trennt Installationen, ist aber keine Authentifizierung.

Es gibt weiterhin keine ACKs, Sessions oder Szenentabellen-Prüfsummen. Die Ursache des gemeldeten Szenenproblems ist offen; diese Änderung ist kein Szenenfehler-Fix.

### Hardwarestand und Grenzen der Bestätigung

Laut Nutzer sind **AtomS3R und Atomic Voice Base angekommen und angeschlossen**. Zielausstattung: ESP32-S3-PICO-1-N8R8, 8 MB Flash, 8 MB PSRAM, 128×128-Display, BMI270, Frontbutton, Mikrofon und Speaker. Keine Kamera und kein zweiter ESP/Controller im Mainframe. U185/STHS34PF80 fehlt noch und wird vorerst nicht benötigt.

Die Hardwareprüfung am angeschlossenen `/dev/cu.usbmodem101` wurde durchgeführt. Der geflashte AtomS3R meldet ESP32-S3-PICO-1 Revision 0, 8.388.608 Byte Flash, 8.385.783 Byte PSRAM, M5Unified-Board-ID 18 und ein 128×128-Display; der 64-KiB-PSRAM-Schreib-/Lesetest liefert `PASS`. Die Voice-Base-I²C-Probe bestätigt ACKs an `0x18` und `0x43`. Das ist eine elektrische Busbestätigung, kein Nachweis von Codec oder Audioaufnahme. Der Offline-Mainframe meldet nach dem Flash `radio=READY`, `show=IDLE`, Szene 0 und 120 BPM. Über USB wurde kein weiterer serieller Node sichtbar; daraus folgt keine Aussage über separat versorgte Funk-Nodes.

Vor dem Port sind tatsächliche Varianten und Pinbelegung zu bestätigen. Die vorhandenen Herstellerhinweise nennen GC9107 und ST7735 als Displaytreiber; die Revision des angeschlossenen Geräts ist offen. Voice Base braucht I²S/Codec statt Unit-Mini-PDM. Mit späterem TMOS entstehen drei getrennte I²C-Pinpaare bei zwei Hardware-I²C-Controllern; die Busverwaltung bleibt offen. Beim Vorbereiten des Hardwaretests erneut geprüft: Die [AtomS3R-Dokumentation](https://docs.m5stack.com/en/core/AtomS3R) nennt seit 14.05.2026 ST7735 statt GC9107; die [Kombinationsseite](https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot) nennt weiterhin GC9107. Die konkrete Displayrevision bleibt am Gerät zu prüfen.

### Verbindlicher USB-CDC-Debugpfad und Renderer-Benchmark (30.09.2026)

Der Mainframe verwendet USB CDC/JTAG mit 115200 Baud. Die Buildflags aktivieren USB CDC, und `setup()` ruft zusätzlich ausdrücklich `Serial.begin(115200)` auf. Diese Initialisierung wartet **nicht** auf einen Host: Das Gerät bleibt daher ohne angeschlossenen Rechner vollständig bootfähig, während Boot-, Audio- und Benchmark-Ausgaben zuverlässig verfügbar sind. Ein erneutes Entfernen dieses expliziten Aufrufs würde die serielle Diagnose wieder unzuverlässig machen.

Der bestätigte Geräteport ist aktuell `/dev/cu.usbmodem101`. In einem interaktiven Terminal ist der Standardweg `pio device monitor -e mainframe --port /dev/cu.usbmodem101`; zuerst mit `pio device list` prüfen, weil der Port nach Hardwarewechsel abweichen kann. Nicht-interaktive Umgebungen ohne TTY können den PlatformIO-Monitor nicht starten; dort ist ein einfacher lesender USB-CDC-Client statt einer Firmwareänderung erforderlich.

Für reale Renderer-Messungen wird `mainframe-eye-procedural` geflasht. Er entspricht visuell dem Produktionsrenderer (48 Facetten), aktiviert aber ausschließlich diagnostische 10-Sekunden-Reports für Framezeiten, Eye-Speicher, Audio-/Sensor-Cadence und Sendeablehnungen. Der normale `mainframe`-Build enthält diese Benchmark-Ausgabe nicht. Tests immer mit aktivierter Audioanalyse durchführen und nach der Messung wieder den normalen `mainframe`-Build flashen.

Der Emoji-Atlas wird jetzt nur beim Glyph-Wechsel in einen einzelnen 32×32-RGB565-Cache dekodiert (2 KiB PSRAM) und danach geblittet. Die bestätigte Messung zeigte vier PNG-Dekodierungen gegenüber 366 Cache-Blits; wiederkehrende Panel-Frames lagen bei etwa 13,9 ms statt rund 22,8 ms beim früheren PNG-pro-Frame-Pfad. Die einmalige Cache-Befüllung ist mit rund 37 ms weiterhin sichtbar teuer, liegt aber nicht im Wiederholpfad.

### Produktziel, Reihenfolge und nächster kleiner Schritt

Die Show funktioniert vollständig offline; Internet und KI bleiben optionale Enhanced Features. Audio und Node-Kommunikation haben Vorrang vor Display und Cloud. Auge/VU, lokale Personality-States und Reaktionstexte gemäß B.2/B.3 sind bestätigt. IMU-Ereignisse und eine erste stimmungsabhängige Lichtvorgabe sind inzwischen umgesetzt (B.6); TinyML bleibt experimentell, ein lokales Sprachmodell steht zuletzt und wäre ausschließlich der „Mund“.

Zuerst Projektstand, Git-Ziel und Hardware klären, dann eine überschaubare eigenständige Repo-Struktur abstimmen. Als schlanker Ausgangspunkt bleiben `apps/mainframe`, `apps/node` und `shared/include` im neuen Repo bestehen und werden gezielt portiert.

Die Funktrennung ist im Code und im geflashten Mainframe umgesetzt. Erste funktionale Priorität bleibt jetzt der End-to-End-Test mit einem geflashten V2-Node: Mainframe-Start, Szenenwechsel, Clock und gegenseitige Funktrennung. Danach Audio/BPM, Auge/VU, Personality/Text, weitere Sensorik und Lichtreaktionen; TinyML später, lokales Sprachmodell zuletzt.

Tatsächlicher Verlauf bis 14.11.2026: Auge/VU, Personality, IMU-Gesten, Textpanel und der neue Tempopfad wurden umgesetzt, bevor der End-to-End-Test und die Szenenanalyse abgeschlossen waren. Der Soll-/Ist-Vergleich steht in B.5; die weitere Reihenfolge ist eine offene Nutzerentscheidung.

### Minimaler S3R-Hardwaretest

Dieser Abschnitt beschreibt den isolierten Bring-up-Test vom 25.09.2026; damals baute `pio run -e mainframe` ausschließlich `apps/mainframe/src/main.cpp`. Heute übersetzt dasselbe Environment die vollständige Mainframe-Firmware unter `apps/mainframe/src/`, und der isolierte Test existiert nicht mehr als eigener Build. Konfiguration: Espressif32 6.13.0, Arduino, `esp32-s3-devkitc-1` als Basis, 8 MB Flash, QIO/Octal-PSRAM, USB CDC/JTAG, M5Unified 0.2.23 und M5GFX 0.2.30. Orientierung: [Hersteller-PlatformIO-Beispiel](https://docs.m5stack.com/en/core/AtomS3R), mit expliziter Flashgröße und fixierten Bibliotheksversionen. Die Bibliothek erkennt das Board; ihre gemeldete ID ist kein unabhängiger Beweis der Hardwarevariante.

Durchgeführter und noch offener Testablauf am bestätigten S3R:

1. **Erledigt:** Serielle Ausgabe bei 115200 bestätigt Chip/Revision, Flash und PSRAM, Board-ID, 128×128 und `sample64k=PASS`.
2. **Offen:** Display visuell prüfen: Farbfelder und Text müssen vollständig und ohne Versatz sichtbar sein. Gemeldete Abmessungen allein bestätigen den Displaytreiber nicht.
3. **Offen:** Frontbutton drücken: Der Mainframe muss die Show starten beziehungsweise die Szene weiterschalten.
4. **Erledigt:** I²C-Probe auf Voice-Base-Pins SDA38/SCL39 bestätigt ACKs an `0x18` und `0x43`. Sie nutzt den anderen Controller als der interne Display-/IMU-Bus. ACK-Adressen belegen nur Erreichbarkeit, noch keine Codec-Identität oder Audiofunktion.

Der damalige Test initialisierte keine Mikrofonaufnahme, Speaker-Ausgabe, Funk- oder Cloudverbindung und enthielt keine lokalen Secrets. TMOS blieb außen vor. Die Punkte 2 und 3 sind nicht als formale Abnahme protokolliert; Display und Frontbutton werden seither in allen späteren Schritten benutzt (Iris-VU laut Nutzer funktionsfähig, Show-Start über den Button). Die I²S-Aufnahme über die Voice Base ist inzwischen der produktive Audiopfad; Gain, Pegelschwellen und Aufnahmequalität sind weiterhin nicht kalibriert. Hersteller-Pinreferenz: [AtomS3R + Voice Base](https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot).


## A. Verifizierter IST-Zustand aus dem Code

### A.1 Architektur und Zuständigkeiten

| Bereich | Tatsächlicher Stand | Quellen |
| --- | --- | --- |
| Build | Aktiver Mainframe: AtomS3R, Espressif32 6.13.0, M5Unified 0.2.23 und M5GFX 0.2.30. Aktiver Node: M5Atom mit FastLED 3.10.3. Der Node folgt nur tatsächlich eingebundenen Headern (`chain`), weil FastLEDs tiefe Quellsuche optionale, unbenutzte Pfade einbezieht. FastLED 3.10.5 lieferte auf dem angeschlossenen Node eine weißlich-statische Ausgabe; der direkte Rücktest mit 3.10.3 stellte Demo und Mainframe-Szenen wieder her. Der frühere M5Atom-Controller wird nicht kompiliert. | `platformio.ini` |
| Historischer M5Atom-Controller (V1) | Hauptloop für Mikrofon, Audio-Follow, Master-Clock, Szenen, Funkverwaltung und Matrix-UI. Boot zunächst nicht live; kurzer Tastendruck aktiviert die Show. Eine lokale ignorierte Lesekopie liegt unter `reference/m5atom_controller`; maßgeblich bleibt der historische V1-Stand. | Historischer V1-Stand |
| Audio (V1, historisch) | Unit Mini PDM; Pegel, Onsets, Tempo, Confidence, VU und optionale Aufnahme. | `pdm_microphone.{h,cpp}` |
| Clock-Follow (V1, historisch) | Eigenständige Zustandsmaschine zur Übernahme der Audio-Schätzung in die Master-Clock. | `audio_follow.{h,cpp}` |
| Node | Eine gemeinsame Firmware für Flashlight oder RGB; Typ und Rolle in Preferences gespeichert. UV ist nur Enum, kein unterstützter Renderer. | `apps/node/src/main.cpp`, `node_output.*` |
| Szenen | Gemeinsame Definitionen für Mainframe und lokale Node-Demo; zwei Szenen: die bestehende Szene 1 und die aus der Git-Historie wiederhergestellte Szene „dynamisch“. | `shared/include/scene_programs.h`, `apps/node/src/node_programs.h` |
| Transport | ESP-NOW-Broadcast auf Kanal 1, gemeinsame C++-Strukturen, eigenes Protokoll v13 mit fünf Nachrichtentypen. | `shared/include/protocol.h`, `espnow_transport.h` |
| Text (V1, historisch) | Matrix-Scrolltext sowie gesonderte beatgebundene Morse-Lichtausgabe auf Nodes. Der V2-Node kann `NodeText` weiterhin empfangen; der V2-Mainframe sendet es derzeit nicht. | `text_playback.cpp`, `node_text_channel.h`, V1-`main.cpp`, `apps/node/src/main.cpp` |
| Cloud (V1, historisch) | Ein FreeRTOS-Worker auf Core 0 für WLAN/HTTPS; Cloudflare vermittelt AudD und Textgenerierung. | `api_client.cpp`, `wifi_manager.cpp`, `ai_mode.cpp`, `workers/decaflash/src/index.js` |

Die als „V1, historisch“ markierten Zeilen beschreiben den früheren M5Atom-Controller. Die lokale Lesekopie ist ignoriert und wird nie gebaut oder versioniert. Die Abschnitte A.4 (BPM-Scanner), A.6 (Matrix-VU/Text) und A.7 (WLAN/Cloud) analysieren ebenfalls diesen V1-Stand; sie sind als solche gekennzeichnet.

Aktiver S3R-Mainframe am Stand `2786c11` (`apps/mainframe/src/`):

| Bereich | Tatsächlicher Stand | Quellen |
| --- | --- | --- |
| Audio-Capture | Voice Base über M5Unified/I²S, 16 kHz, 256-Sample-Blöcke, Sample-Zeitbasis und PCM-Ring im PSRAM. Start erst nach dem ersten Frontbutton-Klick, weil Mikrofon und Speaker denselben I²S-Pfad teilen. | `voice_base_input.*` |
| Tempo und Show-Clock | PCM-Tempo-Tracker mit Vierband-Spektralfluss und Autokorrelation; die Show übernimmt ein Tempo nach zwei übereinstimmenden Auswertungen. Details und offene Hardwareprüfungen in Abschnitt 0. | `v2_tempo_tracker.*`, `v2_tempo_adoption.h`, `main.cpp` |
| VU und Audiomerkmale | Iris-VU aus dem Blockpegel; Stille-Erkennung und breitbandiger Bassanteil für die Stimmung. | `iris_vu.h`, `audio_mood_features.*` |
| Personality | Fünf lokale Stimmungswerte, Creature-Akku und Schlaf vor dem Analysestart, daraus abgeleitete Node-Lichtvorgabe. | `personality.*`, `main.cpp` |
| Bewegung | BMI270 über M5Unified, Abfrage höchstens alle 20 ms; Klassifikator für Tap, MultiTap, Impact, Shake, Rotate, Tilt und Move. | `motion_events.*` |
| Anzeige | Prozedurales Low-Poly-Auge mit Iris-VU, Lidern, Boot-Animation, Debug-Overlay und Textpanel mit eigener Pixelschrift und Emoji-Atlas. | `eye_renderer.*`, `message_panel.*`, `panel_glyphs.*`, `panel_text_sprites.h` (generiert) |
| Speaker | Start-Jingle, Stimmungs-Phrasen und Schnarchen über den Voice-Base-Speaker, ausschließlich vor dem Analysestart. | `sound_engine.*`, `sound_personality.*` |
| Funk | `MainframeHello`, `SceneSelect`, `ClockSync` und `NodeVisualState` als ESP-NOW-Broadcast. | `main.cpp`, `shared/include` |
| Nicht angebunden | `beat_analyzer.*`, `audio_follower.*`, `bpm_tracker.*` werden mitkompiliert, aber nicht aufgerufen; `spectral_onset_features.*` nur im Diagnose-Environment. | Abschnitt 0 |

Im V2-Mainframe **nicht** vorhanden sind WLAN, Cloud-Anbindung, Audioaufnahme für AudD, `NodeText`-Versand, TMOS-Auswertung, TinyML und ein lokales Sprachmodell. Eine Frequenzbandanalyse existiert nur als leichtgewichtige Filterbank im Tempo-Tracker und als Bassindikator für die Stimmung, nicht als FFT oder Bass/Mid/High-Ausgang für die Lichtsteuerung.

Der Mainframe arbeitet kooperativ im Arduino-Loop mit `delay(5)` je Durchlauf. Die Aufnahme wird im M5Unified-Callback fortgeschrieben; die Auswertung der gepufferten Blöcke läuft im Hauptloop und nicht in einer eigenen, fest getakteten Analysetask. Der V2-Mainframe startet keine weitere eigene Task.

### A.2 Mainframe ↔ Node: Datenmodell und Lebenszyklus

Der Mainframe sendet einen Szenenindex, keine Pixelströme und keine Rollenbefehle. Die gemeinsame Szenendefinition liegt in Mainframe und Nodes; jeder Node leitet aus Szenenindex und seiner gespeicherten Rolle die lokale Lichtausgabe ab.

| Nachricht | Richtung | Inhalt/Funktion |
| --- | --- | --- |
| `SceneSelect` | Mainframe → Broadcast | Index der gemeinsamen Szene |
| `ClockSync` | Mainframe → Broadcast | BPM, Beats pro Takt, Beat im Takt, Taktnummer |
| `MainframeHello` | Mainframe → Broadcast | einmalige Begrüßung beim Mainframe-Boot; löst den dreifachen Node-Blink und `MainframeWaiting` aus |
| `NodeText` | Mainframe → Broadcast | Node-Typ, Cancel-Flag, 48-Byte-Textfeld; vom V2-Mainframe derzeit nicht gesendet |
| `NodeVisualState` | Mainframe → Broadcast | neutrale Render-Vorgabe (RGB-Modus, Helligkeit, Farbe, Überlagerung, Flash-Verhalten); bei Änderung und alle drei Sekunden, siehe B.6 |

Headerprüfung: V2-Magic `0x44434632` (V1: `0x4443464C`), Version `13`, Nachrichtentyp; Empfang prüft außerdem `sizeof(...)`. Die Strukturen werden direkt als Speicherabbild gesendet. Es gibt keine explizite portable Serialisierung, keine Senderbindung auf dem Node, keine Verschlüsselung, kein Pairing und keine Anwendungs-ACKs für Szenen.

Nodes senden keine Status-, Heartbeat- oder Clock-Antworten. Der Mainframe weiß damit bewusst nicht, welche Nodes eine Szene angewendet haben. Eine erfolgreiche `esp_now_send()`-Rückgabe beweist weiterhin nicht, dass ein bestimmter Node die Nachricht empfangen hat.

Mainframe-Start und Senden:

1. Setup initialisiert Display, Auge, Start-Sound und ESP-NOW; die Show läuft noch nicht. Das Mikrofon startet erst nach dem ersten Klick und dem Ende des Start-Sounds.
2. Bei betriebsbereitem Funk sendet er genau einmal `MainframeHello`. Es ist nur die Begrüßung für bereits laufende Nodes, keine Berechtigung für Szene oder Clock. Es gibt keine Session-ID und kein wiederholtes Hello.
3. Kurzer Buttondruck startet mit 120 BPM und vier Beats pro Takt und fordert die Audioanalyse an. Weitere kurze Drücke wählen die nächste Szene.
4. Beim Start und jeder Szenenauswahl wird dieselbe `SceneSelect`-Nachricht einmal gesendet. Während der Show folgt derselbe Broadcast alle 30 Sekunden.
5. Clock-Sync erfolgt beim Show-Start, auf Beat 1 jedes Taktes und zusätzlich sofort bei jeder Tempoübernahme aus dem Tempo-Tracker.

Node-Zustände:

- `Demo`: lokale Szene ab Boot, eigene 120-BPM-Clock.
- `MainframeWaiting`: ausschließlich nach einem empfangenen Hello; `serviceClock()` führt bis zum ersten Clock-Sync keine Beats aus.
- `MainframeRunning`: nach einem gültigen Clock-Sync, auch ohne vorheriges Hello; zwischen Syncs läuft die lokale Clock weiter.

Es gibt aktuell keinen Timeout, der einen laufenden Node nach Funkverlust zurück in Demo schaltet. Er läuft mit dem letzten Tempo und Befehl weiter. Ein wartender Node bleibt dagegen bis zum nächsten Clock-Sync wartend. Das ist kein expliziter, zeitgesteuerter Node-Holdover-Zustand.

### A.3 Szenen, Renderer und konkrete Problemstellen

`kScenes` enthält jetzt zwei Szenen. Szene 1 ist der bisherige Zustand. Szene 2 heißt „dynamisch“ und wurde aus dem früheren gemeinsamen Szenenmodell (Commit `07f4003`) wiederhergestellt. Ihre RGB-Farben, Pegel und Taktmuster entsprechen der damaligen Definition. Der alte `Breathe`-Renderer existiert im aktuellen Modell nicht mehr; der Wash verwendet deshalb dessen heutiges beatgebundenes Gegenstück `BarWave`. Das frühere Flash-Modell mit 2er- und 3er-Bursts wird durch ein entsprechendes Variationsprofil aus Drive-, Double- und Quad-Motiven umgesetzt.

Szene 1:

- Flash: deterministisches Variationsprofil, Fenster acht Takte, Seed 17; Gewichte Drive/Heavy/Double/Quad/Riser = 30/14/24/20/12. Die fünf Motive sind keine fünf aktuell auswählbaren Szenen.
- Wash: blau/eisblau, `BarWave` über vier Takte.
- Pulse: blauer Beat-Puls.
- Accent: roter Herzschlag über vier Beats.
- Flicker: beatbezogener alternierender Runner.

RGB wird über zwei GPIOs (26/32) aus demselben 15-Pixel-Puffer ausgegeben, also gespiegelte Ausgänge. Zusätzlich erzeugt der Renderer zeitbasierte Aktivität, Schatten, Farbstich und Dunkelstellen. Diese `activity` ist **kein Audio-Messwert**: Sie stammt aus `millis()`, Hash-/Noise-Funktionen und langsamen Pendelverläufen. Diese Modulationen sind auch nicht an eine gemeinsame Mainframe-Zeit gebunden; unterschiedliche Bootzeiten können Unterschiede erzeugen.

Der übernommene Kommunikationsstand enthält folgende Vereinfachungen und verbleibende Grenzen; dies ist kein nachgewiesener Fix des gemeldeten Szenenproblems:

1. **Eine Nachricht pro Szene.** Fremde Rollenpakete können nichts mehr überschreiben, weil Rollenbefehle nicht mehr über Funk gesendet werden.
2. **Idempotente Auswahl.** Ein Node vergleicht den aus dem Szenenindex abgeleiteten Befehl mit seinem aktiven Befehl. Eine Wiederholung derselben Szene verändert die laufende Ausgabe nicht.
3. **Einzelner, periodischer Szenenbroadcast.** Der Mainframe sendet `SceneSelect` beim Start oder Wechsel einmal und wiederholt den aktuellen Stand alle 30 Sekunden.
4. **Hello ist nur die Begrüßung.** Ein Node ohne Hello läuft zunächst in seiner lokalen Demo, übernimmt aber die nächste Szene und Clock unmittelbar. Nach einem empfangenen Hello blinkt er dreimal und wartet auf die ersten Mainframe-Nachrichten.
5. **Keine globale Szene-Barriere.** Nodes übernehmen eine Szene beim Eintreffen und verwenden dabei ihre laufende Beatposition. Eine kurzzeitig alte Szene auf einem einzelnen Node ist zulässig; die Installation wartet nicht auf andere Nodes.
6. **Kein Node-Rückfunk.** Status- und Clock-Antworten sind entfernt. Der Mainframe bekommt keine Empfangsbestätigung, erzeugt dafür aber keinen gleichzeitigen Antwortverkehr von mehreren Nodes.
7. **Blockierungen vergrößern weiterhin Empfangs- und Clock-Latenzen.** Flash-Ausgabe verwendet `delay(flashMs)` sowie zusätzliche 8-ms-Pausen. Hello erzeugt drei blockierende Bestätigungsblitze über ungefähr 2,3 Sekunden; Rollenbestätigung blockiert ebenfalls.
8. **Clock-Ankunft ist keine präzise Beatzeit.** Details siehe A.5. Drift, Verarbeitungszeit und Sprünge können wie Szenenfehler aussehen.
9. **Text kann die Szene absichtlich verdrängen.** Morse ersetzt vorübergehend die reguläre Node-Ausgabe. Textpakete werden nach Node-Typ gefiltert.

Quellen: historischer V1-Controller (`sendSceneSelect`, `selectNextScene`, Loop), `apps/node/src/main.cpp` (`stageIncomingSceneSelect`, `processPendingRadio`, `processPendingSceneSelectMessage`, `enterMainframeWaitingMode`), Node-Renderer. Die tatsächliche Ursache eines konkreten Hardwarevorfalls bleibt ohne Symptom-/Logkorrelation offen; insbesondere ist kein bestimmter Paketverlustanteil nachgewiesen.

### A.4 BPM-Scanner: vollständiger Ablauf

**Einordnung (14.11.2026):** Dieser Abschnitt analysiert den V1-Scanner (`PdmMicrophone`, `audio_follow`). V2 hatte ihn als `beat_analyzer.*` und `audio_follower.*` auf die Voice Base portiert und bis zum 03.10.2026 produktiv genutzt. Seit Commit `30ac9a7` steuert er weder Clock noch Stimmung; der Code liegt unangebunden im Quellbaum und wird von seinen Hosttests abgedeckt. Der produktive Tempopfad steht in Abschnitt 0 („PCM-Tempo-Tracker“). Die folgenden Formeln bleiben als Analyse des Vorgängers und als Vergleichsgrundlage erhalten; ob der portierte Code Zeile für Zeile damit übereinstimmt, wurde bei diesem Abgleich nicht erneut geprüft.

Es gibt im V1-Scanner drei verschiedene Tempoangaben:

- `PdmMicrophone::detectedBpm()`: gewählter Tempovertreter aus der Onset-Analyse.
- `PdmMicrophone::clockBpm()`: daraus mit Halb-/Doppeltempo-Heuristik gewähltes Clock-Ziel.
- `currentBpm` im Mainframe: tatsächlich laufendes Showtempo nach `audio_follow`.

`updateClockFromAudio()` übergibt ausdrücklich `microphone.clockBpm()` an das Feld `audio_follow::Input::detectedBpm`. Der Feldname ist daher leicht missverständlich.

#### 1. Aufnahme und Vorverarbeitung

Quelle: `pdm_microphone.cpp`, `begin`, `update`, `accumulateSamples`.

- PDM RX auf `I2S_NUM_0`, DATA GPIO26, CLK GPIO32, 16 kHz, 16 Bit, Mono links, DMA 8 × 128, PDM-Downsampling `I2S_PDM_DSR_16S`.
- Je Loop-Aufruf bis zu vier nicht wartende `i2s_read()` mit je 256 Samples: maximal 1024 Samples bzw. 64 ms Audiodaten pro Aufruf. Das ist **kein festes 64-ms-Analyseintervall**.
- Pro Sample: `dc += (sample - dc) >> 6`; anschließend `centered = sample - dc`. Dies ist ein kontinuierlicher DC-Schätzer mit Hochpasswirkung, kein Bassbandfilter.
- `blockLevel` ist der Mittelwert von `abs(centered)` über alle im aktuellen Update gelesenen Samples, nicht RMS. Außerdem wird der größte Absolutwert erfasst.
- Eine weitere Sample-Hüllkurve mit Gewicht 1/16 wird mitgeführt, ist aber nicht die Grundlage des Tempo-Scores.
- Pro Update mit neuen Samples erfolgt genau eine Analyse, Zeitstempel `millis()` nach dem Lesen. Die Onset-Zeit ist somit Verarbeitungszeit, nicht die samplegenaue Position eines Transienten im DMA-Puffer.

#### 2. Energiehüllkurven und Music-Gate

Quelle: `updateAnalysis`. Bei Nullinitialisierung wird direkt der aktuelle Blockpegel übernommen; danach gelten ganzzahlige Filter:

| Größe | Steigender Eingang | Fallender Eingang |
| --- | --- | --- |
| schnelle Analysehüllkurve F | `(2F + block)/3` | `(5F + block)/6` |
| langsame Analysehüllkurve S | `(15S + block)/16` | `(31S + block)/32` |
| Analyse-Floor N | `(255N + S)/256` | `(7N + S)/8` |

`musicPresent` schaltet an bei `S > N + 22` und bleibt an, solange `S > N + 12`. Dies ist Pegelhysterese, keine Unterscheidung zwischen Sprache, Musik und Geräusch.

Transient `T = max(F-S, 0)`. Onsetschwelle `H = 18 + max(S-N, 0)/3`.

#### 3. Onsets und History

Ein Onset wird akzeptiert, wenn:

- Music-Gate aktiv ist;
- seit dem letzten Onset mindestens 300 ms vergangen sind (oder es noch keinen gibt);
- `T > H`;
- der Blockpeak höchstens das 18-Fache des Blockpegels beträgt (bei Blockpegel 0 wird diese Peakprüfung übersprungen).

Bei bestehendem Tempo und Confidence ≥ 50 werden zusätzlich zu frühe Intervalle verworfen: kleiner als 88 % des erwarteten Beatintervalls, außer sie liegen im Bereich 45–65 %. Die absolute 300-ms-Sperre gilt weiterhin und beschränkt diese Halbintervall-Ausnahme.

Akzeptierte Onsets erhalten den aktuellen Verarbeitungszeitstempel. Gespeichert werden die letzten acht Onset-Zeiten und die letzten acht gültigen Intervalle. Intervalle von 300–1200 ms lösen die Temposchätzung aus; sie beginnt erst mit mindestens drei Onset-Zeitstempeln. Bei einer Lücke über 1200 ms bleiben nur der neue Zeitstempel und Confidence 0; Intervallhistory wird gelöscht, Tempo-Buckets werden an dieser Stelle nicht gelöscht.

Nach mehr als 2200 ms ohne Onset wird Confidence auf 0 gesetzt. Nur wenn zugleich das Music-Gate aus ist, werden auch BPM, Clock-BPM, Histories und Tempo-Buckets zurückgesetzt. Bei fortbestehendem Pegel können alte BPM daher mit Confidence 0 stehen bleiben.

#### 4. Puls-History als zusätzliches Periodizitätssignal

Bei jedem Analyseupdate wird `min(255, max(T-H,0)/4)` in einen 64-Einträge-Ring geschrieben; ohne Music-Gate wird 0 geschrieben.

Das geschätzte Frameintervall startet mit 64 ms. Nur tatsächlich gemessene Updateabstände zwischen 20 und 120 ms fließen mit Gewicht 1/8 ein. Dennoch wird für **jedes** Update ein Pulswert abgelegt. Bei häufigeren Updates unter 20 ms stimmt deshalb die Zeitannahme der History nicht zwingend mit ihrer tatsächlichen Abtastung überein. Die 64 Einträge sind kein garantiertes Zeitfenster.

Für jedes Kandidatentempo wird ein auf ganze Frames gerundeter Ein-Beat-Lag bestimmt. Ab acht History-Einträgen und gültigem Lag (mindestens 2, kleiner als Historylänge) summiert der Code `min(pulse[i], pulse[i-lag])` für Paare ungleich null. Das ist eine einfache verzögerte Ähnlichkeit, keine FFT und keine klassische Produkt-Autokorrelation.

#### 5. Kandidatensuche und Scores

Quelle: `updateTempoEstimate`. Es werden alle **101 ganzzahligen BPM-Werte von 80 bis 180** bewertet. Nachfolgende Formeln beschreiben die tatsächlich verwendeten Gewichte; Divisionen sind ganzzahlig.

Für jedes Paar der höchstens acht Onsets:

1. Abstand `d` muss positiv und höchstens 2200 ms sein.
2. Nächste Beatanzahl `m = floor((d*bpm + 30000)/60000)`; zulässig 1–8.
3. Erwarteter Abstand `e = floor((60000*m + bpm/2)/bpm)`.
4. Toleranz `t = max(24 ms, floor(e*9/100))`.
5. Bei Fehler `a = abs(d-e) <= t`: Scorezuwachs `(1+newerIndex)*(9-m)*8 + t-a`.

Dieser Paarscore heißt hier I. Der zusätzliche Primärscore Q zählt bei `m=1` den doppelten Score, bei `m=2` ein Viertel und sonst nichts. Ein-Beat-Treffer werden separat gezählt.

Die bis zu acht direkten Onsetintervalle ergeben Score D: Vergleich mit `floor(60000/bpm)`, Toleranz `max(24 ms, 8 % dieses Intervalls)`, Zuwachs `(2+intervalIndex)*24 + 2*(t-a)`.

Mit Pulsscore P wird je BPM ein Gedächtnisscore M aktualisiert:

`M = M - (M >> 3) + I + Q + floor(P/2)`.

Dieser Zerfall geschieht je Temposchätzung, nicht je Sekunde. Ein kombinierter Kandidatenscore lautet:

`C = M + 2D + 6Q + 6P + 30 % M_des_linken_Nachbarn + 30 % M_des_rechten_Nachbarn`.

Nur Kandidaten mit aktuellem Paartreffer nehmen an der Auswahl teil. Bei Abstand ≤ 2 zum vorigen erkannten BPM kommen `(3-Abstand)*18*6` Punkte hinzu; sonst bei Confidence ≥ 60 und gleicher Tempofamilie `18*4`.

#### 6. Halb-/Doppeltempo-Familien und BPM-Auswahl

`canonicalTempoFamilyBpm()` betrachtet BPM, ganzzahlig BPM/2 und BPM×2 innerhalb 80–180. Bevorzugt wird zunächst die geringste Entfernung zum Fenster 90–170, dann die geringste Entfernung zu dessen bevorzugtem Zentrum 130.

Die kombinierten Scores werden unter diesem kanonischen BPM zusammengefasst. Die vorige Familie erhält beim Ranking 216 Bonuspunkte. Ab bisheriger Confidence 60 bleibt sie erhalten, wenn die neue Familienwertung weniger als 112 % ihrer vorhandenen Unterstützung erreicht.

Innerhalb der Siegerfamilie wird ein tatsächlicher Tempovertreter gewählt:

`R = 2I + 8Q + D + 8P + floor(M/4) + 30 % I_der_beiden_Nachbarn`.

Der kanonische Vertreter erhält 180 Zusatzpunkte. Andere Vertreter verlieren `abs(bpm-kanonisch)*18*5` Punkte; diese Strafe wird geviertelt, wenn ihr Primärscore oder ihre Primärtrefferzahl stärker als beim Familienanker ist. Bei gleicher Familie mit dem vorherigen BPM gibt es weitere Kontinuitätsboni: bis Abstand 2 `(3-Abstand)*18*8`, sonst bis Abstand 6 `(7-Abstand)*18*2`.

Wichtig: `inSameTempoFamily()` bedeutet nicht allgemein „nahe BPM“. Es prüft gleiche kanonische Werte oder annähernde Verdoppelung der kanonischen Werte mit maximal 4 BPM Fehler. Einige vermeintliche Glättungen wirken deshalb nur bei dieser engen Bedingung.

#### 7. Confidence und zwei Analyse-BPM-Ausgänge

Für den gewählten Vertreter:

- Präzision = `max(0, 100 - floor(mittlerer_Paarfehler*320/Beatintervall))`.
- Abdeckung = `min(100, floor(Paartreffer*100/8))`.
- Primärabdeckung = `min(100, floor(Ein-Beat-Paartreffer*100/4))`.
- Trennung = relativer Abstand der besten zur zweitbesten Familienwertung in Prozent; 0, wenn kein positiver Abstand besteht.
- Kontinuität = initial 60, bei unveränderter Familie 100, sonst `max(0,100-6*Familienabstand)`.

`confidence = floor((2*Präzision + Abdeckung + Primärabdeckung + Trennung + Kontinuität)/6)`.

Das ist ein heuristischer Score 0–100, keine kalibrierte Wahrscheinlichkeit. Bei fehlendem gültigem Sieger sinkt der bisherige Wert um 6. Die deklarierte Konstante `kAnalysisTempoMinimumMatches=3` wird nicht verwendet: Tatsächlich erforderlich sind zunächst drei Zeitstempel; die spätere Auswahl prüft nicht ausdrücklich drei passende Paare. Auch mehrere weitere deklarierte Familien-/Doppelintervall-Konstanten sind derzeit unbenutzt.

`detectedBpm` übernimmt den gewählten Vertreter, mit zusätzlicher Familienhysterese: Bei mindestens 6 BPM Unterschied und bestehender Unterstützung muss der neue Vertreter mindestens 125 % der alten Unterstützung (`6Q+D+P`) erreichen. Bei gleichem Familienbezug und Abstand ≤ 2 wird gemittelt.

`clockBpm` kann stattdessen den schnelleren kanonischen Familienwert wählen, wenn dort Primär-/Intervall-/Pulsunterstützung vorhanden ist: mindestens ein Primärtreffer oder Evidenz `6Q+D+6P` von mindestens 35 % des besten Vertreter-Scores. Der Wechsel zu einer langsameren Unterteilung innerhalb derselben Familie wird verzögert. Trotz Konstante „4 Hits“ hält der aktuelle Kontrollfluss beim ersten Kandidaten und beim Hochzählen bis 4 das alte Tempo fest; erst beim **fünften** passenden Durchlauf wird der langsamere Kandidat übernommen. Kleine Änderungen werden wiederum nur bei gleichem Familienbezug gemittelt.

#### 8. Audio-Follow: Übernahme in die Show-Clock

Quelle: `audio_follow.cpp`; läuft nur bei `mainframeLive`.

- Zustände `Searching`, `Locked`, `Holdover`.
- Ein frischer Onset benötigt Music-Gate, nichtnull Clock-Ziel-BPM und einen bislang ungesehenen Onset-Zeitstempel.
- Zum ersten Lock: drei frische Kandidaten mit Confidence ≥ 68, BPM innerhalb ±4 zum laufend gemittelten Kandidaten.
- Beim Lock wird das Tempo direkt übernommen, `nextBeatAtMs` auf den Onset-Zeitstempel gesetzt und Clock-Sync angefordert. Der Taktzähler wird dabei nicht musikalisch neu bestimmt.
- Im Lock wird Confidence ≥ 62 verlangt. Onsetintervalle müssen normalerweise 88–112 % oder 176–224 % des laufenden Beatintervalls betragen. Beim Wiederaufnehmen aus Holdover wird diese Intervallprüfung einmal übersprungen.
- Zwei akzeptierte Updates mit exakt gleichem abweichendem Zieltempo erlauben eine Änderung um höchstens **1 BPM**; danach beginnt die Zählung erneut.
- Phasenfehler = Abstand des Onsets zum näheren der vorigen/nächsten geplanten Beatzeit. Kleine Fehler verschieben den nächsten Beat um Fehler/3.
- Große Fehler: Schwelle `max(20 ms, min(90 ms, Beatintervall/5))`. Zunächst Fehler/2 korrigieren; nach zwei großen Fehlern mit gleichem Vorzeichen die ganze Abweichung korrigieren und Sync anfordern.
- Nach ungefähr vier Beatintervallen ohne akzeptierten Onset wird unter den jeweiligen Prüfbedingungen Holdover betreten, spätestens nach mehr als 4 Sekunden wird der Follow-Zustand zurückgesetzt. Die laufende Master-Clock wird dabei **nicht** auf 120 BPM zurückgesetzt und nicht angehalten.

#### 9. Master-Clock und musikalische Aussagegrenze

Quelle: historischer V1-Controller, `setClockBpm`, `updateClockFromAudio`, `onBeat`, Loop.

Start 120 BPM, erlaubte Clock-Grenzen 60–180, Intervall `floor(60000/BPM)` Millisekunden. Im Loop werden fällige Beats mit einer `while`-Schleife nachgeholt. Vier Beats ergeben einen Takt. Die tatsächlich gesendeten Syncs werden erst in `onBeat()` erzeugt.

Der Scanner erkennt **keine musikalische Eins**, keine Taktart und keine Songphrase. „Beat 1“ ist der Zählerstand der Installation. Onsets können durch rhythmische Akzente oder Geräusche entstehen; sie sind keine sicher erkannten Bassdrums. Es gibt keine Bass/Mid/High-Trennung, kein Genre-, Break- oder Drop-Modell. Bei langen Loop-Pausen können mehrere nachgeholte Beats nahezu gleichzeitig verarbeitet werden.

### A.5 Node-Clock ist derzeit kein Soft-Sync

`apps/node/src/main.cpp::applyClockSync()`:

1. Ein gültiger Clock-Sync mit BPM ungleich null wird auch im Demo-Modus und ohne vorheriges Hello übernommen.
2. BPM und Taktlänge werden übernommen, der Zustand wird `MainframeRunning`.
3. Wenn derselbe Beat/Takt in den letzten 250 ms bereits gerendert wurde, wird kein weiterer Beat ausgegeben, aber `nextBeatAtMs = now + beatIntervalMs` gesetzt.
4. Sonst werden Beat/Takt übernommen, sofort `onBeat()` ausgeführt und danach ebenfalls `nextBeatAtMs = now + beatIntervalMs` gesetzt.

`now` ist die Verarbeitungszeit im Node-Hauptloop; das Paket enthält keinen Senderzeitstempel und der Callback speichert keinen Empfangszeitstempel. Bei blockierender Flash-Ausgabe kann der Zeitpunkt nach der Ausgabe bereits zurückliegen. Es gibt keinen PLL-Regler und keine Funk-Telemetrie zum Mainframe. Die README-Aussage über sanften Node-Phasentrim entspricht diesem Code nicht.

Zusätzlich fehlen Plausibilitätsgrenzen für empfangene Clock-BPM: `bpmToIntervalMs(message.bpm)` dividiert direkt. Das ist ein Protokollrobustheitsbedarf, nicht der Nachweis, dass der vorhandene Mainframe ungültige BPM sendet.

### A.6 VU-Meter und Matrix-Rendering

**Einordnung:** V1-Stand der 5×5-Matrix. Die V2-Umsetzung als Iris-VU und Textpanel steht in Abschnitt 0 und B.6. Der letzte Absatz zum Node-Text gilt weiterhin für den V2-Node.

Die VU-Pegelberechnung ist von der Tempoanalyse getrennt (`updateMeterLevel`): schnelle Hüllkurve mit Attack 1/4 und Release 1/8, langsame mit 1/8 und 1/16. Adaptiver Noise-Floor: nach unten 1/8, nach oben 1/128. Signal-Ceiling folgt nach oben mit 1/4 und nach unten mit 1/16.

Gate = Floor + 6 + 10; Darstellungsbeginn weitere 18 darüber. Grundpegel wird auf maximal 20 Einheiten skaliert, mit mindestens 180 Pegelspanne und sonst zusätzlichem Headroom 140. Einheiten ≤1 werden unterdrückt. Transientenanteil über 18 liefert bis zu fünf zusätzliche Einheiten über eine Spanne von 140. Gesamtmaximum 25, Release zwei Einheiten je Analyseupdate; nach vier ruhigen Updates wird auf 0 gesetzt. Die berechnete Variable `meterDisplayLevel_` wird für diese endgültige Skalierung nicht verwendet.

`matrix_meter.cpp` zeichnet höchstens alle 40 ms, also nominell maximal 25 Aktualisierungen/s. Von 25 LEDs bleiben 23 für den Meter; Pixel 0 ist WLAN-Status und Pixel 4 Beatpunkt. 24/25 gemessene VU-Einheiten werden auf 23 sichtbare Meterpixel gekappt.

Der Meter ist keine klassische Balkenanzeige: Eine zufällige Pixelreihenfolge entscheidet über die aktiven Flächen und verändert sich alle 220 ms. Pro Zeichnen steigen lokale Pixelwerte um 18 oder fallen um 14. Ihre Werte laufen durch Farbverläufe:

- normal: Schwarz → Blau `(0,105,255)` → Pink `(255,40,150)` → Violett `(235,0,255)`;
- Aufnahme/AI-Verarbeitung: Schwarz → Grün → Hellgrün → sehr helles Grün.

Diese Pegel-, Aktivierungs-, Übergangs- und Farbidee ist auf Iris-Polygone übertragbar; `M5.dis.drawpix()` und der 5×5-Framebuffer selbst sind hardwaregebunden.

UI-Vorrang im Grundrenderpfad: Text, Szenenanzeige/temporäre Sperre, AI-Animation, danach VU. Text blockiert Beat-/WLAN-Overlay. Beatpunkt leuchtet 140 ms: **gelb auf Beat 1**, weiß auf anderen Beats, rot als Audio-Sync-Korrekturmarkierung. Das unterscheidet sich von der README-Beschreibung „rot auf Beat 1“.

Text: 96-Byte-Puffer, normalisierte Großbuchstaben/Umlaute, 5×5-Glyphen, Introblitz 90 ms, Pause 1000 ms, dann alle 130 ms eine Scrollspalte. Es gibt noch kein Auge und keinen Text-Overlay-Compositor.

Node-Text: höchstens 47 Nutzbytes, A–Z/0–9 als Morse-ähnliche Lichtfolge, Start auf nächstem lokalen Beat. Einheit ist ein halber Beat, Punkt 1 Einheit, Strich 2, Symbolpause 1, Buchstabenpause 2, Wortpause 4. Das weicht bewusst vom klassischen Morse-Zeitverhältnis ab und ist eine eigene beatgebundene Scheduling-Funktion. Matrixtext und Node-Morse enden nicht notwendigerweise gleichzeitig.

### A.7 ESP-NOW, WLAN und Cloud

**Einordnung:** Der erste Absatz gilt auch für V2. WLAN-Manager, Cloud-Task und Aufnahme-/AI-Pfad beschreiben den V1-Controller; der V2-Mainframe enthält davon derzeit nichts. Der Worker liegt weiterhin als Quellcode unter `workers/decaflash`.

ESP-NOW nutzt Wi-Fi STA, festen Offline-Kanal 1 und einen unverschlüsselten Broadcast-Peer mit `peer.channel=0` (= aktueller lokaler Funkkanal). Die Nodes verbinden sich nicht mit einem Access Point.

Der WLAN-Manager scannt synchron, probiert konfigurierte SSIDs in Listenpriorität und wartet pro Verbindungsversuch bis zu 15 Sekunden, in 250-ms-Schritten. Er filtert nicht auf Kanal 1. Manuelle `wifi scan`/`wifi connect` laufen aus der Shell im Mainframe-Hauptloop und können Audioabholung, Clock und UI blockieren.

Cloud-Arbeit läuft in einer separaten Task auf Core 0, Priorität 1, Stackargument 12288. Es gibt einen Jobslot. Jede eingeplante Cloud-Anfrage aktiviert `managedWifiSessionActive`; `radioPauseActive()` verhindert währenddessen Szenen-, Clock- und Node-Text-Sends. Dies gilt **auch bei einem AP auf Kanal 1**. Nach Abschluss wird WLAN getrennt und ESP-NOW samt Broadcast-Peer wiederhergestellt; der Recovery-Versuch ist auf ein Intervall von einer Sekunde begrenzt.

Bei manuell verbundenem WLAN außerhalb Kanal 1 blockiert `serviceEspNowState()` ESP-NOW ausdrücklich. Mehr RAM oder ein S3R-Port beseitigen das Kanalproblem nicht. Bereits laufende Nodes spielen autonom weiter, bekommen während der Pause aber keine neuen Szenen oder Clock-Korrekturen.

Aktueller Aufnahme-/AI-Pfad:

- Langer Mainframe-Buttondruck ab 1200 ms toggelt AI-Listening.
- Nach 1500 ms kontinuierlichem Music-Gate wird eine Aufnahme vorbereitet. Trigger bevorzugt BPM vorhanden, Confidence ≥18 und Onset höchstens 180 ms alt; nach weiteren 1200 ms greift ein Fallback ohne diese Beatbedingungen.
- Standard und Maximum sind bereits **12 Sekunden** bei 16 kHz. Aufnahme ist µ-law, ein Byte pro Sample, Zielgröße 192000 Bytes. Bei RAM-Mangel wird in 250-ms-Schritten bis mindestens eine Sekunde gekürzt. Keine explizite PSRAM-Allokation.
- Onset-/VU-Analyse läuft während der Aufnahme weiter. Der ganze Clip liegt vor Upload im RAM; Multipart-Streaming vermeidet einen zweiten vollständigen Upload-Puffer, streamt aber nicht direkt vom laufenden Mikrofon ins Netz.
- Mainframe sendet µ-law an `/api/audd`; Worker dekodiert zu 16-Bit-WAV und fragt AudD. Bei Treffer erzeugt er aus Titel/Artist zusätzlich einen kurzen deutschen Text. Kein BPM-Rückkanal aus AudD.
- Erfolgreiche Verarbeitung: Cooldown 420 Sekunden; Fehlschlag 60 Sekunden; WLAN-Fehlschlag deaktiviert den AI-Modus.
- IMA-ADPCM-Code ist vorhanden und serverseitig unterstützt, aber im aktiven Mainframe-Aufnahmepfad wird µ-law verwendet.
- Worker bietet außerdem `/api/chattie`, Health und geschützte Debug-WAV/JSON-Endpunkte. Letzte dekodierte Aufnahme und Metadaten werden in zwei KV-Schlüsseln überschrieben.
- HTTPS-Client verwendet aktuell `setInsecure()`; Zertifikatsprüfung ist also nicht eingerichtet. Lokale Secrets sind gitignoriert, Vorlagen versioniert. Normaler Worker-Deploy ist Git/Cloudflare-Integration.

Damit sind die Hinweise auf ausschließlich kurze Clips und noch fehlende 8–12-s-Unterstützung überholt. Nicht erledigt ist eine belastbare Qualitätsentscheidung, ob ein Clip für Recognition geeignet ist; der Fallback macht das Gate weiterhin grob.

### A.8 Abweichungen vorhandener Dokumentation

Vor der Konsolidierung beschrieb die kopierte README teilweise historische Details: nur Flashlight-Nodes, fünf lokale Programme, alte Rohpegel-Serialreports und frühere UI-Farben. Der übernommene Code unterstützt Flashlight und RGB-Rollen sowie zwei gemeinsam einkompilierte Szenen. Die README-Angaben zu Szenenindex, einmaligem Hello, 30-Sekunden-Wiederholung und fehlenden Sessions/ACKs passen zum übernommenen Stand. Frühere Aussagen dieses Kontextdokuments über getrennte Rollenpakete, nur eine Szene oder eine Hello-Pflicht für Clock-Sync sind überholt. Der Node-Resync erfolgt direkt wie in A.5 beschrieben.

`docs/v1-outline.md` ist historisch: Aussagen „kein Funk/Mikrofon/RGB“ sind kein heutiger IST-Zustand. Die Node-Bootmeldung „Node V2“ bedeutet nicht, dass die hier geplante Mainframe-v2 bereits existiert. Die V1-Outline ist inzwischen ausdrücklich als historisch markiert; die README wurde auf einen kurzen V2-Einstieg konsolidiert.

## B. Geplante Mainframe v2 – Nutzeranforderungen, noch nicht implementiert

**Umsetzungsstand (14.11.2026):** Die Überschrift stammt aus der Planungsphase. B.1–B.4 bleiben die Nutzeranforderungen; unter jedem Abschnitt steht, was davon im Code umgesetzt ist. B.5 stellt die geplante Reihenfolge dem tatsächlichen Verlauf gegenüber. B.6 beschreibt umgesetzten Code.

### B.1 Grundprinzip und Hardware

Die eigentliche Lichtshow muss vollständig ohne Access Point, Internet und KI-Dienste funktionieren. WLAN/Internet liefern ausschließlich Enhanced Features. Kein zusätzlicher ESP/Controller im Mainframe; die bestehenden Licht-Nodes bleiben Teil der Installation. Display hat geringere Priorität als Audio und Node-Kommunikation.

Zielhardware:

- M5Stack AtomS3R, ESP32-S3-PICO-1-N8R8, 8 MB Flash, 8 MB PSRAM.
- 128×128-Farbdisplay, BMI270-IMU, programmierbarer Frontbutton.
- Atomic Voice Base mit MEMS-Mikrofon und Speaker.
- Später: externer M5Stack U185 mit STHS34PF80 zur Präsenzdetektion; noch nicht vorhanden und vorerst nicht benötigt.
- Keine Kamera.

**Stand:** AtomS3R und Voice Base sind in Betrieb; U185/TMOS fehlt weiterhin. Die Show läuft ohne WLAN, weil der V2-Mainframe noch kein WLAN enthält.

### B.2 Display und Interaktion

Normalansicht: animiertes Low-Poly-Auge im PS1-Stil auf Schwarz, wenige Flat-Shaded-Polygone, bewusst niedrige/ruckelige Framerate erlaubt. Idle-Verhalten enthält Blinzeln und Blickbewegungen. Iris ist das VU-Meter und übernimmt die bestehende Pegel-/Farblogik als visuelles Prinzip.

Persönlichkeit äußert sich zunächst ausschließlich über Text. Text erscheint bei Reaktionen; Overlay über weiterlaufendem Auge ist eine mögliche Darstellung. Langer Text darf das Auge temporär ersetzen. Daraus folgt noch keine Entscheidung über Grafikbibliothek, Polygonanzahl oder konkrete FPS.

Inputs: Mikro/Musik, IMU, Frontbutton, TMOS-Präsenz; optional Internet. Die spätere IMU-Auswertung soll Bass/Vibration, leichtes Tippen, Doppeltippen, Stoß/Schlag und Schütteln/Bewegen unterscheiden. Ein TinyML-Modell ist dafür eine Option, keine festgelegte Voraussetzung.

**Stand:** Auge, Iris-VU, Blinzeln/Blickbewegung, Lider und Textpanel über dem weiterlaufenden Auge sind umgesetzt (Abschnitt 0, B.6). Mikrofon, IMU und Frontbutton sind als Eingänge angebunden; TMOS und Internet nicht. Die IMU unterscheidet Tap, MultiTap, Impact, Shake, Rotate, Tilt und Move heuristisch, ohne TinyML. Bass/Vibration wird nicht als eigene IMU-Klasse erkannt. Die Trennschärfe am Gehäuse ist nicht abgenommen.

### B.3 Personality

Die Persönlichkeit wird durch lokale Zustände und Regeln gesteuert, **nicht durch ein Sprachmodell**. Vorgesehene Zustände ungefähr 0–100:

- `energy`
- `annoyance`
- `attention`
- `loneliness`
- `depression` als bewusst humorvoller Grufti-Party-State

Events verändern Werte; ein Teil der Werte baut sich über Zeit ab. Wiederholtes Tippen erhöht beispielsweise annoyance, fehlende Präsenz loneliness, Interaktion attention; annoyance sinkt langsam wieder.

Zunächst `state + event → lokales Textpaket`. Später optional `state + event → kleines lokales Sprachmodell → kurzer Text`. Das Modell wäre ausschließlich der „Mund“; Zustandsentwicklung und Lichtentscheidungen bleiben lokal determinierbar.

States dürfen später Licht beeinflussen, z. B. sehr hohe annoyance → temporär rot. Exakte Grenzen, Dauern, Prioritäten und Rückkehr zur laufenden Szene sind noch nicht spezifiziert. Ein weißer Flashlight-Node kann physisch kein Rot ausgeben; diese konkrete Farbwirkung betrifft RGB-Nodes. Für Flash sind passende Helligkeits-/Rhythmusreaktionen gesondert zu entscheiden.

**Stand:** Die fünf Zustände, ihre Ereignis- und Zeitregeln, lokale Textvarianten je Bewegungsart und Grufti-Verse sind umgesetzt (B.6). Zusätzlich zum Text gibt es Creature-Sounds vor dem Analysestart (Abschnitt 0). Als Lichtwirkung existieren zwei Regeln: annoyance ≥ 90 sendet Vollfarbe Rot und Flash dauerhaft an, loneliness ≥ 90 dimmt auf 15 % und schaltet Flash aus. Kein Sprachmodell.

### B.4 AudioAnalyzer v2

Zuerst bestehenden Scanner verstehen, messen und stabil übernehmen. Mögliche spätere Ausgänge: BPM, Beat, Beat confidence, Bass/Mid/High-Energie, Gesamtenergie, rhythmische Regelmäßigkeit, Break/Drop/Transition und eventuell grobe Musikklassen. Diese Erweiterungen sind nicht bereits implementiert und nicht alle verbindlich beschlossen. TinyML-Audioklassifikation bleibt eine spätere Option.

**Stand:** Der bestehende Scanner wurde zunächst portiert, dann am 03.10.2026 als Clock-Quelle durch `V2TempoTracker` ersetzt (Abschnitt 0); dessen Hardwareabnahme steht aus. Vorhanden sind BPM, Confidence, letzter Onset, Stille-Erkennung und ein geglätteter Bassanteil. Nicht vorhanden sind Bass/Mid/High-Ausgänge für die Lichtsteuerung, Gesamtenergie als eigener Ausgang, Regelmäßigkeitsmaß, Break/Drop/Transition und Musikklassen.

### B.5 Verbindliche Reihenfolge der vorgesehenen Arbeit

Die linke Spalte ist der Plan vom 22.–25.09.2026, unverändert. Die rechte Spalte ist der aus Code, Git-Historie und diesem Dokument ablesbare Stand am 14.11.2026.

| Nr. | Geplanter Schritt | Tatsächlicher Stand |
| ---: | --- | --- |
| 1 | Eigenständigen Projektstand, Git-Ziel und tatsächliche Hardware prüfen; Repo-Struktur abstimmen und minimalen Hardwaretest vorbereiten. | Erledigt. Visuelle Display- und Buttonprüfung des Bring-up-Tests sind nicht formal protokolliert. |
| 2 | Mainframe-/Node-Kommunikation mit verpflichtender V2-Funktrennung stabilisieren. | Teilweise. Funkkennung im Code, Hosttests bestehen, Mainframe geflasht. Ein angeschlossener Node hat laut A.1 Mainframe-Szenen dargestellt. Gegenseitige V1/V2-Ablehnung auf Hardware und ein protokollierter End-to-End-Test stehen aus. |
| 3 | Szenenproblem reproduzieren und anhand von Messungen eingrenzen; keine Ursache vorwegnehmen. | Offen. Keine Reproduktion oder Messung dokumentiert. |
| 4 | Dafür benötigten AtomS3R-Port gezielt aufbauen. | Erledigt; deutlich über das für Schritt 3 Nötige hinaus. |
| 5 | Audio/BPM stabil übernehmen. | In Arbeit, abweichend vom Plan. Der Scanner wurde portiert und danach als Clock-Quelle durch einen neuen PCM-Tracker ersetzt. Hardwareabnahme des neuen Pfads offen (Abschnitt 0). |
| 6 | Low-Poly-Auge mit VU. | Umgesetzt und geflasht; Pegelkalibrierung und Messung unter Last offen. |
| 7 | Personality-State-System und einfache lokale Textpakete. | Umgesetzt (B.6); Kalibrierwerte vorläufig. |
| 8 | IMU/Button/TMOS. | IMU und Button umgesetzt; Gestenabnahme am Gehäuse offen. TMOS nicht vorhanden. |
| 9 | Personality mit Lichtshow verbinden. | Erste Regel umgesetzt (`NodeVisualState`, B.6); Test mit geflashtem V2-Node offen. |
| 10 | TinyML. | Nicht begonnen. |
| 11 | Experimentelles lokales Sprachmodell ganz zuletzt. | Nicht begonnen. |

Abweichung vom Plan: Die Schritte 6 bis 9 wurden umgesetzt, bevor 2, 3 und 5 abgeschlossen waren. Zusätzlich entstanden nicht eingeplante Teile: Creature-Sounds mit Akku- und Schlafzyklus, das Textpanel mit Emoji-Atlas und die Diagnose-Environments.

**Offene Frage an den Nutzer:** Gilt diese Reihenfolge weiter als verbindlich, und welcher Schritt ist der nächste – Abschluss von 2 und 3 (End-to-End-Test, Szenenproblem), Hardwareabnahme von 5 oder etwas anderes? Dieses Dokument legt die Reihenfolge nicht neu fest.

### B.6 Lokale Stimmungsparameter und Debug-Overlay – Entscheidung vom 26.09.2026

Der Nutzer hat den ersten Schritt konkretisiert: **keine Reaktionstexte**, zunächst nur fünf lokale Parameter; spätere Wirkung auf Nodes ist ein Folgeschritt. Der fehlende Präsenzsensor bleibt ausgeklammert. Langes Drücken des Frontbuttons zeigt die fünf Parameter als Debug-Overlay, erneutes langes Drücken blendet es aus. Dieser Abschnitt ersetzt den vorherigen Entwurf einschließlich Textauswahl. Nachfolgende Nutzerpräzisierung: loneliness steigt ständig und sinkt durch Beschäftigung; Tippen/Mehrfachtippen, Stoß, Drehen/Schiefhalten und Schütteln müssen als getrennte Ereignisse verfügbar sein. Der fehlende Präsenzsensor verhindert diese ausdrücklich gewünschte Interaktionsregel nicht.

Implementiert in `personality.{h,cpp}` und `motion_events.{h,cpp}`, angebunden in `main.cpp`: hardwareunabhängiges, deterministisches Modell mit Werten 0–100, intern Millipunkte. Keine Persistenz, keine dynamische Allokation, keine eigene Task und kein Sprachmodell. `depression` bleibt der humorvolle Grufti-Party-State des Geräts.

| Parameter | Neustart | Vorläufige lokale Regel |
| --- | --- | --- |
| energy | 30 | BPM ist der Hauptfaktor: Energy verwendet die aktuelle Schätzung des `V2TempoTracker` (nicht das Showtempo), sobald Audio frisch und nicht still ist und ein Wert zwischen 60 und 200 BPM vorliegt. Die Basis liegt bei etwa 13/26/40/60/90 für 80/100/120/140/160 BPM. Ein aktueller, sicherer Beat addiert abhängig von der Confidence nur 0–10. Nach niedrigerem Tempo fällt Energy mit 20 Punkten/s, beim Anstieg mit 4 Punkten/s. Stille senkt den Wert nach 1 s mit 20 Punkten/s bis 0; unbekannte Audioeingänge frieren den Wert ein. |
| annoyance | 0 | Impact, Shake und MultiTap ab drei Taps erhöhen den Wert. Der Gesten-Klassifikator meldet insgesamt höchstens ein Ereignis pro Sekunde; Abbau 0,1/s Richtung 0 (ein Punkt je zehn Sekunden). |
| attention | 0 | Ereignisimpulse siehe unten; Rückkehr mit 3/s Richtung 0. |
| loneliness | 50 | Ständig +0,1/s bis 100; Beschäftigung baut den Wert ab, siehe Ereignistabelle. Ein Interaktionszustand, keine gemessene Abwesenheit. |
| depression | 0 | Steigt bei loneliness über 80 mit bis zu 0,2 Punkten/s und sinkt bei attention über 80 mit exakt derselben Rate. Frische, nicht stille Musik senkt mit 0,05 Punkten/s; 80–100 BPM sind neutral, der Tempoanteil steigt bis 120 BPM auf 0,1 Punkte/s und fällt bis 160 BPM auf 0. Wenig Bass addiert bis zu 0,12 Punkte/s dagegen. Ohne Einfluss bleibt der Wert stehen. Über 50 zeigt das Panel, sofern es frei ist, in jedem Betriebszustand alle zwei Minuten einen lokalen Grufti-Vers mit Grab-, Sarg- oder Urnen-Emoji. |

Alle Werte sind auf 0–100 begrenzt. Die numerischen Zuordnungen sind vorläufige Kalibrierwerte, keine vom Nutzer einzeln bestätigten Charaktereigenschaften. Der State-Service läuft etwa alle 100 ms und zusätzlich vor einem Bewegungsereignis. Lücken über 1 s werden auf 1 s Integration begrenzt und setzen die Audiobestätigung zurück; keine lange nachgeholte Stimmungsentwicklung nach einer blockierten Loop. Zeitdifferenzen funktionieren über `millis()`-Überlauf hinweg.

**Audio – präzisierte Regel:** Lautstärke treibt energy nicht hoch. Seit Commit `30ac9a7` kommen BPM, Confidence und letzter Onset direkt aus `voiceBaseInput.v2Tempo()`; die frühere Unterscheidung zwischen BeatAnalyzer-Wert vor dem Lock und `currentBpm` danach gibt es nicht mehr. Energy folgt damit dem ungeglätteten Trackerwert jeder 250-ms-Auswertung, auf 80–160 BPM begrenzt, und kann vom angezeigten Showtempo abweichen. Die Basis liegt rund bei 13 bis 90 zwischen 80 und 160 BPM: 120 BPM liegen bei 40, 140 bei 60, 150 bei 75 und 160 bei 90. Ein aktueller, sicherer Beat addiert abhängig von der Confidence nur 0 bis 10. Damit erreicht ein 160-BPM-Track mit maximal klaren Beats 100. Der Beat-Bonus und das für Depression verwendete Tempovertrauen (drei nahe, frische Schätzungen, danach 15 s gültig) verlangen Confidence ≥ 68; dieser Wert stammt aus der BeatAnalyzer-Skala und ist für den neuen Tracker nicht nachkalibriert (Abschnitt 0, offene Prüfungen). Halb-/Doppeltempo bleibt eine Grenze des Schätzers.

Stille wird unabhängig vom adaptiven Music-Gate am RMS der hochpassgefilterten PCM-Blöcke geprüft: Eintritt unter 80, Austritt ab 120 (16-Bit-Sampleeinheiten). Nach einer Sekunde kontinuierlicher Stille sinkt energy mit 20 Punkten/s bis 0; Rückkehr eines Signals beendet diesen Abfall unmittelbar. Capture nicht bereit bzw. über 250 ms ohne verarbeiteten Block bedeutet Unknown und friert die Audio-Energiewirkung ein; es gilt niemals als Stille. Diese absoluten Schwellen sind Hardware-Kalibrierwerte, keine bereits bestätigten Raumgeräuschgrenzen.

`audio_mood_features.{h,cpp}` analysiert die vorhandenen rohen 16-kHz-PCM-Blöcke zusätzlich mit einem einfachen Hochpass nahe 40 Hz und zwei Tiefpassstufen nahe 180 Hz. Verhältnis von Bassbandleistung zu Gesamtleistung hinter dem Hochpass, ohne FFT, zusätzliche Task oder Audiopuffer. Es ist ein breit auslaufender Bandindikator, keine exakte spektrale Integration von 40–180 Hz. Leistungen werden mit etwa 15 s Zeitkonstante geglättet; der Basswert wird erst nach 15 s brauchbaren Samples freigegeben. Stille, >1 % geclippte Samples pro Block oder Capturelücken verwerfen die Basshistorie. Vorhandene DC-/VU-/BeatAnalyzer-Pfade und Bibliotheksversionen bleiben erhalten.

Depression startet bei 0 und ändert sich nur durch aktive Einflüsse: Jeder Punkt loneliness über 80 erhöht die Rate linear bis zu 0,2 Punkten/s bei 100; jeder Punkt attention über 80 senkt sie um exakt dieselbe Rate. Diese sozialen Beiträge gelten auch ohne Analyse. Frische, nicht stille Musik addiert dauerhaft −0,05 Punkte/s. Der BPM-Bereich 80–100 ist wegen möglicher Half-Time-Erkennung neutral. Darüber steigt der Tempoanteil linear auf +0,1 Punkte/s bei 120 BPM und fällt bis 160 BPM wieder auf 0. Wenig gültiger Bass addiert linear bis zu 0,12 Punkten/s bei ≤8 % Leistungsanteil; bei ≥25 % addiert er 0. Die positiven Beiträge können den Musikabfall überwiegen. Ohne Einfluss bleibt der Wert unverändert; das Ergebnis bleibt zwischen 0 und 100. Unbekannte Audioeingänge und Stille liefern keinen Bassmangel- oder Langsamkeitsbeitrag. Audio verändert loneliness nie. Die Bassgewichte und Schwellen müssen mit der Voice Base und realem Musikmaterial geprüft werden.

**Bewegung/Ausrichtung:** Interne IMU aktiviert; maximal alle 20 ms frische Beschleunigungs- und Gyroskopdaten. Der eigenständige hardwareunabhängige Klassifikator verarbeitet 3D-Vektoren statt nur deren Beträge. Die erste ruhige Messung (0,85–1,15 g, <15 Grad/s) definiert die Referenzlage. Ein Tiefpass mit 250-ms-Zeitkonstante schätzt die Schwerkraftrichtung; die Differenz zum aktuellen Sample liefert dynamische Beschleunigung. Die folgenden Grenzen sind ausdrücklich **vorläufige Heuristiken**, keine auf dem Gehäuse validierte Gestenerkennung.

- Beobachtungsfenster 700 ms ab dynamischer Beschleunigung ≥0,30 g oder Drehgeschwindigkeit ≥45 Grad/s. Pro Fenster höchstens ein Ereignis, Priorität Shake → Impact → Rotate → MultiTap → Tap → Move. Zusätzlich gilt nach jedem gemeldeten Ereignis ein globaler Event-Cooldown von einer Sekunde. Dadurch wird ein Schütteln nicht gleichzeitig als mehrere Stöße verrechnet und erzeugt keine neue Debug-Ausgabe innerhalb der Sekunde. Gesten an Fenstergrenzen können getrennt werden; Erkennungslatenz bis ungefähr 700 ms.
- **Tap / MultiTap:** kurzer Impuls ab 0,45 g, Ende unter 0,20 g, Dauer maximal 120 ms; Beginn aufeinanderfolgender Impulse mindestens 100 ms auseinander. Zwei oder mehr abgeschlossene Impulse im Fenster ergeben MultiTap mit Zähler. Ein einzelner Impuls gilt nur bei insgesamt höchstens 120 ms Bewegung im Fenster als Tap.
- **Impact:** dynamischer Spitzenwert ≥1,8 g, sofern keine Schüttelsequenz erkannt wurde.
- **Shake:** mindestens drei Richtungswechsel kräftiger Beschleunigungsvektoren (Betrag ≥0,55 g, Skalarprodukt mit voriger Richtung <−0,15 g²). Hoher Pegel allein genügt nicht.
- **Rotate:** Die Drehgeschwindigkeit wird für jede Achse im Beobachtungsfenster integriert, auch während leichter Handbewegung. Erst ab insgesamt 35 Grad Drehung wird Rotate ausgelöst; eine kurze Gyro-Spitze während einer linearen Bewegung genügt nicht. Vor- und Seitkippen während der Bewegung zählen damit als Rotate, wenn sie diese Winkeländerung erreichen. Wird die neue Lage anschließend ruhig mindestens 500 ms gehalten, kann zusätzlich Tilt ausgelöst werden.
- **Tilt:** die aktuelle, ruhig gehaltene Schwerkraftrichtung weicht mehr als 25 Grad von der Startlage ab, für mindestens 500 ms bei Betrag nahe 1 g und Drehrate <15 Grad/s. Die Pose verwendet nicht die dynamische Bewegungsfilterung, damit Anheben oder Stoßen keine falsche Neigung erzeugt. Rückkehr unter 15 Grad löst den Neigungszustand. Während des weiteren Schiefhaltens alle 2 s ein Tilt-Ereignis. Tilt wird außerhalb eines aktiven Bewegungsfensters gemeldet.
- **Move:** mindestens 150 ms allgemeine Bewegung ab 0,30 g im Fenster ohne spezifischere Klasse. Stillstand in der Referenzlage erzeugt keine Beschäftigung.
- Ungültige Werte oder Samplelücken >150 ms verwerfen angefangene Fenster. Keine nachgeholten Gesten aus fehlenden Daten. Zeitstempel unterstützen `millis()`-Überlauf. Absolute magnetische Himmelsrichtung ist kein Eingang; Drehung und Neigung sind relative Größen aus Gyroskop/Beschleunigung.

| Ereignis | attention | annoyance | loneliness |
| --- | ---: | ---: | ---: |
| Move | +8 | 0 | −5 |
| Tap | +12 | 0 | −8 |
| MultiTap | +18 | ab 3 Taps: +6 | −12 |
| Impact | +20 | +15 | −10 |
| Rotate | +10 | 0 | −6 |
| Tilt, auch weiter gehalten | 0 | 0 | 0 |
| Shake | +25 | +10 | −15 |

Bewegung verändert weder energy noch depression unmittelbar; energy bleibt damit eindeutig musikalisch. Beschäftigung kann depression erst über den erhöhten Attention-Wert oberhalb 80 und über das Senken von loneliness beeinflussen. Alle Werte sättigen an 0/100. Die zeitabhängige Entwicklung wird vor einem Ereignis bis zu dessen Zeitstempel aktualisiert. `MotionEvent` hält Typ, Zeitstempel, Sequenznummer, Zahl kurzer Impulse und dynamischen Spitzenwert getrennt von der Stimmung. `MotionEvents::latest()` liefert das letzte Ereignis ohne zusätzliche Allokation. Das ist ein später nutzbarer Eingang für das kleine Sprachmodell; ein Modell und eine Ereignishistorie werden hier noch nicht integriert. Ereignisse sind Klassifikationshypothesen, keine gesicherte Interpretation menschlicher Absicht.

**Node-Lichtlayer:** Die Stimmungswerte bleiben ausschließlich im Mainframe. Er leitet daraus nach jeder Personality-Aktualisierung eine neutrale `NodeVisualState`-Nachricht ab und wiederholt sie alle drei Sekunden. Sie beschreibt nur die Ausgabe: RGB-Szenen- oder Vollfarbmodus, Helligkeit, eine künftige Farbüberlagerung und das Flash-Verhalten. Der Node speichert die Nachricht zunächst im Empfangs-Callback und setzt diese Render-Vorgaben anschließend außerhalb des Callbacks um. Er kennt dabei weder emotionale Namen noch Schwellenwerte. V1 kennt den zusätzlichen Nachrichtentyp nicht und verwirft ihn.

Die aktuelle Mainframe-Regel ist: bei annoyance ≥90 werden Vollfarbe Rot bei 100 % und Flash dauerhaft an gesendet; ansonsten werden bei loneliness ≥90 Szenenhelligkeit 15 % und Flash aus gesendet; sonst die normalen Szenenwerte. Annoyance hat bei Überschneidung Vorrang. Szene, Clock und Rendererzustand laufen unter den Render-Vorgaben weiter. Wenn der Mainframe wieder die normalen Werte sendet, setzt der nächste Rendererframe die aktuelle Szene sichtbar fort. NodeText und reguläre Flashimpulse sind während einer visuellen Vorgabe unterdrückt. Der Flash-Treiber hat keine echte Dimmstufe; Ausschalten ist die einfache Vorgabe. Farbüberlagerungen werden später als Farbe und Deckkraft aus dem Mainframe gesendet; der Node wendet sie lediglich auf seine gefilterte Szene an.

Hosttests für Personality, Motion, Audio, Tempo, Iris und Protokoll einschließlich NodeVisualState bestehen (`sh tests/run_all.sh`). Mainframe und Node bauen mit den festgesetzten Abhängigkeiten. Der Mainframe wurde am 26.09.2026 auf den angeschlossenen AtomS3R geflasht; jeder Node benötigt die neue Node-Firmware, bevor er die visuellen Vorgaben empfangen kann. Ein Hardwaretest der visuellen Vorgaben mit einem geflashten V2-Node steht aus. Der Stand ist committet.

**Button/Display:** Halteschwelle 1200 ms, einmaliges Umschalten während des Haltens. Kurzer Klick startet die Show bzw. wechselt die Szene beim Loslassen. Loslassen nach Langdruck löst keinen Klick aus. Der Button selbst verändert keine Stimmungswerte; physische Bewegung beim Bedienen kann weiterhin die IMU anregen. EyeRenderer zeichnet das Overlay mit fünf beschrifteten Zahlen und Balken im bestehenden 128×128-Canvas vor dem einzelnen Display-Push. Es verwendet den bisherigen 33-ms-Frameabstand und verdeckt einen Teil des laufenden Auges. Zusätzlich zeigt die Debug-Anzeige den letzten Ereignistyp und die Anzahl kurzer Impulse, damit die Unterscheidung am Gerät geprüft werden kann.

**Textausgabe:** `MessagePanel` ist die einzige Produkt-Textfläche. Es liegt fast vollflächig als PS1-artige, violette Einzelkontur mit abgeschnittenen Ecken über dem weiterlaufenden Auge; cyanfarbene Eckmarken akzentuieren die Kontur. Ein 75-Prozent-Scanline-Dither dunkelt das Auge an, lässt es aber sichtbar. Jede sichtbare Textzeile erhält nur hinter ihrem Inhalt einen eigenen dunklen, abgerundeten Block. Jede Quelle kann eine UTF-8-Nachricht über `show()` senden; Motion ist der erste angeschlossene Absender. Jede Bewegungsart hat zwei oder drei kurze lokale Varianten; die Auswahl rotiert deterministisch und wiederholt bei unmittelbar gleichen Ereignissen nicht dieselbe Variante. Jede lokale Motion-Textvariante endet mit einem passenden Emoji. Eine Nachricht schließt nach fünf Sekunden, auch wenn ihr auslösendes Ereignis fortbesteht. Der neunsekündige Cooldown gilt je Bewegungsart: Ein weiterer Tap wird gebremst, ein Stoß oder Schütteln darf sofort eine neue Nachricht anzeigen. Vier große, linksbündige Zeilen in einer festen Pixelschrift brechen an Wortgrenzen nach spätestens acht Textspalten um. Erst bei mehr als vier Zeilen läuft der Text nach 1,2 Sekunden zeilenweise nach oben und hält an, sobald die letzten vier Elemente sichtbar sind. Ein kleines weißes Dreieck mit Abstand zum Rahmen blinkt unten rechts, solange noch Text folgt. Der Umbruch trennt weder UTF-8-Folgen noch Emoji. Ein Emoji am Nachrichtenende ist ein weiteres, doppelt hohes Element dieses selben Zeilenflusses: Es wird erst nach den Textzeilen zentriert sichtbar und scrollt mit ihnen; sein kleiner Hintergrundblock erscheint nur in seiner jeweiligen Zeile. Ä, Ö, Ü, ä, ö, ü und ß sowie die kuratierte Emoji-Palette sind eigene Glyphen; unbekannte Unicode-Zeichen werden als `?` sichtbar. Die gesamte kuratierte Text-Emoji-Palette stammt aus einem freigegebenen 32-Pixel-Spriteatlas statt aus Laufzeit-Primitiven. Emoji-Gesichter und Hände sind porzellanweiß statt gelb, haben gestufte kaltblaue Plastizität, kräftigen Kajal und violetten Lidschatten unter beiden Augen; Augenringe erscheinen abhängig vom Ausdruck. Die Debug-Anzeige bleibt eine getrennte Diagnoseansicht.

**Prüfung:** `sh tests/run_personality.sh` prüft BPM-Ziele/Confidence, unabhängige loneliness, schnelle Stille/Reaktion auf Audio-Rückkehr und unbekannte Inputs sowie Depressionseinflüsse. `sh tests/run_audio_mood_features.sh` prüft synthetische 100-/1000-Hz-Signale, Lautstärkeunabhängigkeit des Bassverhältnisses, Aufwärmzeit, Stille, DC, Capturelücken, Clipping und Zeitüberlauf. `sh tests/run_motion_events.sh` prüft synthetische Spuren für Tap/MultiTap, Impact, Rotate, gehaltenen Tilt, Shake, Stillstand, Datenlücken, ungültige Samples und Zeitüberlauf. `sh tests/run_message_panel.sh` prüft Lebensdauer, Motion-Anbindung und den Textdurchlauf. Dazu alle übrigen Hosttests über `sh tests/run_all.sh` sowie `pio run -e mainframe`. Hardwareabnahme steht aus: dieselben Gesten mehrfach ausführen, letzte Ereignisklasse im Overlay vergleichen, Tippstärke/Montage/Bassvibrationen prüfen, Neigungsreferenz beim Start beachten, Audio-/Clock-Kontinuität unter IMU- und Displaylast messen. Synthetische Tests belegen die Regeln, nicht deren Trennschärfe am Gehäuse. Am 26.09.2026 auf ausdrücklichen Nutzerauftrag auf den AtomS3R an `/dev/cu.usbmodem101` geflasht (ESP32-S3-PICO-1, MAC `14:c1:9f:d5:6e:ec`): Upload erfolgreich, Flash-Hash verifiziert, Hardware-Reset ausgelöst. Die anschließende BPM-/Bass-/Stille-Version wurde auf demselben Gerät ebenfalls erfolgreich geflasht, Hash verifiziert und Reset ausgelöst (Flash 917.901 Byte, statisches RAM 55.164 Byte laut Build). Alle damaligen sieben Hosttests bestanden. Der anschließend auf 0,1 Punkt/s verlangsamte Annoyance-Abbau wurde mit Personality-/Protokolltests und Mainframe-Build geprüft und ebenfalls erfolgreich geflasht (Hash verifiziert, Reset ausgelöst). Energy kann bei auslaufender Tempo-Evidenz auf dem erreichten Wert stehen bleiben; die vom Nutzer gemeldete dauerhaft hohe Energy ist ohne aktuelle Eingangs-Messwerte noch nicht einer Ursache zugeordnet. Bass-/Stilleschwellen, Erkennungsqualität und Laufzeit unter realer Musiklast bleiben am Gerät zu prüfen. Diese Flash-Vermerke betreffen den Stand vom 26.09.2026; spätere Flashes (Creature-Sounds, Tempo-Arbeit) sind hier nicht einzeln protokolliert. Der Code ist vollständig committet; am 14.11.2026 bestehen alle 15 Hosttests.

## C. Technischer Abgleich, offene Entscheidungen und Experimente

### C.1 Hardwareabgleich – Herstellerangaben, keine Codeverifikation

Am 22.09.2026 anhand offizieller M5Stack-Dokumentation geprüft:

- Die genannten AtomS3R-Eckdaten stimmen. Zusätzlich besitzt er einen BMM150-Magnetometer; dessen Nutzung ist nicht verlangt. Interner BMI270-Bus: SDA G45/SCL G0, Button G41, Grove G2/G1. Display benötigt einen anderen Treiberpfad als M5Atom-Matrix. Quelle: [AtomS3R](https://docs.m5stack.com/en/core/AtomS3R).
- Atomic Voice Base verwendet einen ES8311-Audiocodec und I²S. Der bisherige PDM-Treiber samt GPIO26/32 ist nicht direkt übernehmbar. Quelle: [Atomic Voice Base](https://docs.m5stack.com/en/atom/Atomic%20Echo%20Base).
- M5Stack dokumentiert die Kombination S3R + Voice Base: Codec-I²C SDA G38/SCL G39; I²S-Ausgang vom Host G5, LRCK G6, Eingang zum Host G7, BCLK G8. Quelle: [AtomS3R + Voice Base](https://docs.m5stack.com/en/core/AtomS3R-AI%20Chatbot).
- U185/STHS34PF80 verwendet I²C-Adresse 0x5A, Präsenz-/Bewegungserkennung, keinen Kamerabildstrom. Quelle: [Unit TMOS PIR](https://docs.m5stack.com/en/unit/UNIT-TMOS%20PIR).

Vor dem Port sind Board-/Flash-/PSRAM-Einstellungen, tatsächliche Hardwarevariante, Codec-Gain, PCM-Format, Displayinitialisierung und I²C-Busbelegung gemeinsam zu prüfen. Interne Sensoren, Codec und Grove liegen an verschiedenen Pinpaaren; die Bibliotheks- und Busverwaltung darf nicht als automatisch konfliktfrei angenommen werden. Elektrische/mechanische Gesamtintegration wurde hier nicht am Gerät geprüft.

### C.2 Ausdrückliche Widersprüche bzw. noch nicht erfüllte Anforderungen

| Planung/Annahme | Befund und Konsequenz |
| --- | --- |
| Enhanced Features dürfen die Show nicht voraussetzen oder beeinträchtigen | Offline-Grundpfad existiert, aber aktive Cloud-Sessions pausieren die Mainframe-Funksteuerung. Die starke Prioritätsanforderung ist damit noch nicht erfüllt. |
| Ein Mainframe mit WLAN und ESP-NOW | Mögliches Ziel, aber kein unabhängiger Zweitkanal. Mit festen Nodes auf Kanal 1 funktionieren beliebige AP-Kanäle nicht gleichzeitig zuverlässig. |
| Bestehendes System einfach auf S3R übernehmen | Algorithmische Teile sind übertragbar; Board, Matrix, GPIOs, Mikrofontreiber und SDK-Schnittstellen brauchen einen wirklichen Port. |
| Bestehende Szenen durchschalten | Zwei einkompilierte Szenen (A.3) machen Übernahmefehler sichtbar; ihr endgültiges Lichtdesign ist offen. |
| Audio/BPM stabil übernehmen | Der alte Scanner ist block-/loopabhängig und benutzt absolute Pegelschwellen. V2 hat ihn deshalb nicht stabilisiert, sondern am 03.10.2026 als Clock-Quelle durch einen pegelnormierten PCM-Tracker mit Sample-Zeitbasis ersetzt. „Stabil“ ist für den neuen Pfad am Gerät noch nicht belegt (Abschnitt 0). |
| Iris übernimmt bestehendes VU | Als Pegel-/Farbübernahme mit 36 Iris-Polygonen umgesetzt (Abschnitt 0); Pegelschwellen sind vorläufig. |
| Gesamte Installation bei annoyance rot | Nur farbfähige Nodes können rot werden. Umgesetzt als `NodeVisualState` mit Vollfarbmodus und Vorrang vor der Szene (B.6); Flash-Nodes schalten dauerhaft an. Test auf Node-Hardware offen. |
| Lokales Sprachmodell auf 8 MB PSRAM | Unbewiesenes Experiment; verfügbare Speicherkapazität ist kein Nachweis für passende Qualität, Modellgröße oder Latenz. Kein Bestandteil des Offline-Kernversprechens. |

Espressif dokumentiert die Kanalbindung und dass selbst erfolgreiche MAC-Zustellung keine Verarbeitung auf Anwendungsebene garantiert. Quelle: [ESP-NOW für ESP32-S3](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html). Die dortige aktuelle API ist keine Aussage über die im bestehenden PlatformIO-Build installierte SDK-Version.

### C.3 Offene Designentscheidungen – keine vorweggenommene Implementierung

- **Szenenprotokoll:** Szenenindex und idempotente Auswahl sind implementiert; Wiederholung alle 30 Sekunden. Keine Mainframe-Sitzung und keine Ein-Sekunden-Wiederholung. Eigene V2-Funkkennung ist implementiert, am Host geprüft und auf dem Mainframe geflasht; die End-to-End-Prüfung mit einem V2-Node steht aus. Offen bleiben eine mögliche Empfangsqueue bei künftig mehr Nachrichtentypen, optionale Zustandsbestätigungen sowie Messung von Wiederholungsintervall und Rejoin-Verhalten auf Hardware. Keine gemeinsame Szenenbarriere ist vorgesehen.
- **Zeitbasis:** Empfangszeitstempel, echtes Beatserial vs. Sync-Paketserial, latenzbewusster Phasenabgleich, Umgang mit verspäteten Paketen und nichtblockierende Node-Ausgabe. Keine erfundene musikalische Eins aus dem bisherigen Onsetdetektor ableiten.
- **WLAN-Politik:** Eine mögliche Basispolitik wäre, bei unverändertem ESP-NOW-Kanal 1 nur passende WLAN-Netze als Enhancement zuzulassen und sonst offline zu bleiben. Gemeinsame Kanalwechsel aller Nodes sind eine andere, deutlich aufwendigere Option. Noch keine Auswahl getroffen; reine Holdover-Wiedergabe ersetzt nicht die geforderte kontinuierliche Steuerbarkeit.
- **Audioport:** Capture von Analyse trennen; feste Framezeit bzw. echte Sample-Zeitbasis als zu prüfende Verbesserung. Zunächst alte Signalcharakteristik und Confidence-/Lockverhalten vergleichen, nicht stillschweigend den Algorithmus austauschen. *Ist-Stand:* Capture-Ring und Sample-Zeitbasis sind umgesetzt. Der Algorithmus wurde am 03.10.2026 ausgetauscht; ein dokumentierter Vergleich desselben Materials zwischen altem und neuem Pfad liegt nicht vor. Offen sind außerdem Phasennachführung, Verhalten bei Stille/Break und der Umgang mit Halb-/Doppeltempo in der Übernahmestufe.
- **Audioqualität:** Clipbereitschaft, Clipping, Pegel, zeitliche Kontinuität und tatsächliche Aufnahmelänge bewerten. Die vorhandene 12-s-Anfrage garantiert bei RAM-Kürzung oder verlorenen DMA-Samples keinen lückenlosen 12-s-Clip.
- **Display:** Iris-Polygonmapping und Textlayout sind umgesetzt (Abschnitt 0, B.6); offen bleiben Framebudget und Auslassen von Frames bei Last. Speaker ist Hardwareumfang, aber noch kein geplanter sprachlicher Personality-Ausgang.
- **Sensorik:** TMOS-Präsenz benötigt zeitliche Filterung und ist kein Personenidentifikator. IMU-Klassen und Abtastrate sind heuristisch festgelegt (B.6); Montage, Abnahme am Gehäuse und Trainingsdaten sind offen. Bass/Vibration und leichtes Tippen müssen am tatsächlichen Gehäuse unterscheidbar sein; Speakerbetrieb kann Audio- und Vibrationsinputs beeinflussen.
- **Personality:** Eventregeln, Abklingkurven, Textauswahl/Cooldowns und zwei Licht-Overrides sind mit vorläufigen Werten umgesetzt (B.6). Offen bleiben Bestätigung dieser Kalibrierwerte, Persistenz, Kombination mehrerer States und weitere Lichtwirkungen.
- **TinyML:** zunächst messbarer Bedarf und Datensatz, dann Modellentscheidung; getrennte Optionen für IMU und Audio.
- **Lokales Sprachmodell:** erst am Ende Machbarkeit mit Ressourcenbudget prüfen; lokale Textpakete bleiben die funktionierende Basis.

### C.4 Vorgeschlagene spätere Abnahme, noch nicht durchgeführt

1. Gegenseitige Ablehnung aller fünf V2-Pakettypen zwischen V1 und V2 bei weiterhin gültiger V2-Kommunikation; anschließend Offline-Start aller Node-Typen/Rollen; unterschiedliche Bootreihenfolge; Node-/Mainframe-Neustart; Rollenwechsel; mehrfacher Szenenwechsel und gezielte Paketverluste.
2. Mehrfacher Wechsel zwischen beiden Szenen, auch während laufender Beats; keine unbeabsichtigte Wartephase bei Szenenwechseln und Messung der Übernahme nach Paketverlust: Aktuell folgt erst nach 30 Sekunden die nächste periodische Wiederholung; ein dreifacher `SceneSelect`-Burst ist nicht implementiert.
3. Clockmessung unter Flash-/RGB-Last, ohne Netz sowie mit WLAN-Ausfall und inkompatiblem AP-Kanal. Display-/Cloudlast darf Audio und Node-Steuerung nicht aushungern.
4. Vergleich desselben Audio-Testmaterials vor/nach Port: erkannte BPM, Clock-Ziel, Show-BPM, Confidence, Lockzeit, Phasenfehler, Halb-/Doppeltempo, Breaks/Stille, Sprache/Tippen und Signalübersteuerung. Für den neuen Tempopfad gibt `mainframe-beat-tracker` Tracker-BPM, Rohwert, Confidence und Show-BPM seriell aus; Phasenfehler und Lockzeit werden nicht ausgegeben, und der alte Pfad ist nicht mehr parallel angebunden.
5. Erst danach zusätzliche Analyzer-Merkmale und Personality/Sensorik, anschließend experimentelle Modelle.

Alle Punkte in C sind offene Entscheidungen, abgeleitete Prüfbedarfe oder ausdrücklich bezeichnete Experimente. Sie dürfen nicht als bereits vorhandene Funktion oder als beschlossene konkrete Softwarearchitektur behandelt werden.
