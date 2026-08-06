# Bogenampel Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-06-11

## Project Overview

Bogenampel ist eine funkgesteuerte Timer-Anzeige für Bogenschießplätze.

**Aktuelle Generation V3 (`Sender/`, `Empfaenger/`, `Schaltung-Sender/`, `Schaltung-Empfaenger/`):**
- **Sender (Bedieneinheit)**: ESP32-S3-WROOM-1U + 1.54″ e-Paper (SSD1681) + LiPo/MCP73837 + Power-Latch
- **Empfänger (Anzeigeeinheit)**: XIAO ESP32C3 + WS2811 LED Strip (12 V, 66 Pixel) + Lüfter + 3 Potis,
  seit Rev. 2026-08-03 auf der PD-12V-Platine (CH224K-Trigger, Pegelwandler 74AHCT1G125)
- **Kommunikation**: ESP-NOW (Kanal 1, 6-Byte-Frames, Discovery zur Laufzeit), 11 Kommandos unverändert
- **Features**: Timer-Steuerung, Gruppen-Anzeige, Alarm-System, NVS-Konfiguration, autonomes Passenende (FR-004),
  OTA-Wartungsmodus (beide Taster beim Einschalten; im Normalbetrieb läuft kein WiFi)
- **Stromsparen am Sender** (2026-08-02, gemessen 0,39 W → 0,20 W): CPU 80 MHz,
  ESP-NOW-Empfangsfenster duty-cycled, Auto-Abschaltung nach Inaktivität
  (seit 006: 60 min statt 20).
  **Nicht am 1-Hz-Countdown-Refresh drehen** — das e-Paper macht unter 2 % aus.

**Legacy V2 (vollständig entfernt am 2026-08-01):**
- Arduino Nano + ST7789 TFT bzw. WS2812B + NRF24L01 (2.4 GHz, 250 kbps)
- Firmware (`Sender/`, `Empfaenger/`), Bibliotheken (`libraries/`, 55 MB) und
  KiCad-Projekt (`Schaltung/`) liegen nur noch in der Git-Historie. FR-025
  („V2 bleibt eingefroren erhalten") ist damit überholt: V2 wird nicht mehr im
  Arbeitsverzeichnis vorgehalten. Letzter Stand vor der Entfernung: Commit
  `e632bfb`.
- **Achtung**: Die Ordnernamen `Sender/` und `Empfaenger/` sind seither neu
  belegt — sie enthalten jetzt die V3-Firmware.

## Active Technologies
- C++ (Arduino core for ESP32 / arduino-esp32 3.x, C++17; V2 logic is C++11 and ports without changes) (004-v3-esp32-port)
- ESP32 NVS via `Preferences` (replaces AVR EEPROM), namespace `bogenampel` (004-v3-esp32-port)
- C++17, Arduino core for ESP32 (arduino-esp32 3.3.7) + WiFi, ArduinoOTA, ESPmDNS (Arduino-ESP32 built-ins), FastLED (already used); ESP-NOW via `esp_now`/`esp_wifi` (normal path only) (005-ota-maintenance-mode)
- N/A — no new persistence. WiFi credentials remain compile-time (`wifi_credentials.h`, gitignored); no NVS changes. (005-ota-maintenance-mode)
- C++17, arduino-esp32 3.3.7 + GxEPD2-Busy-Callback (`setBusyCallback()` — hält die
  Tastenabfrage während des 300-400 ms blockierenden e-Paper-Refreshs am Laufen);
  keine neue Persistenz, kein Protokoll-Eingriff (006-shutdown-alarm-gesten)

- **C++** (V3: arduino-esp32/PlatformIO; V2-Legacy: Arduino Nano, C++11)
- **Libraries**:
  - Adafruit ST7789 (Display)
  - Adafruit GFX (Graphics)
  - RF24 (NRF24L01 Funk)
  - FastLED (WS2812B LED Strip)

## Project Structure

```text
platformio.ini        # ZENTRAL im Repo-Root — beide Envs, kein pio in den Unterordnern

Sender/               # Bedieneinheit (ESP32-S3 + e-Paper)
  ├── Sender.cpp      # Setup/Loop, Power-Latch zuerst!, esp_timer 1 Hz
  ├── Config.h        # Pins aus contracts/hardware-pins.md, NVS, Timing
  ├── Commands.h      # ESP-NOW-Protokoll (byte-identisch mit Empfaenger/!)
  ├── RadioManager.*  # ESP-NOW: Discovery, sendCommand+Retry, Qualitätstest,
  │                   #   Wake-Window (Strom!), Relink nach 3 Fehlversuchen
  ├── PowerManager.*  # Latch, Akku (Median-5), MCP73837-Status, Power-Off
  ├── EpaperDisplay.* # GxEPD2-Wrapper: Voll-/Partial-Refresh, Statuszeile
  ├── ButtonManager.* # 2 Taster; Rolle→Pin NUR in readRawState() zugeordnet
  ├── OTAManager.*    # Wartungsmodus: WiFi-Station + ArduinoOTA (kein SoftAP)
  ├── OtaScreen.*     # Wartungsmodus-Anzeige: WLAN-Status + eigene IP
  ├── ConfigStore.*   # NVS-Persistenz (Preferences)
  ├── StateMachine.*  # 5 States — Logik 1:1 aus V2 (FR-004a-Guard!)
  └── *Menu/*Screen.* # ConfigMenu, SchiessBetrieb, PfeileHolen, Splash, Alarm

Empfaenger/           # Anzeigeeinheit (XIAO ESP32C3)
  ├── Empfaenger.cpp  # Timer-Logik 1:1 aus V2 (autonomes Passenende!)
  ├── Config.h        # XIAO-Pins (Strapping-Fixes!), LED-Layout unverändert
  ├── Commands.h      # byte-identische Kopie von Sender/Commands.h
  ├── RadioManager.*  # ESP-NOW-Empfang: Validierung, Dedup, HELLO_ACK
  ├── DisplayManager.*# 7-Segment + Gruppen (unverändert aus V2)
  ├── BuzzerManager.* # LEDC-PWM, Lautstärke vom Poti (Duty 0-50%)
  ├── FanManager.*    # Lüfter-PWM 25 kHz, Drehzahl vom Poti
  └── OTAManager.*    # Wartungsmodus (Debug-Taster D7 beim Boot)

Schaltung-Sender/     # KiCad Sender    — autoritative Hardware-Quelle (Constitution V)
Schaltung-Empfaenger/ # KiCad Empfänger — dito
schaltplan-sender.png     # Schaltplan-Export, ohne KiCad lesbar
schaltplan-empfaenger.png

specs/                # Feature-Spezifikationen (004-v3-esp32-port = V3-Port)
```

**Hinweis zur Namensgebung**: `Sender/` und `Empfaenger/` enthalten seit dem
2026-08-01 die **V3-Firmware** (ESP32). Zuvor lag dort die V2-Firmware
(Arduino Nano), die zusammen mit `libraries/` entfernt wurde und nur noch über
die Git-Historie auffindbar ist. Ältere Commits und Spec-Dokumente sprechen
daher von `SenderV3/` und `EmpfaengerV3/` — gemeint sind dieselben Ordner.

## Commands

### V3: Build & Upload (PlatformIO — einziger unterstützter Build-Pfad)
```bash
cd /d/git/Bogenampel   # Repo-Root — platformio.ini liegt HIER, nicht in den Firmware-Ordnern
pio run -e sender          # bauen (oder -e empfaenger)
pio run -t upload -e sender # flashen (Sender: BTN1 dabei halten!)
pio device monitor -e sender # 115200 Baud
pio run -e sender-release  # Feld-Build ohne Debug-Ausgaben/CDC
pio run -t upload -e sender-release-ota  # Feld-Build per OTA (Auslieferungsweg)
```
**Sender-USB-Flash**: `upload_speed` MUSS 115200 bleiben (natives USB-Serial/JTAG — ein
Baudratenwechsel re-enumeriert den Port und killt den Upload). `pio run -t upload` startet
esptool erst nach ~65 s Build-Scan; für BTN1-Halten ist ein direkter esptool-Aufruf mit
den vier Images angenehmer. **OTA-Diagnose**: ArduinoOTA lauscht auf **UDP** 3232 — ein
TCP-Portscan sagt nichts über die Erreichbarkeit aus.
Libraries kommen versioniert über `lib_deps` (kein manuelles Installieren).
Arduino-IDE-Build ist seit 2026-06-11 keine Anforderung mehr (Constitution v2.2.0).

### V2 (Legacy): nicht mehr im Arbeitsverzeichnis
V2 wurde am 2026-08-01 entfernt. Zum Nachschlagen aus der Historie:
```bash
git log --all --oneline -- Sender/Sender.ino     # letzten V2-Commit finden
git show <commit>:Sender/Sender.ino              # Datei ansehen
```
Baubar war V2 nur mit der Arduino IDE (Nano, ATmega328P Old Bootloader;
Adafruit ST7789 + GFX, RF24, FastLED aus dem entfernten `libraries/`).

## Code Style

- **C++ (Arduino, C++11)**: Follow standard conventions
- **Naming**:
  - Classes: PascalCase (StateMachine, ButtonManager)
  - Functions: camelCase (updateDisplay, handleButton)
  - Constants: UPPER_SNAKE_CASE (LED_PIN, MAX_RETRY)
  - Namespaces: PascalCase (Pins, Display, RF, Battery)
- **Files**: Flat structure (no subdirs) — Projekt-Konvention (V3 baut nur mit PlatformIO)
- **Comments**: Doxygen-style for functions
- **Pin Definitions**: Centralized in Config.h using constexpr

## Key Features Implemented

1. **Interrupt-basierte Timer-Synchronisation** (Timer1 ISR, 1 Hz)
2. **Alarm-System** (OK-Taste 2s halten, 8x blinken)
3. **Gruppen-Anzeige** (A/B, C/D mit 4-State Cycle)
4. **Halbe Passe** (Position 1, Position 2)
5. **7-Segment LED-Display** (158 LEDs: 16 AB + 16 CD + 126 Timer)
6. **EEPROM-Konfiguration** (shootingTime, shooterCount)
7. **Batterie-Überwachung** (Median-Filter, Low-Battery-Warnung)
8. **Menü-System** (Config, Schießbetrieb, Pfeile Holen)

## Hardware Pin-Belegung V3 (aktuell)

Verbindlich: `specs/004-v3-esp32-port/contracts/hardware-pins.md` (aus KiCad-Netzlisten).

### Sender (ESP32-S3)
- Power: LATCH=GPIO16 (**erste Aktion in setup(): HIGH!**), LOAD-Rail=GPIO7
- Taster: BTN1=GPIO15 (**aktiv HIGH**, keine internen Pulls), BTN2=GPIO9 (INPUT_PULLUP).
  Rollen seit 2026-08-01 an der Gehäusebeschriftung ausgerichtet: GPIO15 = **CONFIG**
  (Weiter/Ändern, hält beim Einschalten den Latch), GPIO9 = **OK** (Bestätigen,
  2 s Alarm, 3 s Aus). Zuordnung ausschließlich in `ButtonManager::readRawState()`.
  Wartungsmodus = beide beim Einschalten halten.
- e-Paper: CS=21, DC=47, RST=48, BUSY=38, CLK=14, MOSI=13 (write-only)
- Akku: ADC_BAT=GPIO10 (ADC1, ×2,5); USB_CON=GPIO8; Lader ST1/ST2/PG/PRG=11/12/17/18
- Reserviert: GPIO35-37 (Octal-PSRAM), GPIO19/20 (natives USB)

### Empfänger (XIAO ESP32C3)
- Potis (alle ADC1!): Lautstärke=D0/GPIO2 (Fußpunkt an D4!), Helligkeit=D1/GPIO3, Lüfter=D2/GPIO4
- POTI_GND=D4/GPIO6 (in setup() früh OUTPUT LOW — Strapping-Fix GPIO2)
- Piezo=D3/GPIO5 (LEDC), Lüfter-PWM=D6/GPIO21 (LEDC 25 kHz)
- Debug-Taster=D7/GPIO20, Status-LED=D9/GPIO9 (**aktiv LOW** — Strapping-Fix),
  WS2811-Data=D10/GPIO10 (über Pegelwandler U5 74AHCT1G125 → 5 V; früh OUTPUT LOW setzen!)
- Frei: D5/GPIO7, D8/GPIO8

### V2 (Legacy)
- Sender: NRF24 CE=D9/CSN=D8; Display CS=A0/DC=D10/RST=A1; Buttons D5/D6/D7; Buzzer D4; Battery A4
- Empfänger: NRF24 CE=D9/CSN=D8; LED-Strip D3; Buzzer D4; Debug D7/D2; Status-LEDs A2-A4

## Recent Changes
- 2026-08-05: **Empfänger-Firmware auf die PD-12V-Platine angepasst** (Schaltplan-Rev.
  `cdde8dc`). Pin-Funktionen unverändert, aber: `BRIGHTNESS_MAX` 64 → **255** (der
  USB-Übergangsdeckel ist hinfällig — der Strip hängt jetzt direkt am 12-V-PD-Netz, der
  XIAO an einem eigenen TSR0.5-2433) und GPIO10 wird in `setup()` **früh auf OUTPUT LOW**
  gelegt, weil der neue Pegelwandler U5 (74AHCT1G125) keinen Eingangs-Pulldown hat.
  Netzteil: 12 V / ≥ 2 A. Referenzbezeichner haben sich verschoben (Potis jetzt J3/J4/J2,
  Piezo J8, Lüfter J6). **HIL-Abnahme auf der neuen Platine offen.**
- 006-shutdown-alarm-gesten (2026-08-02, **Code fertig, HIL-Abnahme offen**):
  Ausschalten und Alarm entkoppelt — Halten ≥ 3 s (beliebige Taste) = Aus in *allen*
  Zuständen (zentral in `StateMachine::update()`), **OK** 3× ≤ 400 ms = Alarm
  (nur Schießbetrieb + Pfeile holen; CONFIG löst bewusst KEINEN Alarm aus),
  Idle-Abschaltung 20 → 60 min. Die alte 2-s-Alarm-Geste ist entfallen.
  **Merkposten**: Die Folge schließt auf der *Loslass*-Flanke ab — auf der
  Drückflanke stünde der Alarm fest, bevor sich zeigt, ob der dritte Druck ein Klick
  oder der Beginn eines Haltens ist. Ebenso muss `discardPendingClicks()` beim
  Auslösen und am Ende von `enterAlarm()` laufen — sonst quittiert ein während
  des Bildaufbaus liegengebliebener Klick den Alarm sofort wieder (im Feldtest
  beobachtet: Alarmbildschirm blitzt auf, Gerät fällt nach „Pfeile holen"
  zurück). Und `ButtonManager::update()` hängt jetzt
  zusätzlich im GxEPD2-Busy-Callback (`Sender.cpp`), weil die Tastenabfrage sonst
  bei jedem Refresh 300-400 ms steht und schnelle Klicks verschluckt.
  Abnahme: `specs/006-shutdown-alarm-gesten/quickstart.md` (T-01 … T-64).
- 2026-08-02: **Stromverbrauch Sender halbiert** (0,39 W → 0,20 W, gemessen):
  `setCpuFrequencyMhz(80)` im Normalbetrieb (OTA-Modus bleibt bei 240),
  ESP-NOW Connectionless Power Save (`esp_now_set_wake_window()` — nach der Discovery
  zu, währenddessen nur 500 ms je HELLO), Peer-Relink nach 3 Fehlversuchen,
  Auto-Abschaltung nach 20 min Inaktivität, `env:sender-release`.
  Nicht machbar: PSRAM abschalten (`CONFIG_SPIRAM=1` in allen vorkompilierten
  Lib-Varianten) und automatisches Light-Sleep (`CONFIG_PM_ENABLE` fehlt) — beides
  bräuchte einen eigenen IDF-Build.
- 005-ota-maintenance-mode: Added C++17, Arduino core for ESP32 (arduino-esp32 3.3.7) + WiFi, ArduinoOTA, ESPmDNS (Arduino-ESP32 built-ins), FastLED (already used); ESP-NOW via `esp_now`/`esp_wifi` (normal path only)
- 2026-06-11: **V3-Firmware implementiert** (`Sender/`, `Empfaenger/`, damals `SenderV3/`/`EmpfaengerV3/`): ESP-NOW-Transport,
  e-Paper-UI, Soft-Power, NVS, Lokalregler — beide Firmwares bauen mit PlatformIO
  (arduino-esp32 3.3.7); HIL-Abnahme offen (quickstart.md #1-#21)
- 004-v3-esp32-port: Added C++ (Arduino core for ESP32 / arduino-esp32 3.x, C++17; V2 logic is C++11 and ports without changes)


<!-- MANUAL ADDITIONS START -->
Kommunikationssprache ist deutsch, informell und immer per Du
<!-- MANUAL ADDITIONS END -->
