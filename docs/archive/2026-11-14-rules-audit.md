# Regel-Audit vom 14.11.2026

> **Archiviert.** Abgeschlossenes Protokoll vom 14.11.2026, inhaltlich unverändert. `docs/MAINFRAME_V2.md` heißt inzwischen `docs/project-context.md`, `docs/v1-outline.md` liegt unter `docs/archive/`.

Bestandsaufnahme aller Arbeitsregeln für Coding-Agents und Nachweis, was mit jeder Regel geschehen ist. Geprüft gegen den Code und `git log` auf Stand `2786c11`. Zeilennummern in `docs/MAINFRAME_V2.md` beziehen sich auf diesen Commit und verschieben sich, sobald die Doku-Session gemergt ist; das Zitat dient dann zum Wiederfinden. Die Doku-Session hat `README.md` und `docs/MAINFRAME_V2.md` inzwischen ins Englische übertragen; nach ihrem Merge passen deshalb auch die deutschen Zitate nicht mehr, und die Tabelle in Abschnitt 5 ist anhand der Abschnittsnamen zuzuordnen.

Geändert wurden in diesem Schritt nur `AGENTS.md` und `CLAUDE.md`. `README.md`, `docs/MAINFRAME_V2.md` und `workers/decaflash/README.md` sind unverändert. Die Ergänzung von `.claude/settings.json` ist nicht angewendet, siehe Abschnitt 4.

**Nachtrag vom selben Tag:** Der Nutzer hat entschieden, `AGENTS.md` aufzulösen. Alle Regeln stehen seither unverändert im Wortlaut in `CLAUDE.md`; `AGENTS.md` ist gelöscht. Wo dieses Audit `AGENTS.md` als Verbleib oder Ziel nennt, ist `CLAUDE.md` gemeint. Begründung: Nach der Umstellung von Codex arbeitet nur noch Claude Code im Repo, und Claude Code liest nur `CLAUDE.md`. Folgearbeit daraus: R3 in Abschnitt 3.

Befundklassen: **gültig**, **Dublette**, **Widerspruch**, **veraltet**, **Statusnotiz** (Zustandsbeschreibung in Regelform), **Produktverhalten** (gehört in Doku bzw. später in eine OpenSpec-Spec).

## 1. Bisheriges `AGENTS.md`

| Nr. | Bisherige Regel | Befund | Verbleib und Begründung |
| --- | --- | --- | --- |
| A1 | „Read README.md first for current state and operational commands." | gültig | Unverändert übernommen (Orientation). |
| A2 | „Read docs/MAINFRAME_V2.md for confirmed decisions, detailed code analysis, hardware limits and implementation order." | gültig, ergänzt | Übernommen. Neu: bei Abweichung gelten Code und `git log`, weil das Dokument stellenweise älter ist als der Code (siehe Abschnitt 5). „implementation order" entfällt, weil die Reihenfolge in B.5 überholt ist (M19). |
| A3 | „Keep README concise; maintain detailed product context in MAINFRAME_V2.md rather than duplicating it." | gültig, umformuliert | Aufgegangen im Kopfabsatz: Stand in README, Entscheidungen in MAINFRAME_V2, Produktverhalten in Doku und später in `openspec/specs`. |
| A4 | „Worker-specific instructions belong in workers/decaflash/README.md. docs/v1-outline.md is historical." | gültig | Übernommen. |
| A5 | „Work only in this V2 repository. Do not modify the old Decaflash project or share mutable source files with it." | gültig | Übernommen (Boundaries). |
| A6 | „Keep apps/mainframe, apps/node and shared/include as the starting structure. V2 may change both Mainframe and Node firmware." | gültig | Übernommen. |
| A7 | „Reuse existing code selectively; proceed in small, verifiable steps. reference/m5atom_controller is a local ignored copy … for reading only" | gültig | Übernommen, in zwei Punkte geteilt. Ergänzt um „never build, edit or version" aus `.gitignore` und M13. |
| A8 | „V1 and V2 must reject each other's SceneSelect, ClockSync, MainframeHello and NodeText packets." | veraltet (Aufzählung) | Es gibt fünf Nachrichtentypen; `NodeVisualState` fehlt in der Liste (`shared/include/protocol.h:16`, geprüft in `tests/protocol_identity.cpp:57`). Ersetzt durch „every message type" und die Pflicht, für neue Typen einen Testfall zu ergänzen. |
| A9 | „The observed scene problem has no confirmed cause or verified hardware fix." | Statusnotiz | Als Zustand entfernt; er steht in MAINFRAME_V2 (M9). Die Absicht bleibt als Regel erhalten: keine Ursache und kein Fix für Hardwareverhalten ohne Messung am Gerät. |
| A10 | Architecture anchors (vier Pfade) | gültig | Übernommen (Orientation). |
| A11 | „mainframe targets AtomS3R and includes its validated bring-up plus the first offline ESP-NOW scene/clock path. node remains the M5Atom/FastLED firmware" | veraltet, Statusnotiz | Entfernt. Der Mainframe enthält inzwischen Audio/BPM, Auge, Personality, IMU und Sound (`apps/mainframe/src/`). Der Stand steht in README „Stand". |
| A11b | „FastLED is retained only for Node output." | gültig | Als Regel behalten (Dependencies). |
| A12 | „Pin PlatformIO platforms and hardware libraries after a successful device test. Update one dependency at a time …" | gültig | Übernommen. Ergänzt um die Zustimmung zum Flashen, damit die Regel nicht mit A-neu (Flash-Freigabe) kollidiert. Versionsnummern stehen nur in `platformio.ini`. |
| A13 | „Run sh tests/run_all.sh … and build the affected PlatformIO environments." | gültig | Übernommen. Ergänzt: Änderungen unter `shared/include` betreffen `mainframe` und `node`. |
| A14 | „AtomS3R and Atomic Voice Base are connected according to the user; confirm actual variants before porting. TMOS is absent and not currently needed." | Statusnotiz, veraltet | Entfernt. Der Port ist erfolgt, der Hardwaretest steht in M10. Offen bleibt nur die Display-Revision; sie steht in MAINFRAME_V2, Abschnitt „Hardwarestand". |
| A15 | „Voice Base requires an I²S/codec port of the existing PDM input. The old Matrix UI is 5×5; the target display is 128×128." | veraltet | Entfernt. Beides ist umgesetzt (`voice_base_input.cpp`, `eye_renderer.cpp`). Die Hardwarefakten stehen in C.1. |
| A16a | „Offline audio and Node communication take priority over display and cloud." | gültig, zugleich Produktverhalten | Behalten und mit B.1 zusammengeführt (M15, M33). Kandidat für eine spätere Spec; dann hier nur noch ein Verweis. |
| A16b | „ESP-NOW currently uses channel 1; Wi-Fi association can change that channel." | gültig | Behalten und zur Regel gemacht: jede WLAN-Funktion braucht vorher eine Nutzerentscheidung zur Kanalpolitik. C.3 hält fest, dass diese Entscheidung offen ist (M30). |
| A16c | „Existing cloud jobs pause ESP-NOW." | veraltet | Entfernt. `apps/mainframe/src` enthält keinen WLAN-, HTTP- oder Cloud-Code; die Aussage beschreibt den V1-Controller (`radioPauseActive` kommt nur in `reference/` vor). Der Sachverhalt steht in A.7. |
| A17 | „Morse output is a BPM/beat-clock scheduling problem, not just a visual effect." | Produktverhalten | Entfernt. Das Timing steht in A.6 („Start auf nächstem lokalen Beat. Einheit ist ein halber Beat"). Ziel: künftige Spec für Node-Text. Hinweis: Der V2-Mainframe sendet derzeit kein `NodeText`; nur der Node-Empfänger und der Paketkonstruktor existieren. |
| A18 | „Audio analysis depends on loop timing and signal level; validate codec/gain and timing effects when porting." | gültig, umformuliert | Der Port ist erfolgt. Verallgemeinert und mit M31 zusammengeführt: vor dem Ersetzen des produktiven Pfads am Gerät vergleichen, kein stiller Algorithmustausch. |
| A19 | „Keep include/wifi_credentials.h and include/cloud_config.h local and ignored; their .example.h files are tracked templates." | gültig | Übernommen, `.dev.vars` in denselben Punkt gezogen. |
| A20 | „Never print or commit local secrets, including Worker .dev.vars." | gültig | Übernommen. |
| A21 | „Do not carry over the old Cloudflare deployment connection, Worker or KV resources automatically. Deployment is not part of the current work." | gültig | Übernommen, mit W1 zusammengeführt. |
| A22 | „When deployment is requested later, the normal path is Git/Cloudflare integration; do not assume local Wrangler is installed or needed." | Dublette | Aus `AGENTS.md` entfernt. Steht wortgleich im Worker-README (W3); `AGENTS.md` verweist dorthin. |
| A23 | „Recording already requests up to 12 seconds of µ-law audio … do not describe 8–12-second support as absent." | Statusnotiz zu V1-Code | Entfernt. Der Aufnahmepfad existiert nur in `reference/m5atom_controller`, nicht im V2-Build. Der Sachverhalt steht in A.7 (M14). |
| A24 | „musicPresent() is a coarse local gate, not a reliable recognition-readiness signal." | Statusnotiz zu V1-Code | Entfernt. `musicPresent()` existiert in V2 (`beat_analyzer.h:12`), aber es gibt keinen Erkennungspfad, der es als Bereitschaftssignal nutzen könnte. Steht in A.7. |
| A25 | „Improve clip quality/readiness before spending further AudD calls; communication and the hardware port come first." | veraltet, Absicht unklar | Die Priorisierung ist überholt (kein AudD-Pfad in V2, Port erledigt). Die Kostenabsicht ist als neue Regel erhalten: kostenpflichtige Dienste nur auf Nutzerwunsch. Siehe Frage F2. |

## 2. Bisheriges `CLAUDE.md`

| Nr. | Bisherige Regel | Befund | Verbleib und Begründung |
| --- | --- | --- | --- |
| K1 | „Run sh tests/run_all.sh for all host tests. After firmware changes, also build each affected environment" | Dublette zu A13 | Aus `CLAUDE.md` entfernt, steht in `AGENTS.md` (Verification). |
| K2 | „Do not read apps/mainframe/src/panel_text_sprites.h. It is a generated sprite atlas of about 7,000 lines" | gültig | Nach `AGENTS.md` verschoben, weil der Kontextverbrauch jeden Agenten betrifft. „generated" durch „embedded" ersetzt und um „nicht von Hand ändern" ergänzt: Im Repo liegt kein Generator. Siehe Frage F5. |
| K3 | „Flashing (pio run -t upload) and serial monitoring need explicit approval from the user every time. A successful build is not a device test." | gültig; zweiter Satz Dublette zu R1 | Nach `AGENTS.md` verschoben, damit die Regel für alle Agenten gilt. M27 belegt, dass schon bisher nur auf Nutzerauftrag geflasht wurde. Siehe Frage F1. |
| K4 | „docs/MAINFRAME_V2.md is large. Read only the section the task needs." | gültig | Nach `AGENTS.md` verschoben (Orientation). |

Zwischenstand: `CLAUDE.md` enthielt nur noch den Import `@AGENTS.md` und einen Hinweis darauf, was `.claude/settings.json` erzwingt. Nach dem Nachtrag oben enthält `CLAUDE.md` alle Regeln selbst; der Hinweis steht dort im Abschnitt „Permissions".

### Neue Regeln in `AGENTS.md`

Beide Regeln hat die Hauptsitzung der Umstellung als vom Nutzer bestätigt übermittelt; sie standen bisher in keiner Datei des Repos.

| Nr. | Regel | Befund | Quelle |
| --- | --- | --- | --- |
| N1 | Gearbeitet wird in einem Git-Worktree auf eigenem Branch. Kleine Änderungen dürfen direkt auf `main`. Worktree-Branches werden erst nach Zustimmung des Nutzers nach `main` gemergt. | neu, vom Nutzer bestätigt | Nutzer, wörtlich: „außer bei kleinen änderungen, die auch auf main gehen können sollten wir immer in worktrees arbeiten." Eine erste Fassung zählte Beispiele für „klein" auf (einzeilige Doku-Korrektur, `.gitignore`-Eintrag); der Nutzer hat das als zu detailliert und einschränkend verworfen. Was klein ist, bleibt Ermessenssache. |
| N2 | Jede Commit-Nachricht ist ein Conventional Commit, `type(scope): summary`, auf Englisch. | neu, vom Nutzer bestätigt | Vorgabe der Umstellung; „auf Englisch" stammt aus dem Auftrag an diese Sitzung. |

## 3. `README.md` und `workers/decaflash/README.md`

Beide Dateien sind unverändert. Für das Worker-README war kein Eingriff nötig: Es ist die maßgebliche Stelle für Deployment-Regeln, und `AGENTS.md` verweist nur noch dorthin.

| Nr. | Stelle | Zitat | Befund | Vorschlag |
| --- | --- | --- | --- | --- |
| R1 | README, „Lokale Befehle" | „Ein erfolgreicher Build oder Host-Test ersetzt keine Geräteprüfung." | Dublette zu `AGENTS.md` | Belassen; der Satz richtet sich an Menschen. |
| R2 | README, „Lokale Befehle" | „Lokale Konfiguration bleibt außerhalb von Git" | Dublette zu A19 | Belassen. |
| R3 | README, „Orientierung", Z. 20–21 | „[AGENTS.md](AGENTS.md) \| Arbeitsregeln für alle Coding-Agents" | veraltet seit dem Nachtrag | Zeile löschen und die Folgezeile auf „[CLAUDE.md](CLAUDE.md), [.claude/settings.json](.claude/settings.json) \| Arbeitsregeln und Berechtigungen" ändern. Bis dahin ist der Link tot. Nicht angewendet, weil die README der Doku-Session gehört. |
| W1 | Worker-README, „Deployment-Status" | „Den alten Worker oder seine KV-Daten nicht durch ein V2-Deployment überschreiben." | gültig | Maßgebliche Stelle; Kurzform in `AGENTS.md`. |
| W2 | Worker-README, „Deployment-Status" | „Cloud-Deployment ist aktuell nicht beauftragt." | gültig, zugleich Statusnotiz | Belassen, bis ein Deployment beauftragt wird. |
| W3 | Worker-README, „Deployment-Status" | „Der vorgesehene Weg ist Git/Cloudflare-Integration; lokales Wrangler-Deployment wird nicht vorausgesetzt." | gültig | Maßgebliche Stelle; A22 dort aufgegangen. |
| W4 | Worker-README, „Debug Artifacts" | „treat wrangler.jsonc as the source of truth for the binding when deploying from Wrangler or Git" | gültig | Belassen. Gilt nur im Deployment-Fall und widerspricht W3 nicht. |
| W5 | Worker-README, „Mainframe Contract" | „Kurzer Matrix-Text" und der Vertrag insgesamt | Statusnotiz, teils veraltet | Der V2-Mainframe hat keinen Cloud-Client und keine Matrix. Der Vertrag beschreibt den V1-Controller. Bei einem späteren Cloud-Port neu fassen; kein Eingriff jetzt. |

## 4. `.claude/settings.json`

Die Datei ist unverändert. Meine Ergänzung wurde von der Berechtigungsprüfung der Sitzung abgelehnt, weil eine Agentensitzung ihre eigenen Berechtigungen nicht ändern soll. Der Nutzer muss sie selbst eintragen oder ausdrücklich freigeben.

| Regel in `AGENTS.md` | Heute erzwungen | Lücke |
| --- | --- | --- |
| Builds und Hosttests | `allow`: `pio run`, `pio run -e *`, `sh tests/*` | keine |
| Flashen nur mit Zustimmung | `ask`: `pio run *-t *`, `pio run *--target*` | `platformio run …`, `esptool` und `python -m esptool` sind nicht erfasst. Ob `pio run *-t *` auch `pio run -t upload` ohne `-e` trifft, hängt davon ab, ob der Platzhalter die leere Zeichenkette erlaubt; zwei ausdrückliche Muster beseitigen den Zweifel. |
| Serielles Mitlesen ist frei (Entscheidung E1) | `ask`: `pio device monitor*` | Die Einstellung ist jetzt strenger als die Regel. `pio device monitor*` aus `ask` entfernen oder nach `allow` verschieben. |
| Abhängigkeiten nicht nebenbei ändern | nichts | `pio pkg …`, `pio platform …`, `pio upgrade` laufen ohne eigene Nachfrage. |
| Kein Deployment | nichts | `npm run deploy` (im Worker-`package.json` als `wrangler deploy` definiert) und `wrangler deploy`. |
| Secrets nicht lesen oder ändern | `deny`: `Read` und `Edit` für die drei Dateien | Shell-Befehle wie `cat` werden von `Read`-Regeln nicht sicher erfasst; hier trägt die Regel in `AGENTS.md`. |
| Push nur auf Auftrag (Entscheidung E6) | nichts | `git push`. |

Vorgeschlagener `ask`-Block:

```json
"ask": [
  "Bash(pio run -t *)",
  "Bash(pio run --target*)",
  "Bash(pio run *-t *)",
  "Bash(pio run *--target*)",
  "Bash(platformio *)",
  "Bash(esptool*)",
  "Bash(python* -m esptool*)",
  "Bash(pio pkg *)",
  "Bash(pio platform *)",
  "Bash(pio upgrade*)",
  "Bash(npm run deploy*)",
  "Bash(*wrangler deploy*)",
  "Bash(git push*)"
]
```

`deny` bleibt wie es ist. In `allow` kann `Bash(pio device monitor*)` ergänzt werden (E1). Sobald der Block eingetragen ist, den Hinweis in `CLAUDE.md` um Abhängigkeiten, Deployment und Push ergänzen.

## 5. Regelartige Passagen in `docs/MAINFRAME_V2.md`

Nicht angewendet. Die Tabelle ist nach dem Merge der Doku-Session abzuarbeiten. Ziel „AGENTS.md (übernommen)" bedeutet: Die Regel steht dort bereits, die Passage kann in der Doku zur reinen Beschreibung werden.

| Nr. | Abschnitt, Zeile | Zitat | Befund | Ziel |
| --- | --- | --- | --- | --- |
| M1 | Kopf, Z. 5 | „Es ist keine Implementierung oder Freigabe einer Implementierung." | veraltet | Löschen. Der Satz stammt aus der reinen Analysephase. |
| M2 | Kopf, Z. 7 | „Alle versionierten V2-Quellen und Dokumente verwenden diese Bezeichnung [Mainframe]." | gültig | AGENTS.md (übernommen). |
| M3 | Kopf Z. 9, Abschnitt 0 Z. 11–13 | „steht verbindlich in Abschnitt 0"; „Verbindlicher V2-Stand vom 25.09.2026"; „ersetzen widersprechende ältere Planungsannahmen" | Widerspruch, veraltet | „Verbindlich" streichen. Abschnitt 0 enthält selbst überholte Aussagen (M4) und ist zugleich Änderungsprotokoll. Stand nach README, Protokoll nach `git log`. |
| M4 | 0 „Eigenständiges Projekt", Z. 22 | „Cloud und Audioanalyse sind noch bewusst nicht enthalten." | veraltet | Löschen bzw. korrigieren. Audioanalyse ist enthalten (`beat_analyzer.cpp`, `v2_tempo_tracker.cpp`). Dieselbe Formulierung stand in `AGENTS.md` (A11). |
| M5 | 0, Z. 20–21 | „Alte Hardware und altes Projekt bleiben unabhängig funktionsfähig und unverändert. Keine gemeinsam veränderlichen Quellcodedateien." | Dublette zu A5 | In der Doku als Feststellung belassen oder löschen; Regel steht in AGENTS.md. |
| M6 | 0 „Iris-VU", Z. 27 und 31 | „Codec/Gain und Raumgeräusch müssen am Gerät geprüft werden"; „Zusätzlich sind Protokoll-, BeatAnalyzer- und AudioFollower-Tests auszuführen sowie pio run -e mainframe." | Statusnotiz; Dublette zu A13 | Testanweisung löschen (`tests/run_all.sh` deckt sie ab). Offene Kalibrierung als offener Punkt behalten. |
| M7 | 0 „PCM-Tempo-Tracker", Z. 35 | „Die neue Schätzung steuert weder Energy noch Node-Clock und kann deshalb erst nach Gerätevergleich aktiviert werden." | veraltet (laut `git log`) | Korrigieren. Commits `30ac9a7` „Use PCM tempo estimator as V2 clock source" und `67c4f96` „Confirm V2 tempo before show update" sprechen dagegen; im Code gegenprüfen. Der Regelkern (erst Gerätevergleich, dann aktivieren) steht in AGENTS.md. |
| M8 | 0 „PCM-Tempo-Tracker", Z. 39 | „ESP-DSP/FFT bleibt bewusst unintegriert … ein Frameworkwechsel ist dafür nicht vorgesehen." | gültig als Entscheidung zur ESP-DSP-Frage | In der Doku belassen; keine Regel (E8). |
| M9 | 0 „Funktrennung", Z. 55–57 | „beide V2-Gerätetypen müssen später mit der neuen Kennung geflasht werden"; „Die Ursache des gemeldeten Szenenproblems ist offen; diese Änderung ist kein Szenenfehler-Fix." | Statusnotiz | In der Doku belassen. Die Regelanteile stehen in AGENTS.md (Radio identity, Verification). |
| M10 | 0 „Hardwarestand", Z. 65 | „Vor dem Port sind tatsächliche Varianten und Pinbelegung zu bestätigen." | veraltet | Umformulieren: Der Port ist erfolgt; offen ist nur die Display-Revision. Entspricht A14. |
| M11 | 0 „USB-CDC-Debugpfad", Z. 67–69 | „Verbindlicher USB-CDC-Debugpfad"; „Ein erneutes Entfernen dieses expliziten Aufrufs würde die serielle Diagnose wieder unzuverlässig machen." | gültig | Als Beschreibung belassen. Die Invariante ist im Code geschützt (Kommentar `apps/mainframe/src/main.cpp:385–387`); keine eigene Zeile in AGENTS.md nötig. |
| M12 | 0 „USB-CDC-Debugpfad", Z. 71 und 73 | „zuerst mit pio device list prüfen"; „dort ist ein einfacher lesender USB-CDC-Client statt einer Firmwareänderung erforderlich"; „Tests immer mit aktivierter Audioanalyse durchführen und nach der Messung wieder den normalen mainframe-Build flashen." | gültig | AGENTS.md (übernommen, Verification). Der konkrete Port `/dev/cu.usbmodem101` bleibt als Statusangabe in der Doku. |
| M13 | A.1, Z. 115 | „Die lokale Lesekopie ist ignoriert und wird nie gebaut oder versioniert." | Dublette zu A7 | AGENTS.md (übernommen). Der Rest des Absatzes ist veraltet, siehe Abschnitt 6. |
| M14 | A.7, Z. 370–372 | „Normaler Worker-Deploy ist Git/Cloudflare-Integration."; „Damit sind die Hinweise auf ausschließlich kurze Clips und noch fehlende 8–12-s-Unterstützung überholt." | Dublette zu W3; Statusnotiz zu V1-Code | Deploy-Satz löschen (steht im Worker-README). Den Aufnahmepfad als V1-Referenz kennzeichnen. Nimmt A23 und A24 auf. |
| M15 | B.1, Z. 384 | „Die eigentliche Lichtshow muss vollständig ohne Access Point, Internet und KI-Dienste funktionieren. … Display hat geringere Priorität als Audio und Node-Kommunikation." | gültig, Produktverhalten | Künftige Spec (Offline-Kern). Kurzform steht in AGENTS.md (Boundaries). |
| M16 | B.2, Z. 398 | „Persönlichkeit äußert sich zunächst ausschließlich über Text." | Widerspruch | Mit M21 („keine Reaktionstexte") und dem Sound-Abschnitt (Z. 41–49) unvereinbar. Bereinigen; Ziel künftige Spec (Personality). |
| M17 | B.3, Z. 404 und 414 | „nicht durch ein Sprachmodell"; „Das Modell wäre ausschließlich der ‚Mund'; Zustandsentwicklung und Lichtentscheidungen bleiben lokal determinierbar." | gültig, Produktverhalten | Künftige Spec (Personality). |
| M18 | B.4, Z. 420 | „Zuerst bestehenden Scanner verstehen, messen und stabil übernehmen." | veraltet | Löschen oder als erledigt kennzeichnen; der V2-Tracker ist ein Neubau. |
| M19 | B.5, Z. 422–436 | „Verbindliche Reihenfolge der vorgesehenen Arbeit" und zugleich „Diese Reihenfolge ist Planung, kein Auftrag zur Umsetzung in diesem Analyseschritt." | Widerspruch, veraltet | Löschen oder durch eine kurze Liste offener Arbeit ersetzen. Schritte 5 bis 9 sind umgesetzt, Schritt 3 und der Test mit einem V2-Node sind offen. Siehe Frage F3. |
| M20 | B.5, Z. 426 | „Szenenproblem reproduzieren und anhand von Messungen eingrenzen; keine Ursache vorwegnehmen." | gültig | AGENTS.md (übernommen, Verification). |
| M21 | B.6, Z. 440 | „keine Reaktionstexte, zunächst nur fünf lokale Parameter"; „müssen als getrennte Ereignisse verfügbar sein" | veraltet, Produktverhalten | `MessagePanel` zeigt inzwischen Reaktionstexte (Z. 495). Künftige Spec (Personality, Motion). |
| M22 | 0 „Creature-Sound", Z. 45–47 | „Ab dann bleibt das Mikrofon aktiv. Es gibt keine Creature-Sounds, keinen I2S-Wechsel und keine Pause …" | gültig, Produktverhalten | Künftige Spec (Audio-Lebenszyklus). |
| M23 | B.6, Z. 452 und 456 | „vorläufige Kalibrierwerte, keine vom Nutzer einzeln bestätigten Charaktereigenschaften"; „es gilt niemals als Stille" | Statusnotiz; Produktverhalten | Kalibrierhinweis in der Doku belassen. Verhalten in künftige Spec (Personality). |
| M24 | B.6, Z. 458 | „Vorhandene DC-/VU-/BeatAnalyzer-Pfade und Bibliotheksversionen bleiben erhalten." | Statusnotiz | Löschen; Änderungsprotokoll eines einzelnen Schritts. |
| M25 | B.6, Z. 460 | „Die Bassgewichte und Schwellen müssen mit der Voice Base und realem Musikmaterial geprüft werden." | Statusnotiz | Als offener Prüfpunkt belassen. |
| M26 | B.6, Z. 489, 491, 497 | „Kein Commit." (dreimal) | veraltet | Löschen. Alle drei Stände sind committet. Siehe N1, N2 und Frage F6. |
| M27 | B.6, Z. 497 | „Am 26.09.2026 auf ausdrücklichen Nutzerauftrag auf den AtomS3R … geflasht" | Statusnotiz | Protokolleintrag; löschen oder kürzen. Belegt die Flash-Freigabe (K3). |
| M28 | B.6, Z. 497 | „Hardwareabnahme steht aus: dieselben Gesten mehrfach ausführen …" | Statusnotiz | Künftige Spec als Abnahmeszenario; bis dahin als offener Punkt belassen. |
| M29 | C.1, Z. 510 | „die Bibliotheks- und Busverwaltung darf nicht als automatisch konfliktfrei angenommen werden" | gültig | In der Doku belassen. Wird erst mit TMOS relevant und ist zu speziell für AGENTS.md. |
| M30 | C.3, Z. 530 und 531 | „Keine erfundene musikalische Eins aus dem bisherigen Onsetdetektor ableiten."; „Noch keine Auswahl getroffen" (WLAN-Politik) | gültig, Produktverhalten; offene Entscheidung | Künftige Spec (Clock). Die offene WLAN-Entscheidung ist in AGENTS.md als Vorbehalt übernommen (A16b). |
| M31 | C.3, Z. 532 | „Zunächst alte Signalcharakteristik und Confidence-/Lockverhalten vergleichen, nicht stillschweigend den Algorithmus austauschen." | gültig | AGENTS.md (übernommen, Verification). |
| M32 | C.2, Z. 516 | „Enhanced Features dürfen die Show nicht voraussetzen oder beeinträchtigen" mit Befund „aktive Cloud-Sessions pausieren die Mainframe-Funksteuerung" | Dublette zu M15; Befund veraltet | Anforderung in künftige Spec; den Befund als V1-Stand kennzeichnen (kein Cloud-Code in V2). |
| M33 | C.4, Z. 544 und 546 | „Display-/Cloudlast darf Audio und Node-Steuerung nicht aushungern."; „Erst danach zusätzliche Analyzer-Merkmale und Personality/Sensorik" | gültig; veraltet | Erster Satz: AGENTS.md (übernommen) und künftige Spec als Abnahmekriterium. Zweiter Satz löschen; Personality und Sensorik sind umgesetzt. |
| M34 | C, Z. 548 | „Sie dürfen nicht als bereits vorhandene Funktion oder als beschlossene konkrete Softwarearchitektur behandelt werden." | gültig | In der Doku belassen. In AGENTS.md durch „Code und `git log` gelten" abgedeckt. |

## 6. Sachfehler in `docs/MAINFRAME_V2.md` ohne Regelcharakter

Nebenbefunde für die Doku-Session; nicht Teil der Regelbereinigung.

- Z. 39 nennt `sh tests/run_tempo_tracker.sh`. Die Datei existiert nicht; vorhanden sind `tests/run_v2_tempo_tracker.sh` und `tests/run_bpm_tracker.sh`.
- Z. 55 zählt für den Identitätstest Szene, Clock, Hello und Text auf. Der Test deckt auch `NodeVisualState` ab.
- Z. 96 „Noch keine Hardwareergebnisse erhoben" widerspricht den als erledigt markierten Schritten in Z. 91 und 94.
- Z. 115 „Es gibt noch kein Personality-State-System, keine IMU-/TMOS-Auswertung, … keine Speaker-Ausgabe" widerspricht `personality.cpp`, `motion_events.cpp` und `sound_engine.cpp`.
- Abschnitt A.7 steht unter „Verifizierter IST-Zustand aus dem Code", beschreibt aber WLAN, Cloud und Aufnahme des V1-Controllers. Im V2-Mainframe gibt es diesen Code nicht.
- Die Überschrift von Teil B lautet „noch nicht implementiert"; B.6 beginnt mit „Implementiert in personality.{h,cpp}".
- Z. 497 „Alle sieben Hosttests bestehen"; `tests/run_all.sh` führt inzwischen 15 aus.
- Z. 519 spricht von drei Testszenen; laut Z. 152 und `scene_programs.h` gibt es zwei.

## 7. Entscheidungen des Nutzers (Interview vom 14.11.2026)

Die Fragen F1 bis F7 aus der ersten Fassung dieses Audits sind im Interview beantwortet. Die Entscheidungen sind in `CLAUDE.md` umgesetzt und gehen den Einträgen in Abschnitt 1 und 2 vor. Verweise „siehe Frage F…" in den Tabellen oben meinen: F1 → E1, F2 → E2, F3 → E3 und E4, F4 → E8, F5 → E5, F6 → E6, F7 → E7.

| Nr. | Frage | Entscheidung | Wirkung auf die Regeln |
| --- | --- | --- | --- |
| E1 | Wie oft für Flashen und serielles Mitlesen fragen? (K3) | Nur das Flashen braucht Zustimmung. | Geändert: „Flashing needs the user's explicit approval. Reading the serial port does not." Der Zusatz „every time" entfällt; wie lange eine Zustimmung gilt, hat der Nutzer nicht festgelegt. |
| E2 | War „before spending further AudD calls" Kostenschutz oder Reihenfolge? (A25) | Kostenschutz. | Unverändert: kostenpflichtige Dienste nur auf Nutzerwunsch. |
| E3 | Gilt „Audio und Node-Kommunikation vor Display und Cloud" weiter? (A16a, M15, M33) | Ganz streichen. | Entfernt, einschließlich „display or cloud load must not starve them". Der Offline-Grundsatz bleibt. M15 und M33 gehen damit nur noch in eine künftige Spec, nicht mehr in die Regeln. |
| E4 | Steht der Funktest mit einem geflashten V2-Node vor weiterer Feature-Arbeit? (M19) | Ja, zuerst der Node-Test. | Neu: keine neue Feature-Arbeit an Funk oder Node-Ausgabe, bevor der End-to-End-Test am Gerät gelaufen ist. Die Regel ist befristet und entfällt nach bestandenem Test. Der Rest der Reihenfolge in B.5 bleibt Vorschlag „löschen" (M19). |
| E5 | Woher stammt `panel_text_sprites.h`? (K2) | Einmalig aus einem Bild konvertiert; kein Generator. | Regel präzisiert: „converted once from an image". |
| E6 | Selbstständig committen und pushen? (M26) | Commit frei, Push nur auf Auftrag. | Neu im Abschnitt „Git workflow". |
| E7 | Kanalpolitik für WLAN jetzt festlegen? (A16b, M30) | Erst entscheiden, wenn es ansteht. | Unverändert: vor jeder WLAN-Funktion eine Nutzerentscheidung. |
| E8 | Soll „kein Wechsel weg von Arduino/PlatformIO" als Regel gelten? (M8) | Nein, keine Regel. | Keine Zeile in `CLAUDE.md`. Ein Frameworkwechsel ist zu groß, um nebenbei zu geschehen, und die Regeln sollen schlank bleiben. M8 bleibt als Notiz in der Doku. |
| E9 | Ist das Szenenproblem noch offen? (A9, M9, M20) | Nein. Laut Nutzer ist es behoben und lange nicht mehr aufgetreten. | Das Beispiel „such as the open scene problem" ist aus der Regel zur Geräteprüfung gestrichen; die Regel selbst bleibt. Die Doku-Session hält den Abschluss in MAINFRAME_V2 fest (A.3, B.5 Schritt 3). Kein Messwert verbindet die Behebung mit einer Ursache. |

Damit ist keine Frage aus diesem Audit mehr offen.

## 8. Kürzung vom 14.11.2026

Der Nutzer hat die Regeln nach dem Interview kürzen lassen: von 34 auf 23 Regeln; mit N3 sind es 24. Maßstab: Eine Regel bleibt, wenn ohne sie etwas schiefginge und sie sich nicht aus dem Code oder aus normalem Arbeiten ergibt. Die Einträge hier gehen den Abschnitten 1 und 2 vor.

| Nr. | Regel vor der Kürzung | Verbleib | Begründung |
| --- | --- | --- | --- |
| S1 | Einstiegspunkte (`main.cpp` von Mainframe und Node, `protocol.h`, `espnow_transport.h`) (A10) | gestrichen | Ergibt sich aus der Verzeichnisstruktur und aus der README-Tabelle „Orientierung". |
| S2 | „The central device is called Mainframe in all versioned sources and documents." (M2) | gestrichen | Der Code verwendet den Namen durchgehend; wer dort arbeitet, übernimmt ihn. |
| S3 | „Keep apps/mainframe, apps/node and shared/include as the structure. Both the Mainframe and the Node firmware may change." (A6) | gestrichen | Die Struktur besteht; ein Umbau geschähe nicht nebenbei. |
| S4 | „Take code over from V1 selectively, and work in small steps that can each be verified." (A7) | gestrichen | Allgemeine Arbeitsweise, keine projektspezifische Regel. Der Teil zur Referenzkopie bleibt. |
| S5 | „FastLED is for Node output only." (A11b) | gestrichen | Steht in `platformio.ini`: Nur `env:node` bindet FastLED ein. |
| S6 | „Every sender uses the shared V2 header from protocol.h, and every receive path checks it." | gestrichen | `tests/protocol_identity.cpp` prüft die Konstruktoren; die Regel zur gegenseitigen Ablehnung und zum Testfall für neue Typen bleibt. |
| S7 | „A change to the magic, payload version or packet layout affects both firmwares. Build mainframe and node, and say that both device types need flashing." | zusammengeführt | Aufgegangen in der Build-Regel: Änderungen unter `shared/include` betreffen beide Firmwares, und beide Gerätetypen brauchen dann einen Flash. |
| S8 | Abschnitt „Permissions" (Beschreibung von `.claude/settings.json`) | gestrichen | Die Einstellungen wirken ohne Beschreibung. Der Satz „Do not route around a prompt or a denial" entfällt mit; das Umgehen einer Ablehnung ist ohnehin nicht zulässig. |
| S9 | Drei Zeilen zu Abhängigkeiten (A12) | zusammengeführt | Eine Zeile: Pins nur als eigener Schritt, eine Abhängigkeit zur Zeit, gebaut, auf ein Testgerät geflasht und dort geprüft. Die Zustimmung zum Flashen regelt die Flash-Regel. |
| S10 | „Run sh tests/run_all.sh …" und „Build each affected environment …" (A13) | zusammengeführt | Eine Zeile, Inhalt unverändert. |
| S11 | „Check the port with pio device list before using it." und die TTY-Regel (M12) | zusammengeführt | Eine Zeile, Inhalt unverändert. |
| S12 | „Audio analysis depends on loop timing and signal level. Before a change to capture, codec gain, loop load or tempo estimation replaces the productive path, compare old and new behaviour on the device. Do not swap an algorithm silently." (A18, M31) | gekürzt | Ein Satz: produktiven Audio- oder Tempopfad nicht ohne Gerätevergleich ersetzen. Die Begründung steht in MAINFRAME_V2, C.2 und C.3. |
| S13 | „Read README.md first …", Verweis auf Worker-README und `docs/v1-outline.md` (A1, A4) | zusammengeführt | Eine Zeile. |
| S14 | „MAINFRAME_V2.md holds decisions, code analysis and hardware limits." (A2) | gekürzt | Die Inhaltsangabe steht bereits im Kopfabsatz; die Regel selbst (nur den nötigen Abschnitt lesen, Code geht vor) bleibt. |
| S15 | „Only their example files are tracked." (A19); „The original project and its Git history are canonical." (A7); „Deployment is not part of the current work. … The Worker README describes the path for a later V2 deployment." (A21) | gekürzt | Zustandsbeschreibungen ohne eigene Handlungsanweisung. Die Verbote daneben bleiben. |
| S16 | „This is the only rules file; there is no separate AGENTS.md." | gestrichen | Verlaufsnotiz; steht im Nachtrag oben. |
| S17 | Zwischenfassung im Kopfabsatz: „Add a rule here only when the user asks for it or confirms it." | ersetzt | Auf Wunsch des Nutzers durch das Verfahren „Regel-Kandidaten" ersetzt (N3). |
| N3 | Neu, Abschnitt „Rule candidates": Regeln nicht von sich aus ergänzen, sondern dem Skill `rule-candidates` folgen; die Kandidaten stehen in `ideas.md`. | neu, vom Nutzer vorgegeben | Nutzer: „statt regeln regel kandidaten anlegen in einem eigenen abschnitt in ideas.md … aufgenommen, wenn sie mehr mals vorkommen … am ende jedes features gecheckt und mit dem user besprochen". Der Ablauf (eine Zeile je Kandidat mit Datum, weiteres Datum bei Wiederholung, Besprechung nach Abschluss des Features) steht nur im persönlichen Skill `rule-candidates`, den der Nutzer auf allen Rechnern hat. `CLAUDE.md` und `ideas.md` verweisen darauf und wiederholen ihn nicht; frühere Fassungen taten das und wurden vom Nutzer verworfen. |
