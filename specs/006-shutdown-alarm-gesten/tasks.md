---

description: "Task list for 006-shutdown-alarm-gesten"
---

# Tasks: Entkopplung von Ausschalten und Alarm

**Input**: Design documents from `/specs/006-shutdown-alarm-gesten/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/button-gestures.md, quickstart.md

**Tests**: Es gibt keine Unit-Test-Infrastruktur im Repo. Die Validierung erfolgt
als Hardware-in-the-Loop-Abnahme nach `quickstart.md` (Constitution IV) — die
HIL-Tasks stehen am Ende jeder Story-Phase.

**Organization**: Nach User Story gruppiert, damit jede Story einzeln geflasht
und am Gerät abgenommen werden kann.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: parallel möglich (andere Datei, keine offenen Abhängigkeiten)
- **[Story]**: US1 = Alarm (P1), US2 = Ausschalten (P2), US3 = 60-min-Abschaltung (P3)
- Alle Pfade sind repo-relativ; gebaut wird immer aus dem Repo-Root

## Path Conventions

Embedded-Firmware, flache Struktur: alle Quelldateien liegen direkt in
`Sender/`. `platformio.ini` liegt im Repo-Root; Build immer mit
`pio run -e sender` aus `/d/git/Bogenampel`. `Empfaenger/` und
`Schaltung-Sender/` werden in diesem Feature **nicht** angefasst.

---

## Phase 1: Setup

**Purpose**: Ausgangszustand absichern, damit spätere Fehler zuordenbar sind

- [X] T001 Baseline-Build prüfen: `pio run -e sender` aus dem Repo-Root (`platformio.ini`) muss fehlerfrei durchlaufen, bevor irgendetwas geändert wird
- [ ] T002 Baseline-Verhalten am Gerät festhalten: Sender flashen und die Ausgangslage aus `specs/006-shutdown-alarm-gesten/quickstart.md` notieren — heute löst OK ≥ 2 s im Schießbetrieb den Alarm aus und Ausschalten ist dort **nicht** möglich (Gegenprobe für T-04/T-33)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Konstanten und die Tastenabfrage während des e-Paper-Refreshs — ohne
diese Phase ist die Alarmfolge nicht zuverlässig erkennbar (research.md R-1)

**⚠️ CRITICAL**: Keine Story-Arbeit vor Abschluss dieser Phase

- [X] T003 In `Sender/Config.h` (namespace `Timing`, bei den Tastergesten ~Zeile 277) `MULTI_CLICK_COUNT = 3` und `MULTI_CLICK_GAP_MS = 400` als `constexpr` ergänzen, mit Kommentar-Verweis auf FR-007/A-002 — `ALARM_THRESHOLD_MS` bleibt in dieser Phase noch stehen
- [X] T004 In `Sender/Config.h` (bei den bestehenden `static_assert`s ab ~Zeile 404) die Reihenfolge absichern: `DEBOUNCE_MS < MULTI_CLICK_GAP_MS < POWER_OFF_HOLD_MS` (data-model.md §1)
- [X] T005 [P] In `Sender/EpaperDisplay.h` und `Sender/EpaperDisplay.cpp` einen Passthrough `void setBusyCallback(void (*cb)(const void*), const void* param = nullptr)` auf `display.epd2.setBusyCallback()` ergänzen, mit Doxygen-Kommentar zum Zweck (Tastenabfrage während des 300–400 ms blockierenden Refreshs, research.md R-1)
- [X] T006 In `Sender/Sender.cpp` eine statische Callback-Funktion anlegen, die `buttons.update()` aufruft, und sie in `setup()` über `epaper.setBusyCallback()` registrieren (nach der Display-Initialisierung, vor `stateMachine.begin()`); Kommentar mit Verweis auf Contract D-1…D-3 (abhängig von T005)
- [X] T007 In `Sender/ButtonManager.cpp` (`update()`, Loslass-Zweig ~Zeile 79) die Klick-Schwelle der Rolle OK von `Timing::ALARM_THRESHOLD_MS` auf `Timing::POWER_OFF_HOLD_MS` umstellen (Contract G-8, research.md R-4)
- [ ] T008 Build-Gate: `pio run -e sender`, flashen und am Gerät prüfen, dass das Display unverändert refresht und Klicks in allen Menüs weiterhin ankommen (Regression zu T002)

**Checkpoint**: Tastenabfrage läuft auch während des Refreshs — Story-Arbeit kann beginnen

---

## Phase 3: User Story 1 - Not-Alarm zuverlässig auslösen (Priority: P1) 🎯 MVP

**Goal**: Der Alarm wird durch einen Dreifachklick (≤ 400 ms Abstand, beide
Taster gleichwertig) ausgelöst — in „Schießbetrieb" und „Pfeile holen". Die alte
2-s-Halte-Geste entfällt.

**Independent Test**: Im Schießbetrieb und in „Pfeile holen" je dreimal schnell
klicken → Alarmbildschirm + Empfänger alarmiert; dieselbe Folge im
Konfigurationsmenü löst nichts aus; OK 2 s halten löst keinen Alarm mehr
(quickstart.md T-20…T-33).

### Implementation for User Story 1

- [X] T009 [US1] In `Sender/ButtonManager.h` die Struktur `ButtonState` um `clickCount` (uint8_t), `lastPressTime` (uint32_t), `multiClickFlag` (bool) und `multiClickArmed` (bool) erweitern (data-model.md §2) — *umgesetzt als `multiClickArmed` statt `suppressClickOnRelease`, siehe T014*
- [X] T010 [US1] In `Sender/ButtonManager.h` die Methoden `bool wasMultiClicked(Button btn)` und `void resetMultiClick()` mit Doxygen-Kommentaren deklarieren (contracts/button-gestures.md §1)
- [X] T011 [US1] Im Konstruktor von `Sender/ButtonManager.cpp` die vier neuen Felder mit 0/false initialisieren
- [X] T012 [US1] In `Sender/ButtonManager.cpp` (`update()`, Drück-Flanken-Zweig ~Zeile 59) das Zählwerk ergänzen: im Boot-Lockout nicht zählen; `clickCount++` wenn `now - lastPressTime <= MULTI_CLICK_GAP_MS`, sonst `clickCount = 1`; `lastPressTime = now` (data-model.md §2, Contract G-2/G-6)
- [X] T013 [US1] In `Sender/ButtonManager.cpp` (`update()`, Loslass-Zweig) den Folgenabbruch beim Halten ergänzen: dauerte die Betätigung ≥ `MULTI_CLICK_GAP_MS`, `clickCount = 0` setzen (Contract G-5, FR-011)
- [X] T014 [US1] In `Sender/ButtonManager.cpp` (`update()`, Drück-Flanke) nur **scharfstellen**: bei `clickCount >= MULTI_CLICK_COUNT` → `multiClickArmed = true` und den CONFIG-Klick zurückhalten. **Abweichung vom ursprünglichen Entwurf**: das Auslösen auf der Drückflanke hätte den Edge Case „nach zwei Klicks halten = nur ausschalten" (FR-011) unmöglich gemacht — begründet in research.md R-2, Nachtrag
- [X] T015 [US1] In `Sender/ButtonManager.cpp` (`update()`, Loslass-Zweig) die Folge abschließen: kurz gehalten und `multiClickArmed` → `multiClickFlag = true`, kein Klick; sonst Folge verwerfen und den zurückgehaltenen CONFIG-Klick nachliefern (Contract G-4/G-4a)
- [X] T016 [US1] In `Sender/ButtonManager.cpp` `wasMultiClicked()` (one-shot, Flag beim Lesen löschen) und `resetMultiClick()` (verwirft `clickCount`, `lastPressTime` und `multiClickFlag` **beider** Taster) implementieren (Contract G-1)
- [X] T017 [US1] In `Sender/StateMachine.h` die private Hilfsfunktion `static bool isAlarmCapable(State s)` deklarieren und den Klassen-Kopfkommentar (Zeile 10: „OK ≥2s = Alarm") auf die neue Gestenlage umschreiben
- [X] T018 [US1] In `Sender/StateMachine.cpp` `isAlarmCapable()` implementieren (`STATE_SCHIESS_BETRIEB`, `STATE_PFEILE_HOLEN`) und in `setState()` (~Zeile 63) `buttons.resetMultiClick()` aufrufen, außer wenn alter **und** neuer Zustand alarmfähig sind (research.md R-3, FR-011 + FR-012)
- [X] T019 [US1] In `Sender/StateMachine.cpp` `handleSchiessBetrieb()` (~Zeile 383) die Geste `wasHeldFor(Button::OK, ALARM_THRESHOLD_MS)` durch `wasMultiClicked(Button::OK) || wasMultiClicked(Button::CONFIG)` ersetzen; `setState(STATE_ALARM)` + sofortiges `return` beibehalten (Contract P-2/P-5)
- [X] T020 [US1] In `Sender/StateMachine.cpp` `handlePfeileHolen()` (~Zeile 291) dieselbe Alarmprüfung **vor** `pfeileHolenMenu.update()` einfügen, mit `return` nach dem Zustandswechsel (FR-010)
- [X] T021 [US1] In `Sender/Config.h` `ALARM_THRESHOLD_MS` ersatzlos entfernen und den Rollen-Kommentar (~Zeile 59: „Alarm 2s") auf „Alarm = 3× Klick" korrigieren (FR-016)
- [X] T022 [P] [US1] Kommentare in `Sender/SchiessBetriebMenu.h` (Zeile 10) und `Sender/SchiessBetriebMenu.cpp` (Zeile 30) auf die neue Alarm-Geste umschreiben
- [ ] T023 [US1] `pio run -e sender` bauen, flashen und HIL-Testfälle T-20 bis T-33 aus `specs/006-shutdown-alarm-gesten/quickstart.md` abarbeiten — **T-23 (10 von 10 Auslösungen im laufenden Countdown) ist das Gate für research.md R-1**; bei Durchfallen auf die dort dokumentierte GPIO-Interrupt-Variante wechseln

**Checkpoint**: Alarm läuft über den Dreifachklick, die 2-s-Geste ist weg. Halten
tut im Schießbetrieb noch nichts — das liefert US2.

---

## Phase 4: User Story 2 - Gerät jederzeit vorhersagbar ausschalten (Priority: P2)

**Goal**: Halten einer beliebigen Taste ≥ 3 s schaltet aus jedem Zustand ab,
auch aus laufendem Schießbetrieb und Alarm.

**Independent Test**: In allen fünf Zuständen nacheinander jede Taste 3 s halten
→ Gerät schaltet jedes Mal ab (quickstart.md T-01…T-09).

### Implementation for User Story 2

- [X] T024 [US2] In `Sender/StateMachine.cpp` `update()` (~Zeile 53) `checkPowerOffGesture()` **einmal zentral** vor dem `switch` aufrufen (Contract P-1, data-model.md §5)
- [X] T025 [US2] In `Sender/StateMachine.cpp` die Einzelaufrufe von `checkPowerOffGesture()` in `handleSplash()` (~Zeile 169), `handleConfigMenu()` (~Zeile 220), `handlePfeileHolen()` (~Zeile 292) und `handleAlarm()` (~Zeile 546) entfernen (abhängig von T024)
- [X] T026 [US2] In `Sender/StateMachine.cpp` den Kommentar in `checkPowerOffGesture()` (~Zeile 100–105) neu fassen: gilt jetzt in **allen** Zuständen, die Ausnahme „nicht im Schießbetrieb (dort Alarm)" entfällt; Verweis auf FR-002 und research.md R-6 (autonomes Passenende deckt das Abschalten im Schießbetrieb ab)
- [X] T027 [P] [US2] In `Sender/StateMachine.h` die Doxygen-Kommentare zu `checkPowerOffGesture()` (~Zeile 171–180) auf die neue Gültigkeit und den falschen FR-Verweis („FR-015") korrigieren
- [ ] T028 [US2] `pio run -e sender` bauen, flashen und HIL-Testfälle T-01 bis T-09 aus `specs/006-shutdown-alarm-gesten/quickstart.md` abarbeiten — T-04 (Ausschalten im laufenden Countdown, Empfänger führt die Passe autonom zu Ende) und T-07 (kein Selbstabschalten beim Einschalten) sind die kritischen

**Checkpoint**: Beide Gesten sind entkoppelt und in allen Zuständen konsistent

---

## Phase 5: User Story 3 - Längere Pausen ohne Selbstabschaltung (Priority: P3)

**Goal**: Die Inaktivitäts-Abschaltung greift erst nach 60 statt 20 Minuten.

**Independent Test**: Gerät unbedient stehen lassen — nach 59 min noch an, nach
60 min aus (mit `DEBUG_SHORT_TIMES` in Sekunden, quickstart.md T-40…T-44).

### Implementation for User Story 3

- [X] T029 [US3] In `Sender/Config.h` `IDLE_POWER_OFF_MS` im Nicht-DEBUG-Zweig (~Zeile 294) von `20UL * 60 * 1000` auf `60UL * 60 * 1000` ändern; DEBUG-Wert (60 s) unverändert lassen und den Begründungskommentar um den Turnier-Anwendungsfall ergänzen (FR-019, research.md R-5)
- [X] T030 [US3] In `Sender/StateMachine.cpp` verifizieren, dass `checkIdleTimeout()` (~Zeile 112) und seine Aufrufstellen in `handleConfigMenu()` und `handlePfeileHolen()` unverändert bleiben — die Abschaltung darf weiterhin nur in den Wartezuständen greifen, nicht im Schießbetrieb und nicht im Alarm (FR-021)
- [ ] T031 [US3] `pio run -e sender` mit `DEBUG_SHORT_TIMES` bauen, flashen und HIL-Testfälle T-40 bis T-44 aus `specs/006-shutdown-alarm-gesten/quickstart.md` abarbeiten

**Checkpoint**: Alle drei Stories sind einzeln abgenommen

---

## Phase 6: Polish & Cross-Cutting Concerns

- [X] T032 [P] Kopfkommentar von `Sender/ButtonManager.h` (Zeilen 1–26) neu schreiben: Gesten-Übersicht auf „Klick / 3× Klick = Alarm / ≥ 3 s = Aus", Rolle OK ohne 2-s-Schwelle, Hinweis auf den Busy-Callback-Aufrufpfad
- [X] T033 [P] `README.md` aktualisieren: Tastentabelle (Zeilen 41–46) auf die neuen Gesten, Spalte „2 s halten" ersetzen durch „3× schnell klicken"; Abschnitt „Automatische Abschaltung" (Zeile 50) von 20 auf 60 Minuten (FR-023)
- [X] T034 [P] `CLAUDE.md` finalisieren: den Planungsvermerk unter „Recent Changes" auf den umgesetzten Stand umschreiben und den Hinweis „Auto-Abschaltung nach 20 min Inaktivität" im Stromspar-Absatz auf 60 min korrigieren
- [X] T035 [P] In `specs/004-v3-esp32-port/data-model.md` §3 (Zeilen 77–81) einen Hinweis ergänzen, dass die Gesten-Tabelle durch `specs/006-shutdown-alarm-gesten/data-model.md` §3 abgelöst ist
- [ ] T036 Regressionstests T-60 bis T-63 aus `specs/006-shutdown-alarm-gesten/quickstart.md` abarbeiten (kompletter Turnierdurchlauf, Gruppenzyklus, Wartungsmodus/OTA, 30 min Bedienung ohne Fehlalarm)
- [ ] T037 Stromaufnahme im Konfigurationsmenü nachmessen (T-64): muss bei ≤ 0,20 W bleiben — Constitution fordert Neumessung bei Verbrauchsänderungen; Ergebnis in `specs/006-shutdown-alarm-gesten/quickstart.md` eintragen
- [ ] T038 `pio run -e sender-release` bauen und flashen, danach T-20 und T-01 im Feld-Build gegenprüfen (kein Debug-Output, kein CDC)
- [ ] T039 Safety-Review über den Diff von `Sender/ButtonManager.cpp` und `Sender/StateMachine.cpp` durchführen (Constitution „Review-Anforderungen": sicherheitskritische Änderung erfordert zusätzliches Review) — Fokus auf Determinismus der Gestenerkennung und das Abschalten im laufenden Schießbetrieb
- [ ] T040 Checkliste `specs/006-shutdown-alarm-gesten/quickstart.md` vollständig auf ✅ setzen und offene Punkte dokumentieren

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: keine Abhängigkeiten
- **Foundational (Phase 2)**: nach Setup — **blockiert alle Stories** (ohne den Busy-Callback ist die Alarmfolge nicht zuverlässig messbar)
- **US1 (Phase 3)**: nach Phase 2 — MVP
- **US2 (Phase 4)**: nach Phase 2 technisch möglich, **fachlich aber nach US1**: solange die alte 2-s-Alarm-Geste lebt, würden im Schießbetrieb beim Durchhalten erst der Alarm (2 s) und dann das Abschalten (3 s) feuern
- **US3 (Phase 5)**: nach Phase 2, sonst unabhängig — berührt nur `IDLE_POWER_OFF_MS`
- **Polish (Phase 6)**: nach allen gewünschten Stories

### Within User Story 1

Strikt sequenziell in `Sender/ButtonManager.cpp` (T011 → T016, eine Datei) und
danach `Sender/StateMachine.cpp` (T018 → T020). T021 (Konstante löschen) erst
**nach** T019, sonst bricht der Build. T022 ist reine Kommentararbeit und
parallel möglich.

### Parallel Opportunities

- Phase 2: T005 (`EpaperDisplay.*`) parallel zu T003/T004 (`Config.h`); T006 wartet auf T005
- Phase 3: T022 parallel zu allem anderen
- Phase 4: T027 (`StateMachine.h`) parallel zu T024–T026 (`StateMachine.cpp`)
- Phase 6: T032–T035 sind vier verschiedene Dateien und laufen komplett parallel
- **Nicht parallelisierbar**: alles innerhalb von `Sender/ButtonManager.cpp`, `Sender/StateMachine.cpp` und `Sender/Config.h` — jeweils eine Datei, mehrere Tasks

## Parallel Example: Phase 6

```bash
# Vier Dokumentations-Tasks gleichzeitig (verschiedene Dateien):
Task: "Kopfkommentar Sender/ButtonManager.h neu schreiben"
Task: "README.md Tastentabelle und Abschaltzeit aktualisieren"
Task: "CLAUDE.md Recent Changes finalisieren"
Task: "specs/004-v3-esp32-port/data-model.md §3 als abgelöst markieren"
```

## Implementation Strategy

### MVP First (User Story 1)

1. Phase 1 (Setup) → Phase 2 (Foundational)
2. Phase 3 (US1) vollständig
3. **STOPP und validieren**: T-20…T-33, insbesondere T-23
4. Ab hier ist die sicherheitskritische Funktion bereits besser als vorher: Alarm ist auch in „Pfeile holen" erreichbar und nicht mehr mit dem Ausschalten verwechselbar

### Incremental Delivery

1. Setup + Foundational → Tastenabfrage refreshfest
2. US1 → Alarm entkoppelt → flashen, abnehmen (MVP)
3. US2 → Ausschalten überall → flashen, abnehmen
4. US3 → 60-Minuten-Frist → flashen, abnehmen
5. Polish → Doku, Regression, Strommessung, Safety-Review

### Ein Entwickler, ein Gerät

Parallelarbeit lohnt hier kaum: Es gibt genau einen Sender zum Flashen, und die
drei Kern-Dateien werden von fast allen Tasks angefasst. Die [P]-Marker sind
dokumentativ (verschiedene Dateien) und vor allem für die Doku-Phase relevant.

## Notes

- Gebaut wird **immer** aus dem Repo-Root; unter Windows vor jedem Upload `$env:PYTHONIOENCODING="utf-8"` setzen
- Sender-USB-Flash: BTN1 während des gesamten Uploads gedrückt halten
- `Empfaenger/` wird nicht angefasst und **nicht** neu geflasht
- Keine Pin-, Schaltplan- oder Protokolländerung — `Schaltung-Sender/` bleibt unberührt (research.md R-7)
- Nach jedem Task oder logischen Block committen
