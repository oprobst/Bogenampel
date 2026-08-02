# Implementation Plan: Entkopplung von Ausschalten und Alarm

**Branch**: `006-shutdown-alarm-gesten` | **Date**: 2026-08-02 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/006-shutdown-alarm-gesten/spec.md`

## Summary

Ausschalten und Alarm liegen heute beide auf „Taste halten" und werden nur über
den Betriebszustand auseinandergehalten. Künftig gilt: **Halten ≥ 3 s = Aus (in
jedem Zustand)**, **Dreifachklick ≤ 400 ms Abstand = Alarm (nur im Schießbetrieb
und in „Pfeile holen")**, und die Inaktivitäts-Abschaltung greift erst nach
60 statt 20 Minuten.

Technisch ist das eine reine Sender-Firmware-Änderung in drei Klassen
(`ButtonManager`, `StateMachine`, `Config.h`) plus einer kleinen Ergänzung im
Display-Layer. Der einzige nicht triviale Punkt: Der e-Paper-Refresh blockiert
300–400 ms und im Schießbetrieb läuft er im Sekundentakt — ohne Gegenmaßnahme
gingen die schnellen Klicks der Alarmfolge dort verloren. Gelöst über den
GxEPD2-Busy-Callback, der `ButtonManager::update()` auch während des Refreshs
weiterlaufen lässt (Details und Fallback: `research.md` R-1).

Keine Hardware-, Protokoll- oder Empfänger-Änderung.

## Technical Context

**Language/Version**: C++17, Arduino core for ESP32 (arduino-esp32 3.3.7)
**Primary Dependencies**: GxEPD2 (Busy-Callback, neu genutzt), Adafruit GFX; ESP-NOW unverändert
**Storage**: keine — kein NVS-Zugriff, keine neuen Einstellungen
**Testing**: Hardware-in-the-Loop nach `quickstart.md` (T-01…T-64); keine Unit-Test-Infrastruktur im Repo
**Target Platform**: Sender ESP32-S3-WROOM-1U-N16R8; Empfänger XIAO ESP32C3 **unverändert**
**Project Type**: Embedded-Firmware, flache Dateistruktur, zentrale `platformio.ini` im Repo-Root
**Performance Goals**: Alarmfolge < 1,5 s ab erstem Klick (SC-001), 9/10 Trefferquote (SC-001a), Erkennung zuverlässig trotz 300–400 ms Refresh-Blockade
**Constraints**: Betriebsstrom bleibt bei 0,20 W (SC-007); Zeitkonstanten `DEBOUNCE_MS (80) < MULTI_CLICK_GAP_MS (400) < POWER_OFF_HOLD_MS (3000)`; kein Pin-, Schaltplan- oder Protokoll-Eingriff
**Scale/Scope**: 5 Zustände, 2 Taster, ~6 Quelldateien im Ordner `Sender/`, geschätzt 120–150 geänderte Zeilen

## Constitution Check

*GATE: bestanden vor Phase 0 — und nach Phase 1 erneut geprüft (Ergebnis unten identisch).*

| Prinzip | Bewertung | Begründung |
|---|---|---|
| **I. Sicherheit Zuerst** (NON-NEGOTIABLE) | ✅ PASS | Der Alarm wird **zugänglicher**, nicht eingeschränkt: bisher nur im Schießbetrieb, künftig auch in „Pfeile holen". Zustandsübergänge bleiben deterministisch (feste Reihenfolge Aus → Alarm → Klick, `data-model.md` §5). Das neue Ausschalten im Schießbetrieb ist durch das autonome Passenende des Empfängers abgesichert (R-6). Funkvalidierung unverändert. **Auflage**: R-1 muss umgesetzt sein, sonst wäre die Alarmerkennung nicht deterministisch — Abnahme über T-23. |
| **II. Einfachheit & Zuverlässigkeit** | ✅ PASS | Zwei klar getrennte Gesten statt einer zustandsabhängigen Doppelbelegung — das ist netto weniger Bedienkomplexität. Vier neue Felder pro Taster, zwei neue Methoden, keine neue Bibliothek. Kein Konfigurationsmenü für die Schwellen (bewusst out of scope). |
| **III. Embedded-Hardware-Standards** | ✅ PASS | Keine Pin-Änderung (R-7), KiCad unangetastet. Neue Zeitwerte als `constexpr` in `Config.h` mit `static_assert` auf die Reihenfolge. Baubar mit `pio run -e sender`. |
| **IV. Testbarkeit & Validierung** | ✅ PASS | 33 HIL-Testfälle in `quickstart.md`, jeder auf eine FR/SC abgebildet. Grenzfälle (Halten statt Klicken, Zustandswechsel mitten in der Folge, Boot-Lockout) explizit abgedeckt. Stromverbrauch wird nachgemessen (T-64). |
| **V. Wartbarkeit & Dokumentation** | ✅ PASS | Gesten-Tabelle und Zustandsübergänge in `data-model.md` §3/§4 aktualisiert (ersetzt 004 §3) — erfüllt „Änderungen an Zustandsmaschine MÜSSEN State-Diagramm-Updates beinhalten". Header-Doku von `ButtonManager.h`, `CLAUDE.md` und `README.md` werden nachgezogen (FR-023). |

**Safety-kritische Änderung** → laut Constitution „Review-Anforderungen"
zusätzliches Review erforderlich, bevor das Feature als abgenommen gilt.

Keine Verstöße → **Complexity Tracking entfällt.**

## Project Structure

### Documentation (this feature)

```text
specs/006-shutdown-alarm-gesten/
├── plan.md                        # diese Datei
├── spec.md                        # Feature-Spezifikation (freigegeben)
├── research.md                    # Phase 0: R-1…R-8
├── data-model.md                  # Phase 1: Konstanten, Tasterzustand, Gesten-Tabelle
├── quickstart.md                  # Phase 1: HIL-Abnahme T-01…T-64
├── contracts/
│   └── button-gestures.md         # Phase 1: Vertrag ButtonManager ↔ StateMachine
├── checklists/
│   └── requirements.md            # Spec-Qualität (vollständig grün)
└── tasks.md                       # Phase 2 — NICHT von /speckit.plan erzeugt
```

### Source Code (repository root)

```text
platformio.ini                     # unverändert (env: sender / sender-release)

Sender/                            # NUR hier wird geändert
├── Config.h                       # ALARM_THRESHOLD_MS raus; MULTI_CLICK_* rein;
│                                  #   IDLE_POWER_OFF_MS 20 → 60 min; static_asserts
├── ButtonManager.h                # neu: wasMultiClicked(), resetMultiClick(); Doku
├── ButtonManager.cpp              # Zählwerk, Klick-Unterdrückung, Schwelle → 3 s
├── StateMachine.h                 # neu: isAlarmCapable(); Doku der Gesten
├── StateMachine.cpp               # Power-Off zentral; Alarm über Mehrfachklick;
│                                  #   resetMultiClick() in setState()
├── EpaperDisplay.h/.cpp           # Passthrough setBusyCallback() (R-1)
├── Sender.cpp                     # Busy-Callback registrieren; loopOta unverändert
└── SchiessBetriebMenu.h           # nur Kommentar (alte 2-s-Geste erwähnt)

Empfaenger/                        # NICHT angefasst, nicht neu geflasht
Schaltung-Sender/                  # NICHT angefasst (keine Hardware-Änderung)

CLAUDE.md, README.md               # Tastenbelegung nachziehen (FR-023)
specs/004-v3-esp32-port/data-model.md   # Hinweis: §3 durch 006 §3 ersetzt
```

**Structure Decision**: Bestehende flache Struktur unter `Sender/` beibehalten
(Projektkonvention, `platformio.ini` bleibt zentral im Repo-Root). Es entstehen
keine neuen Quelldateien — die Gestenerkennung gehört fachlich in den
`ButtonManager`, der sie bereits für Klick und Halten kapselt.

## Phase 0 — Research (abgeschlossen)

Ergebnisse in [`research.md`](./research.md):

| # | Thema | Entscheidung |
|---|---|---|
| R-1 | Klickverlust während des e-Paper-Refreshs | `GxEPD2_EPD::setBusyCallback()` → `buttons.update()` läuft im Refresh weiter; Fallback GPIO-Interrupts |
| R-2 | Auslösender Klick wirkt doppelt (quittiert den Alarm sofort) | `ButtonManager` unterdrückt Klick-Flag + kommende Loslass-Flanke |
| R-3 | Zähler über Zustandswechsel | `resetMultiClick()` in `setState()`, außer zwischen zwei alarmfähigen Zuständen |
| R-4 | Klickschwelle beim Loslassen von OK | `ALARM_THRESHOLD_MS` → `POWER_OFF_HOLD_MS` |
| R-5 | 60-min-Frist vs. Akku | +36 mAh im Nachlauf, unkritisch; DEBUG-Wert bleibt 60 s |
| R-6 | Ausschalten im laufenden Schießbetrieb | zulässig, autonomes Passenende deckt es ab |
| R-7 | Hardware | keine Änderung, KiCad unberührt |
| R-8 | Empfänger | unverändert, nicht neu flashen |

## Phase 1 — Design & Contracts (abgeschlossen)

- [`data-model.md`](./data-model.md) — Konstanten, neue Felder im Tasterzustand,
  Zählregeln, **autoritative Gesten-Tabelle** (ersetzt 004 §3), geänderte
  Zustandsübergänge, Aufrufreihenfolge
- [`contracts/button-gestures.md`](./contracts/button-gestures.md) — Vertrag
  `ButtonManager` ↔ `StateMachine`: neue API, 10 Garantien (G-1…G-10), 5
  Aufrufer-Pflichten (P-1…P-5), Display-Vertrag (D-1…D-3)
- [`quickstart.md`](./quickstart.md) — HIL-Abnahme, 33 Testfälle mit FR-Bezug

Agent-Kontext (`CLAUDE.md`) wurde über `.specify/scripts/bash/update-agent-context.sh claude`
aktualisiert.

## Umsetzungsreihenfolge (Vorschau für `/speckit.tasks`)

1. **Konstanten** (`Config.h`) — Basis für alles Weitere, isoliert prüfbar per Build
2. **Busy-Callback** (`EpaperDisplay`, `Sender.cpp`) — muss **vor** der
   Alarmfolge stehen, sonst testet T-23 ins Leere
3. **`ButtonManager`** — Zählwerk, Klick-Unterdrückung, neue Schwelle
4. **`StateMachine`** — Power-Off zentral, Alarmfolge in zwei Handlern,
   `resetMultiClick()` in `setState()`
5. **Dokumentation** — Header-Doku, `CLAUDE.md`, `README.md`, Hinweis in 004
6. **HIL-Abnahme** nach `quickstart.md`, danach Strommessung (T-64)

Reihenfolge 3 → 4 ist zwingend (die StateMachine ruft die neue API auf);
Schritt 2 ist unabhängig von 3/4 und kann parallel laufen.

## Risiken

| Risiko | Auswirkung | Gegenmaßnahme |
|---|---|---|
| Busy-Callback reicht nicht, Klicks gehen weiter verloren | Alarm unzuverlässig — safety-kritisch | T-23 (10 Wiederholungen) als Gate; Fallback GPIO-Interrupts steht in R-1 bereit |
| 400 ms Klickabstand für ungeübte Bediener zu eng | Alarm wird nicht ausgelöst | T-23/SC-001a misst die Trefferquote; Stellschraube ist genau eine Konstante (`MULTI_CLICK_GAP_MS`) |
| Fehlalarm durch schnelles Blättern | Unnötiger Betriebsstopp | Folge gilt nur in zwei Zuständen; T-26 und T-63 prüfen genau das |
| `MENU_LOCKOUT_MS` (400 ms) schluckt Klick 2/3 nach dem Passenende | T-30 schlägt fehl | Zählung liegt im `ButtonManager` **vor** dem Menü-Lockout — bewusst so entworfen (`data-model.md` §3) |
