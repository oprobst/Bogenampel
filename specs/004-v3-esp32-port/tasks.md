# Tasks: Bogenampel V3 Hardware Port (ESP32 + e-Paper + ESP-NOW)

**Input**: Design documents from `/specs/004-v3-esp32-port/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Keine automatisierten Test-Tasks (nicht angefordert). Verifikation = PlatformIO-Builds
(SC-011) + Hardware-in-the-Loop-Checkliste aus `quickstart.md` (HIL #1–20). HIL-Punkte brauchen
die echte Hardware und damit Mitwirkung des Nutzers.

**Organization**: Tasks gruppiert nach User Story (US1–US5 aus spec.md), damit jede Story
unabhängig implementier- und testbar bleibt.

## Format: `[ID] [P?] [Story] Beschreibung mit Dateipfad`

- **[P]**: parallelisierbar (andere Dateien, keine offenen Abhängigkeiten)
- **[Story]**: US1–US5 (nur in den User-Story-Phasen)

---

## Phase 1: Setup (Projektgerüste + Hardware-Vorbedingungen)

**Purpose**: Beide Firmware-Ordner anlegen, Build-Toolchain verifizieren, Hardware-Reworks anstoßen

- [x] T001 Projektgerüst `SenderV3/` anlegen: `SenderV3/SenderV3.ino` (setup/loop-Minimal — **erste Anweisung: LATCH GPIO16 OUTPUT+HIGH**, R-13) und `SenderV3/platformio.ini` (`src_dir = .`, `board = esp32-s3-devkitc-1`, 16MB Flash, `memory_type = qio_opi`, `-DBOARD_HAS_PSRAM`, USB-CDC on Boot, `lib_deps`: zinggjm/GxEPD2 + adafruit/Adafruit GFX Library) gemäß R-14
- [x] T002 [P] Projektgerüst `EmpfaengerV3/` anlegen: `EmpfaengerV3/EmpfaengerV3.ino` (setup/loop-Minimal) und `EmpfaengerV3/platformio.ini` (`src_dir = .`, `board = seeed_xiao_esp32c3`, `lib_deps`: fastled/FastLED) gemäß R-14
- [x] T003 **Schaltplan-Korrekturen — komplett erledigt, per Netzlisten-Export verifiziert (2026-06-10)**: Empfänger (Helligkeits-Poti D1, Piezo D3, Lüfter-Poti D2/J8, POTI_GND D4, Status-LED aktiv LOW, Tacho unbeschaltet, J1-GND-Fehler korrigiert) und Sender (R2=47k, R7=240k). T007/T008 sind freigegeben („Schaltplan zuerst, dann Code", Constitution V)
- [ ] T003b [P] ⚠ **Nutzer-Aktion (physisches Rework)**: restliche Hardware-Umsetzung — Lochraster: Helligkeits-Poti auf D1 (falls noch offen); Sender: R7 mit 240k bestücken. Bereits erledigt (2026-06-10): Piezo → D3, Lüfter-Poti → D2, Status-LED aktiv LOW, Tacho ab, Lautstärke-Poti-Fußpunkt an D4. Blockiert nur die HIL-Tests (T021, T025, T028, T031, T033, T036), nicht die Implementierung
- [x] T004 Build-Verifikation der Gerüste: `pio run` in `SenderV3/` und `EmpfaengerV3/` erfolgreich (SC-011-Basis)

---

## Phase 2: Foundational (blockiert alle User Stories)

**Purpose**: Protokoll, Pin-/Konstanten-Konfiguration und ESP-NOW-Transport — von jeder Story benötigt

**⚠️ CRITICAL**: Erst abschließen, dann User-Story-Phasen beginnen

- [x] T005 [P] `SenderV3/Commands.h` erstellen: `RadioCommand` (11 Codes), `FrameType`, `RadioPacketV3` (6 Bytes packed), `calculateChecksum`/`validateChecksum`, `commandToString`, `TransmissionResult` — exakt nach `contracts/espnow-protocol.md`
- [x] T006 [P] `EmpfaengerV3/Commands.h` erstellen: byte-identische Kopie von T005 (Konvention aus plan.md, Prüfung in T034)
- [x] T007 [P] `SenderV3/Config.h` erstellen: Namespaces `Pins` (alle GPIOs aus `contracts/hardware-pins.md` Sender-Tabelle inkl. Polaritäts-Kommentaren und Connector-Referenzen), `Display` (200×200, Refresh-Fenster aus data-model.md §7), `Radio` (Kanal 1, Retry 3×50 ms), `Battery` (Teiler ×2,5, Schwellen 3000/3300/4200 mV, Median-5), `Timing` (V2-Werte: PREPARATION 10 s, ALARM_THRESHOLD 2 s, POWER_OFF_HOLD 3 s, DEBOUNCE 80 ms, MENU_LOCKOUT 400 ms), `NVS` (Namespace `bogenampel`, Keys `timeS`/`count`), `System` (DEBUG-Makros wie V2)
- [x] T008 [P] `EmpfaengerV3/Config.h` erstellen: Namespaces `Pins` (XIAO-Tabelle aus `contracts/hardware-pins.md`: Helligkeits-Poti = D1/GPIO3, Piezo = D3/GPIO5, Lüfter-Poti = D2/GPIO4, POTI_GND = D4/GPIO6, Status-LED aktiv LOW!), `LEDStrip` (unverändert aus `Empfaenger/Config.h`: 158 LEDs, Segment-Layout, Brightness 64–255), `Radio`, `Timing` (Buzzer 2700 Hz etc.), `System`
- [x] T009 `SenderV3/RadioManager.h` + `SenderV3/RadioManager.cpp`: ESP-NOW-Init (`WIFI_STA`, Kanal 1), Discovery (FT_HELLO-Broadcast max. 1 Hz bis FT_HELLO_ACK, Peer-Registrierung), `sendCommand(RadioCommand) → TransmissionResult` mit Send-Callback-ACK + 3 Retries à 50 ms und `seq`-Verwaltung, `pingQualityTest(n, interval)` für US5 — nach `contracts/espnow-protocol.md` und R-3/R-4/R-5
- [x] T010 `EmpfaengerV3/RadioManager.h` + `EmpfaengerV3/RadioManager.cpp`: ESP-NOW-Empfang mit Validierung (Länge/magic/checksum), Dedup per `seq` (FR-008), FT_HELLO → FT_HELLO_ACK unicast + Peer-Registrierung, Kommando-Übergabe als Flag/Queue an den Loop (ISR-sicher, `volatile`)
- [x] T011 Build-Verifikation Foundational: `pio run` in beiden Ordnern fehlerfrei

**Checkpoint**: Funkstrecke + Konfiguration stehen — User Stories können beginnen

---

## Phase 3: User Story 1 — Passe-Zyklus auf V3-Hardware (P1) 🎯 MVP

**Goal**: Kompletter Turnierbetrieb: Konfigurieren → Pfeile holen ↔ Schießbetrieb mit autonomem
Empfänger (FR-004/FR-004a), 2-Tasten-Bedienung, e-Paper-UI, NVS-Persistenz

**Independent Test**: quickstart.md HIL #3–#10 (Konfiguration, Passe starten/stoppen,
autonomes Ende, Gruppenwechsel, Partial-Refresh-Countdown)

### Empfänger (zuerst — mit Debug-Sender/HELLO testbar)

- [x] T012 [P] [US1] `EmpfaengerV3/DisplayManager.h` + `EmpfaengerV3/DisplayManager.cpp` portieren aus `Empfaenger/DisplayManager.*`: Logik unverändert (7-Segment-Mapping, `displayTimer`, `setGroup`), nur Includes/Pins auf `EmpfaengerV3/Config.h` (Data = D10/GPIO10)
- [x] T013 [P] [US1] `EmpfaengerV3/BuzzerManager.h` + `EmpfaengerV3/BuzzerManager.cpp` portieren aus `Empfaenger/BuzzerManager.*`: API `beep(n)` erhalten, intern AVR-`tone()` durch LEDC-PWM ersetzen (2700 Hz, vorerst fester Duty 50 % — Lautstärke-Poti kommt in US4/T029), nicht-blockierend wie V2
- [x] T014 [US1] `EmpfaengerV3/EmpfaengerV3.ino`: Hauptlogik aus `Empfaenger/Empfaenger.ino` portieren — Zustandsmodell (data-model.md §4), `updateTimer()` 1:1 inkl. **autonomem Passenende und Gruppenwechsel** (FR-004, Original Zeilen 518–639), Ampelfarben (grün/orange ab 30 s/rot), CMD-Dispatch über RadioManager (CMD_START während laufender Vorbereitungsphase ignorieren!), 1-Hz-Zeitbasis per `esp_timer` + `volatile`-Flag (R-7), Status-LED D9 aktiv LOW (an = bereit, Blink bei Empfang; Strapping-Befund 3), in `setup()` früh POTI_GND D4 auf OUTPUT LOW (Strapping-Befund 2)

### Sender

- [x] T015 [P] [US1] `SenderV3/EpaperDisplay.h` + `SenderV3/EpaperDisplay.cpp`: GxEPD2-Wrapper (`GxEPD2_154_D67`, SPI CLK=14/MOSI=13, CS=21, DC=47, RST=48, BUSY=38), LOAD-Rail-Steuerung (GPIO7 an vor `init()`), API: `fullRefresh(screen)`, `partialUpdate(window)`, Statuszeilen- und Countdown-Fenster aus data-model.md §7, `hibernate()`, Ghosting-Regel R-2
- [x] T016 [P] [US1] `SenderV3/ButtonManager.h` + `SenderV3/ButtonManager.cpp` portieren aus `Sender/ButtonManager.*`: 2 Taster — BTN1 GPIO15 **aktiv HIGH** (keine internen Pulls), BTN2 GPIO9 `INPUT_PULLUP` aktiv LOW; Debounce 80 ms; Gesten kurz/lang (2 s/3 s-Schwellen melden, Auswertung in StateMachine); **Boot-Lockout bis BTN1 erstmals losgelassen** (Edge Case „Boot press duration"); Buzzer-Code entfernen (kein Buzzer am V3-Sender, FR-019)
- [x] T017 [P] [US1] `SenderV3/ConfigStore.h` + `SenderV3/ConfigStore.cpp`: TournamentConfig via `Preferences` (R-6, data-model.md §1) — `load()` mit Whitelist-Validierung + Defaults 120 s/2, `save()` sofort bei Bestätigung
- [x] T018 [US1] Menüs portieren auf e-Paper + 2 Tasten: `SenderV3/ConfigMenu.h/.cpp` (aus `Sender/ConfigMenu.*`: Wrap-around-Navigation mit BTN2, Bestätigen BTN1, „Ändern"-los wie V2-Stand), `SenderV3/SchiessBetriebMenu.h/.cpp` (Countdown mm:ss im Partial-Fenster, 1 Hz), `SenderV3/PfeileHolenMenu.h/.cpp` (aus `Sender/PfeileHolenMenu.*`: Nächste Passe/Reihenfolge/Neustart) — Rendering ausschließlich über EpaperDisplay-API
- [x] T019 [US1] `SenderV3/StateMachine.h` + `SenderV3/StateMachine.cpp` portieren aus `Sender/StateMachine.*`: Zustände/Übergänge 1:1 (data-model.md §3), Gruppen-4-Zyklus (`advanceToNextGroup` AB/1→CD/2→CD/1→AB/2), **`handleShootingPhaseEnd(false)` bei Zeitablauf = KEIN CMD_STOP, CMD_START für 2. Gruppe nur als Sync** (FR-004a-Regression-Guard!), `sendCommand` auf RadioManager, Renderaufrufe auf Menü-Klassen, EEPROM-Aufrufe auf ConfigStore
- [x] T020 [US1] `SenderV3/SenderV3.ino` vervollständigen: setup-Reihenfolge (LATCH! → ConfigStore → Buttons → LOAD-Rail+Display → Radio-Init+Discovery → StateMachine, Splash zunächst als einfacher Startscreen — voller Splash kommt in US5/T032), loop (Button-Polling, 1-Hz-Flag, StateMachine.update), `resetSenderTimer()`-Äquivalent für synchronen Gruppen-Neustart (R-7)
- [ ] T021 [US1] Verifikation: ~~`pio run` beide Firmwares fehlerfrei~~ ✅ (2026-06-11), ~~`Commands.h`-Gleichheit~~ ✅ (fc /b: identisch); **offen: mit Nutzer HIL #3–#10** aus `quickstart.md` durchführen (insbes. #7 autonomes Ende ohne Funk und #8 = SC-012-Test des V2-Bugfixes)

**Checkpoint**: MVP — Turnier auf V3-Hardware vollständig durchführbar

---

## Phase 4: User Story 2 — One-Button-Power & Akku-Management (P2)

**Goal**: Soft-Power (Latch), geordnetes Ausschalten, Akku-/Lade-/USB-Anzeige

**Independent Test**: quickstart.md HIL #1, #12–#14

- [x] T022 [P] [US2] `SenderV3/PowerManager.h` + `SenderV3/PowerManager.cpp`: Latch-Verwaltung (GPIO16), `powerOff()`-Sequenz (Abschalt-Screen → `display.hibernate()` → LOAD aus → LATCH LOW), Akkumessung `analogReadMilliVolts(GPIO10)` × 2,5 mit Median-5 (R-11, V2-Schwellen), Lader-Decode C_ST1/C_ST2/C_PG als `INPUT_PULLUP` → `CHARGING/COMPLETE/FAULT/NO_INPUT` (R-12), USB_CON GPIO8, Update-Intervall 5 s (data-model.md §5)
- [x] T023 [US2] Statuszeile in `SenderV3/EpaperDisplay.cpp`/Menüs erweitern: Akku-%-Symbol, USB-/Ladesymbol, Low-Battery-Warnung < 3,3 V (FR-016/FR-017) — Partial-Refresh nur bei Wertänderung
- [x] T024 [US2] Power-Off-Geste in `SenderV3/StateMachine.cpp` integrieren: BTN1 ≥ 3 s in SPLASH/CONFIG/PFEILE_HOLEN/ALARM → `PowerManager::powerOff()`; im SCHIESS_BETRIEB deaktiviert (FR-015, Gesten-Tabelle data-model.md §3)
- [ ] T025 [US2] Verifikation: ~~`pio run`~~ ✅ (2026-06-11); **offen: mit Nutzer HIL #1** (Einschalten/Boot-Lockout), #12 (Ausschalten), #13 (NVS über Power-Cycle), #14 (Ladeanzeige)

**Checkpoint**: US1 + US2 unabhängig funktionsfähig

---

## Phase 5: User Story 3 — Not-Alarm (P2)

**Goal**: Alarm-Geste, zuverlässige Übertragung mit Retries, Alarm-Muster auf beiden Geräten

**Independent Test**: quickstart.md HIL #11

- [x] T026 [P] [US3] `SenderV3/AlarmScreen.h` + `SenderV3/AlarmScreen.cpp` portieren aus `Sender/AlarmScreen.*`: Alarm-UI auf e-Paper (Voll-Refresh, statisch — kein Blinken nötig), Quittierungs-Hinweis
- [x] T027 [P] [US3] Alarm-Muster in `EmpfaengerV3/EmpfaengerV3.ino` portieren (aus `Empfaenger/Empfaenger.ino` `updateAlarm()`, Zeilen ~460–510): CMD_ALARM → Countdown abbrechen, 8× alles rot blinken (nicht-blockierend), danach STOPPED; Status-LED-Mapping statt 3 V2-LEDs
- [x] T028 [US3] Alarm-Auslösung in `SenderV3/StateMachine.cpp`: BTN1 ≥ 2 s im SCHIESS_BETRIEB → CMD_ALARM mit 3 App-Retries à 200 ms (V2 `ALARM_MAX_RETRIES`), Zustellstatus auf Alarmscreen anzeigen (US3-Szenario 3), Quittierung → CMD_STOP → PFEILE_HOLEN; `pio run` ✅ — **offen: mit Nutzer HIL #11** (→ T036)

**Checkpoint**: Alarm end-to-end

---

## Phase 6: User Story 4 — Empfänger-Lokalregler (P3)

**Goal**: Lautstärke-Poti, Helligkeits-Poti, Lüfter-Poti, Debug-Taster

**Independent Test**: quickstart.md HIL #15–#17, #21

- [x] T029 [P] [US4] Poti-Regler in `EmpfaengerV3/EmpfaengerV3.ino` + `EmpfaengerV3/BuzzerManager.cpp`: Lautstärke-Poti D0/GPIO2 ≥ 10 Hz lesen, quadratische Kennlinie → LEDC-Duty 0–50 % mit hörbarem Minimum (R-9, FR-021); Helligkeits-Poti D1/GPIO3 → `FastLED.setBrightness(64–255)` (FR-022, V2-Verhalten)
- [x] T030 [P] [US4] `EmpfaengerV3/FanManager.h` + `EmpfaengerV3/FanManager.cpp`: Lüfter-PWM D6/GPIO21 (LEDC ≥ 25 kHz), Drehzahl vom Lüfter-Poti D2/GPIO4 (≥ 10 Hz gelesen, FR-023); kein Tacho (nicht angeschlossen, R-10); Hinweis: Gate-Pull-up → in `setup()` früh definierten Duty setzen
- [x] T031 [US4] Debug-Taster D7/GPIO20 (`INPUT_PULLUP`): lokaler Testlauf implementiert (Druck = Start 120s-Passe ohne Sender, erneuter Druck = Stopp, FR-024); V2-`BRIGHTNESS_DEBUG`-Deckel bewusst NICHT übernommen; `pio run` ✅ — **offen: mit Nutzer HIL #15–#17 und #21** (→ T036)

**Checkpoint**: Alle Komfortfunktionen aktiv

---

## Phase 7: User Story 5 — Verbindungstest beim Start (P3)

**Goal**: Splash mit Ping-Qualitätstest wie V2

**Independent Test**: quickstart.md HIL #2 (+ Variante Empfänger aus)

- [x] T032 [P] [US5] `SenderV3/SplashScreen.h` + `SenderV3/SplashScreen.cpp` portieren aus `Sender/SplashScreen.*`: Logo/Version auf e-Paper (ein Voll-Refresh), Fortschritts-/Ergebnisbereich als Partial-Fenster
- [x] T033 [US5] Qualitätstest integriert in `SenderV3/StateMachine.cpp`: nach Discovery 10 × FT_PING im 250-ms-Raster (R-5), Ergebnis in % angezeigt; ohne HELLO_ACK „Keine Verbindung" + Menü nach 15 s trotzdem freigegeben (US5-Szenario 2); BTN1 kurz = Splash überspringen; `pio run` ✅ — **offen: mit Nutzer HIL #2** (→ T036)

**Checkpoint**: Alle 5 User Stories vollständig

---

## Phase 8: Polish & Cross-Cutting

- [x] T034 [P] Konsistenz-Checks — **komplett erledigt (2026-06-11)**: `Commands.h` byte-identisch (fc /b), Pin-Definitionen gegen `contracts/hardware-pins.md` (Config.h direkt aus dem Contract befüllt, statische Asserts für ADC1-Regel), finale `pio run`-Builds beider Firmwares SUCCESS (arduino-esp32 3.3.7). Der ursprünglich geforderte Arduino-IDE-Build ist **entfallen** (Anforderungsänderung 2026-06-11: vollständige PlatformIO-Migration, FR-026 aktualisiert, Constitution v2.2.0)
- [x] T035 [P] Dokumentation: `README.md` + `HARDWARE.md` um V3-Abschnitte ergänzen (Hardware-Tabelle, Pinouts aus contracts, Build-Anleitung aus quickstart.md), `CLAUDE.md` Projektübersicht/Pin-Belegung um V3 erweitern; Doxygen-Kommentare mit Connector-Referenzen (J1, J5, SW2 …) vervollständigen (Constitution V)
- [ ] T036 Komplette HIL-Abnahme mit Nutzer: alle 21 Punkte aus `quickstart.md` inkl. Reichweite (#19, SC-004) und Protokoll-Vollabdeckung (#18, SC-009)
- [x] T037 Constitution-Amendment — **erledigt 2026-06-10** (v2.0.0 + v2.1.0): V3-Hardware-Standards aufgenommen, `Schaltung_v3/BogenampelV3/` als autoritative Quelle, Betriebsparameter aktualisiert (Init < 10 s); USB-Strombegrenzungs-Regel entfernt (LED-Versorgung immer extern)

---

## Dependencies & Execution Order

### Phasen-Abhängigkeiten

- **Phase 1 (Setup)** → blockiert alles; T003 (Schaltplan-Korrektur, ✅ erledigt 2026-06-10) blockierte T007/T008 (Constitution V: Schaltplan zuerst, dann Code); T003b (physisches Rework) blockiert nur HIL-Tasks (T021, T025, T028, T031, T033, T036), nicht die Implementierung
- **Phase 2 (Foundational)** → blockiert alle User Stories (Commands.h/Config.h/RadioManager werden überall gebraucht)
- **US1 (Phase 3)** → MVP; US2–US5 bauen auf den in US1 erstellten Dateien auf (StateMachine, EpaperDisplay, EmpfaengerV3.ino), sind aber einzeln testbar
- **US2/US3 (Phase 4/5)** → unabhängig voneinander, beide nach US1 (erweitern StateMachine)
- **US4 (Phase 6)** → nach US1 (erweitert EmpfaengerV3.ino/BuzzerManager); unabhängig von US2/US3
- **US5 (Phase 7)** → nach US1 (nutzt RadioManager + EpaperDisplay); unabhängig von US2–US4
- **Phase 8 (Polish)** → nach allen gewünschten Stories

### Innerhalb der Stories

- Manager-/Wrapper-Klassen [P] vor Integrations-Tasks (z. B. T015–T017 vor T018–T020)
- Empfänger-Pfad (T012–T014) und Sender-Pfad (T015–T020) sind innerhalb von US1 parallel bearbeitbar

### Parallel-Beispiel: User Story 1

```text
# Gleichzeitig (verschiedene Dateien):
T012 DisplayManager (Empfänger)   | T015 EpaperDisplay (Sender)
T013 BuzzerManager (Empfänger)    | T016 ButtonManager (Sender)
                                  | T017 ConfigStore  (Sender)
# Danach sequenziell: T014 (Empfänger-Integration) bzw. T018 → T019 → T020 (Sender)
```

---

## Implementation Strategy

**MVP zuerst (nur US1)**: Phase 1 → Phase 2 → Phase 3, dann STOPP und HIL #3–#10 validieren —
damit ist ein einsatzfähiger Turniertimer auf V3-Hardware vorhanden (Demo-/Feldtest-fähig).

**Inkrementell**: danach US2 (Power — macht das Gerät alltagstauglich), US3 (Alarm —
sicherheitsrelevant vor echtem Einsatz), US4/US5 (Komfort). Nach jeder Story: Build + zugehörige
HIL-Punkte. Physische Hardware-Reworks (T003b) sollten vor dem ersten HIL-Termin erledigt sein.

**Hinweis Aufwandsverteilung**: Die Portierungs-Tasks (T012–T014, T018–T019) übernehmen getestete
V2-Logik — Änderungen dort minimal halten (nur Plattform-API-Ersatz), damit das V2-Verhalten
nachweislich erhalten bleibt (SC-001).
