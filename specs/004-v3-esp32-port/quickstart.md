# Quickstart: Bogenampel V3 bauen, flashen, testen

**Feature**: 004-v3-esp32-port

## Voraussetzungen

- **Hardware-Rework vor dem ersten Test** (siehe `contracts/hardware-pins.md`):
  1. Empfänger: Helligkeits-Poti von D5 auf **D1** umverdrahten (Lochraster).
  2. Sender: **R7 mit 240k bestücken** (R2 bleibt 47k; BTN1-Pegel — Schaltplan seit
     2026-06-10 korrigiert und per Netzliste verifiziert).
  3. Empfänger: Piezo auf **D3**, Lüfter-Poti an **D2**, Status-LED **aktiv LOW**, Tacho
     unbeschaltet, Lautstärke-Poti-Fußpunkt an **D4** — Schaltplan und Lochraster erledigt
     (verifiziert/bestätigt 2026-06-10).
  4. Hinweis: Lüfter läuft beim Boot/Flashen voll (Gate-Pull-up R5, Befund 6 in
     `contracts/hardware-pins.md`).
- PlatformIO (`pio` im PATH) — einziger unterstützter Build-Pfad (vollständige
  PlatformIO-Migration, Entscheidung 2026-06-11, Constitution v2.2.0).

## Build & Flash — PlatformIO (SC-011)

```bash
# Sender (ESP32-S3, natives USB)
cd SenderV3
pio run                 # bauen
pio run -t upload       # flashen (USB-C des Geräts)
pio device monitor      # 115200 Baud, USB-CDC

# Empfänger (XIAO ESP32C3)
cd EmpfaengerV3
pio run
pio run -t upload
pio device monitor
```

Erstes Flashen des Senders: BTN1 gedrückt halten (Gerät an), dann ggf. Boot-Taster (SW4/IO0)
beim Stecken halten, falls der Bootloader nicht automatisch erreicht wird.

> Hinweis: Der frühere Arduino-IDE-Build-Pfad ist seit 2026-06-11 gestrichen
> (vollständige PlatformIO-Migration). Libraries kommen versioniert über `lib_deps`
> aus der jeweiligen `platformio.ini`; nichts muss manuell installiert werden.

## Hardware-in-the-Loop-Checkliste (Akzeptanz, SC-001…SC-012)

| # | Test | Erwartung | Spec |
|---|------|-----------|------|
| 1 | BTN1 kurz drücken (Gerät aus) | Sender bootet, bleibt an, Splash + Verbindungstest | US2, SC-006 |
| 2 | Splash mit Empfänger an | Qualität ≥ 90 % angezeigt | US5, SC-004 |
| 3 | Config: BTN2 mehrfach | Auswahl läuft mit Wrap-around (120↔240, 1-2↔3-4, Start) | US1 |
| 4 | Start bestätigen | Empfänger: INIT (rot); Sender: Pfeile holen | US1 |
| 5 | „Nächste Passe" | Empfänger: 2 Pieptöne, 10 s rot, dann Countdown grün; Sender-Countdown 1 Hz ohne Vollbild-Blitzen | US1, SC-007 |
| 6 | Countdown < 30 s | Empfänger orange | US1 |
| 7 | Zeit ablaufen lassen (1-2 Schützen) | Empfänger: autonom „000" rot + 3 Pieptöne, OHNE Funk; Sender geht selbst zu Pfeile holen | FR-004/004a |
| 8 | **Sender nach Start ausschalten**, Zeit ablaufen lassen | Passe endet trotzdem regulär (Test des V2-Bugfixes) | SC-012 |
| 9 | 3-4 Schützen, Zeitablauf 1. Gruppe | beide Geräte wechseln autonom zur 2. Gruppe (neue 10-s-Vorbereitung); Gruppen-LEDs korrekt (4-Zyklus) | US1, FR-004 |
| 10 | „Passe beenden" während Countdown | CMD_STOP: Empfänger sofort rot + 3 Pieptöne | US1, SC-002 |
| 11 | Schießbetrieb: BTN1 2 s halten | Alarm: 8× blinken, Sender zeigt Alarmscreen | US3 |
| 12 | Config-Menü: BTN1 3 s halten | Abschalt-Screen, Gerät komplett aus | US2, SC-006 |
| 13 | Aus/Ein nach Konfig-Änderung | Konfiguration wiederhergestellt (NVS) | SC-008 |
| 14 | USB anstecken | Lade-Symbol; nach Ladeende „voll" | US2 |
| 15 | Lautstärke-Poti drehen während Pieptönen | Lautstärke folgt stufenlos | US4, SC-010 |
| 16 | Helligkeits-Poti drehen bei laufender Anzeige | Helligkeit 25–100 % | US4, SC-010 |
| 17 | Debug-Taster am Empfänger (ohne Sender) | lokaler Testlauf wie V2-Debugmodus | US4 |
| 18 | Alle 11 Kommandos (Protokolltest) | je Kommando korrekte Reaktion + TX-Feedback am Sender | SC-009 |
| 19 | Reichweite ≥ 30 m Freifeld | Kommandos weiterhin bestätigt | SC-004 |
| 20 | `Commands.h` Diff Sender↔Empfänger | identisch (`fc /b SenderV3\Commands.h EmpfaengerV3\Commands.h`) | Plan |
| 21 | Lüfter-Poti drehen | Lüfterdrehzahl folgt stufenlos (PWM); Empfänger bootet in jeder Poti-Stellung (Strapping-Fix) | US4, SC-010 |

## Bekannte Stolpersteine

- **ADC2-Falle**: Mit aktivem Funk sind ADC2-Pins unbrauchbar — deshalb Potis auf D0/D1 (ADC1)
  und BTN1 rein digital.
- **Power-Latch**: `LATCH=HIGH` muss die allererste Aktion in `setup()` sein, sonst geht das
  Gerät beim Loslassen von BTN1 wieder aus.
- **e-Paper**: Erst LOAD-Rail (GPIO7) einschalten, dann `display.init()`; vor dem Ausschalten
  `hibernate()` — sonst Geisterbilder/undefinierte Panel-Zustände.
- **Discovery**: Empfänger zuerst einschalten ist NICHT nötig (Sender wiederholt HELLO), aber
  der Qualitätstest im Splash zeigt ohne laufenden Empfänger „keine Verbindung".
