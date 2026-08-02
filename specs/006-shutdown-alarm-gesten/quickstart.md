# Quickstart & HIL-Abnahme: Entkopplung von Ausschalten und Alarm

**Feature**: 006-shutdown-alarm-gesten
**Datum**: 2026-08-02

Es gibt im Repo keine Unit-Test-Infrastruktur; die Abnahme erfolgt wie bei 004
und 005 als Hardware-in-the-Loop-Test (Constitution IV). Der Empfänger wird
**nicht** neu geflasht — seine Firmware ist unverändert.

## Vorbereitung

```bash
cd /d/git/Bogenampel            # Repo-Root, platformio.ini liegt HIER
pio run -e sender               # bauen
```

Flashen (Sender, USB): **BTN1 während des gesamten Uploads gedrückt halten.**
Unter Windows vorher `$env:PYTHONIOENCODING="utf-8"` setzen.

```bash
pio run -t upload -e sender
pio device monitor -e sender    # 115200 Baud
```

Für die Zeitmessungen (T-10) einen Build mit `DEBUG_SHORT_TIMES` verwenden —
dort schaltet das Gerät nach 60 s statt 60 min ab.

Empfänger währenddessen eingeschaltet lassen (für T-04/T-05/T-06 nötig).

## Testfälle

Legende: ✅ = bestanden, ❌ = durchgefallen, ⬜ = offen

### Ausschalten (User Story 2)

| # | Vorbedingung | Aktion | Erwartung | FR | Status |
|---|---|---|---|---|---|
| T-01 | Konfigurationsmenü | OK 3 s halten | „Auf Wiedersehen!", Gerät aus | FR-001 | ⬜ |
| T-02 | Konfigurationsmenü | CONFIG 3 s halten | Gerät aus | FR-003 | ⬜ |
| T-03 | Pfeile holen | beide Taster einzeln 3 s halten | jeweils aus | FR-002 | ⬜ |
| T-04 | **Schießbetrieb, Countdown läuft** | OK 3 s halten | Gerät aus, **kein Alarm**; Empfänger führt die Passe autonom zu Ende | FR-002, R-6 | ⬜ |
| T-05 | Schießbetrieb | CONFIG 3 s halten | Gerät aus | FR-002 | ⬜ |
| T-06 | Alarm steht | OK 3 s halten | Gerät aus | FR-002 | ⬜ |
| T-07 | aus | Einschalt-Taster beim Einschalten 5 s halten | Gerät bleibt an, kein sofortiges Abschalten | FR-004 | ⬜ |
| T-08 | Wartungsmodus (beide beim Einschalten) | einen Taster 3 s halten | Gerät aus (Verhalten unverändert) | A-004 | ⬜ |
| T-09 | beliebiger Zustand | Taste 2 s halten, loslassen | kein Abschalten; der Druck wirkt als normaler Klick | FR-001, R-4 | ⬜ |

### Alarm (User Story 1)

| # | Vorbedingung | Aktion | Erwartung | FR | Status |
|---|---|---|---|---|---|
| T-20 | **Schießbetrieb, Countdown läuft** | OK 3× schnell klicken | Alarmbildschirm, Empfänger alarmiert | FR-007 | ✅ |
| T-21 | Schießbetrieb | CONFIG 3× schnell klicken | **kein** Alarm (CONFIG hat keine Alarmfolge) | FR-009 | ✅ |
| T-22 | Pfeile holen | OK 3× schnell klicken | Alarm | FR-010 | ⬜ |
| T-23 | **Schießbetrieb** | 3× klicken, **10× wiederholt** | 10 von 10 Auslösungen (Nachweis, dass der e-Paper-Refresh keine Klicks frisst — R-1) | SC-001a | ⬜ |
| T-24 | Schießbetrieb | schnell OK/CONFIG/OK/CONFIG klicken (nur 2× OK) | **kein** Alarm | FR-008 | ⬜ |
| T-24a | Schießbetrieb | OK/CONFIG/OK/OK schnell klicken (3× OK, CONFIG dazwischen) | Alarm — CONFIG bricht die Folge nicht ab | FR-008 | ⬜ |
| T-25 | Schießbetrieb | 3× klicken mit ~1 s Abstand | kein Alarm, jeder Klick wirkt normal (nach dem 1. Klick: Pfeile holen) | FR-011 | ⬜ |
| T-26 | Konfigurationsmenü | OK/CONFIG 3× schnell klicken | **kein** Alarm, Werte zählen normal | FR-010, SC-004 | ✅ |
| T-27 | Splash-Screen | 3× schnell klicken | kein Alarm (Splash wird übersprungen) | FR-010 | ⬜ |
| T-28 | **Alarm steht** | 3× schnell klicken | Alarm bleibt stehen, **kein** Zustandssprung, kein zweiter Funkbefehl | FR-015 | ⬜ |
| T-29 | Alarm steht | einmal kurz OK klicken | Alarm quittiert → Pfeile holen | FR-017 | ⬜ |
| T-30 | Schießbetrieb | 3× schnell klicken (erster Klick beendet die Passe) | Endzustand ist **Alarm**, nicht „Pfeile holen" | FR-012, FR-013 | ⬜ |
| T-31 | Schießbetrieb | 2× schnell klicken, dann 3. Mal halten | Gerät schaltet nach 3 s ab, **kein** Alarm | FR-011 | ⬜ |
| T-32 | Konfigurationsmenü | 2× schnell klicken, dann sofort in den Schießbetrieb wechseln und 1× klicken | **kein** Alarm (Zähler wurde beim Zustandswechsel verworfen) | FR-011 | ⬜ |
| T-33 | Schießbetrieb | OK **2 s halten** | **kein** Alarm (alte Geste entfernt) | FR-016 | ⬜ |

### Automatische Abschaltung (User Story 3)

| # | Vorbedingung | Aktion | Erwartung | FR | Status |
|---|---|---|---|---|---|
| T-40 | Konfigurationsmenü, unbedient | 59 min warten (DEBUG: 59 s) | Gerät noch an und bedienbar | FR-019 | ⬜ |
| T-41 | Konfigurationsmenü, unbedient | 60 min warten (DEBUG: 60 s) | „Automatische Abschaltung", Gerät aus | FR-019, FR-022 | ⬜ |
| T-42 | 45 min unbedient (DEBUG: 45 s) | eine Taste kurz drücken | Frist startet neu, kein Abschalten zum ursprünglichen Zeitpunkt | FR-020 | ⬜ |
| T-43 | Schießbetrieb, Countdown läuft länger als die Frist | warten | **kein** automatisches Abschalten | FR-021 | ⬜ |
| T-44 | Alarm steht, unbedient | Frist abwarten | **kein** automatisches Abschalten | FR-021 | ⬜ |

### Regression (unveränderte Funktionen)

| # | Aktion | Erwartung | Status |
|---|---|---|---|
| T-60 | Kompletter Turnierdurchlauf: Menü → Schießbetrieb → Passe → Pfeile holen → nächste Passe | unverändert wie vor dem Feature | ⬜ |
| T-61 | Gruppenwechsel A/B ↔ C/D über den 4er-Zyklus | unverändert | ⬜ |
| T-62 | Wartungsmodus: beide Taster beim Einschalten, OTA-Upload | funktioniert unverändert | ⬜ |
| T-63 | 30 min normale Bedienung im üblichen Tempo | kein einziger Fehlalarm (SC-004) | ⬜ |
| T-64 | Stromaufnahme im Konfigurationsmenü messen | ≤ 0,20 W wie bisher (SC-007, Constitution: „Stromverbrauchsänderungen MÜSSEN neu gemessen werden") | ⬜ |

## Abnahmekriterium

Alle Testfälle ✅.

**Wenn T-23 durchfällt** (Klicks werden verschluckt), in dieser Reihenfolge
prüfen — die drei möglichen Ursachen stehen in `research.md` R-1:

1. **Klickabstand zu eng**: `Timing::MULTI_CLICK_GAP_MS` von 400 auf 500–600 ms
   anheben. Danach T-26 und T-63 wiederholen (Fehlalarm-Gegenprobe).
2. **Entprellung als Untergrenze**: Betätigungen unter `Timing::DEBOUNCE_MS`
   (80 ms) werden gar nicht erkannt. Erst anfassen, wenn Punkt 1 nichts bringt —
   ein niedrigerer Wert erhöht das Prellrisiko überall sonst.
3. **Refresh-Blockade doch nicht abgedeckt**: dann auf die in R-1 dokumentierte
   GPIO-Interrupt-Erfassung der Drückflanken umstellen.

Beobachtung protokollieren: Bei welchem Testdurchlauf ging der wievielte Klick
verloren? Das trennt Ursache 1/2 (einzelne Klicks fehlen sporadisch) von
Ursache 3 (Ausfälle häufen sich rund um den Sekundentakt des Countdowns).
