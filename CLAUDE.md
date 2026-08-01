# Bogenampel Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-06-11

## Project Overview

Bogenampel ist eine funkgesteuerte Timer-Anzeige für Bogenschießplätze.

**Aktuelle Generation V3 (`SenderV3/`, `EmpfaengerV3/`, `Schaltung-Sender/`, `Schaltung-Empfaenger/`):**
- **Sender (Bedieneinheit)**: ESP32-S3-WROOM-1U + 1.54″ e-Paper (SSD1681) + LiPo/MCP73837 + Power-Latch
- **Empfänger (Anzeigeeinheit)**: XIAO ESP32C3 + WS2812B LED Strip (158 LEDs) + Lüfter + 3 Potis
- **Kommunikation**: ESP-NOW (Kanal 1, 6-Byte-Frames, Discovery zur Laufzeit), 11 Kommandos unverändert
- **Features**: Timer-Steuerung, Gruppen-Anzeige, Alarm-System, NVS-Konfiguration, autonomes Passenende (FR-004),
  OTA-Wartungsmodus (beide Taster beim Einschalten; im Normalbetrieb läuft kein WiFi)

**Legacy V2 (`Sender/`, `Empfaenger/` — eingefroren, FR-025):**
- Arduino Nano + ST7789 TFT bzw. WS2812B + NRF24L01 (2.4 GHz, 250 kbps)
- Die V2-KiCad-Dateien (`Schaltung/`) wurden am 2026-08-01 entfernt und sind
  nur noch über die Git-Historie auffindbar.

## Active Technologies
- C++ (Arduino core for ESP32 / arduino-esp32 3.x, C++17; V2 logic is C++11 and ports without changes) (004-v3-esp32-port)
- ESP32 NVS via `Preferences` (replaces AVR EEPROM), namespace `bogenampel` (004-v3-esp32-port)
- C++17, Arduino core for ESP32 (arduino-esp32 3.3.7) + WiFi, ArduinoOTA, ESPmDNS (Arduino-ESP32 built-ins), FastLED (already used); ESP-NOW via `esp_now`/`esp_wifi` (normal path only) (005-ota-maintenance-mode)
- N/A — no new persistence. WiFi credentials remain compile-time (`wifi_credentials.h`, gitignored); no NVS changes. (005-ota-maintenance-mode)

- **C++** (V3: arduino-esp32/PlatformIO; V2-Legacy: Arduino Nano, C++11)
- **Libraries**:
  - Adafruit ST7789 (Display)
  - Adafruit GFX (Graphics)
  - RF24 (NRF24L01 Funk)
  - FastLED (WS2812B LED Strip)

## Project Structure

```text
SenderV3/             # V3 Bedieneinheit (ESP32-S3 + e-Paper) — AKTUELL
  ├── SenderV3.cpp    # Setup/Loop, Power-Latch zuerst!, esp_timer 1 Hz
  ├── platformio.ini  # env:sender (esp32-s3-devkitc-1, 16MB, OPI-PSRAM)
  ├── Config.h        # Pins aus contracts/hardware-pins.md, NVS, Timing
  ├── Commands.h      # ESP-NOW-Protokoll (byte-identisch mit EmpfaengerV3!)
  ├── RadioManager.*  # ESP-NOW: Discovery, sendCommand+Retry, Qualitätstest
  ├── PowerManager.*  # Latch, Akku (Median-5), MCP73837-Status, Power-Off
  ├── EpaperDisplay.* # GxEPD2-Wrapper: Voll-/Partial-Refresh, Statuszeile
  ├── ButtonManager.* # 2 Taster: BTN1 aktiv HIGH (Gesten), BTN2 aktiv LOW
  ├── ConfigStore.*   # NVS-Persistenz (Preferences)
  ├── StateMachine.*  # 5 States — Logik 1:1 aus V2 (FR-004a-Guard!)
  └── *Menu/*Screen.* # ConfigMenu, SchiessBetrieb, PfeileHolen, Splash, Alarm

EmpfaengerV3/         # V3 Anzeigeeinheit (XIAO ESP32C3) — AKTUELL
  ├── EmpfaengerV3.cpp# Timer-Logik 1:1 aus V2 (autonomes Passenende!)
  ├── platformio.ini  # env:empfaenger (seeed_xiao_esp32c3)
  ├── Config.h        # XIAO-Pins (Strapping-Fixes!), LED-Layout unverändert
  ├── Commands.h      # byte-identische Kopie von SenderV3/Commands.h
  ├── RadioManager.*  # ESP-NOW-Empfang: Validierung, Dedup, HELLO_ACK
  ├── DisplayManager.*# 7-Segment + Gruppen (unverändert aus V2)
  ├── BuzzerManager.* # LEDC-PWM, Lautstärke vom Poti (Duty 0-50%)
  └── FanManager.*    # Lüfter-PWM 25 kHz, Drehzahl vom Poti

Schaltung-Sender/     # V3 KiCad Sender    — autoritative Hardware-Quelle (Constitution V)
Schaltung-Empfaenger/ # V3 KiCad Empfänger — dito
schaltplan-sender.png     # Schaltplan-Export, ohne KiCad lesbar
schaltplan-empfaenger.png

Sender/               # V2 Bedieneinheit (Arduino Nano) — EINGEFROREN (FR-025)
Empfaenger/           # V2 Anzeigeeinheit (Arduino Nano) — EINGEFROREN (FR-025)
libraries/            # Externe Libraries — NUR V2. Der V3-Build ist davon
                      # unabhängig (verifiziert 2026-08-01: beide Envs bauen
                      # ohne den Ordner); V3 bezieht alles über lib_deps.
specs/                # Feature-Spezifikationen (004-v3-esp32-port = V3-Port)
```

## Commands

### V3: Build & Upload (PlatformIO — einziger unterstützter Build-Pfad)
```bash
cd SenderV3        # oder EmpfaengerV3
pio run            # bauen
pio run -t upload  # flashen (Sender: natives USB-C)
pio device monitor # 115200 Baud
```
Libraries kommen versioniert über `lib_deps` (kein manuelles Installieren).
Arduino-IDE-Build ist seit 2026-06-11 keine Anforderung mehr (Constitution v2.2.0).

### V2 (Legacy): Arduino IDE
1. Open `Sender/Sender.ino` or `Empfaenger/Empfaenger.ino`
2. Board: Arduino Nano, ATmega328P (Old Bootloader)
3. Libraries: Adafruit ST7789, Adafruit GFX, RF24, FastLED

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
- Taster: BTN1=GPIO15 (**aktiv HIGH**, keine internen Pulls), BTN2=GPIO9 (INPUT_PULLUP)
- e-Paper: CS=21, DC=47, RST=48, BUSY=38, CLK=14, MOSI=13 (write-only)
- Akku: ADC_BAT=GPIO10 (ADC1, ×2,5); USB_CON=GPIO8; Lader ST1/ST2/PG/PRG=11/12/17/18
- Reserviert: GPIO35-37 (Octal-PSRAM), GPIO19/20 (natives USB)

### Empfänger (XIAO ESP32C3)
- Potis (alle ADC1!): Lautstärke=D0/GPIO2 (Fußpunkt an D4!), Helligkeit=D1/GPIO3, Lüfter=D2/GPIO4
- POTI_GND=D4/GPIO6 (in setup() früh OUTPUT LOW — Strapping-Fix GPIO2)
- Piezo=D3/GPIO5 (LEDC), Lüfter-PWM=D6/GPIO21 (LEDC 25 kHz)
- Debug-Taster=D7/GPIO20, Status-LED=D9/GPIO9 (**aktiv LOW** — Strapping-Fix), WS2812B=D10/GPIO10
- Frei: D5/GPIO7, D8/GPIO8

### V2 (Legacy)
- Sender: NRF24 CE=D9/CSN=D8; Display CS=A0/DC=D10/RST=A1; Buttons D5/D6/D7; Buzzer D4; Battery A4
- Empfänger: NRF24 CE=D9/CSN=D8; LED-Strip D3; Buzzer D4; Debug D7/D2; Status-LEDs A2-A4

## Recent Changes
- 005-ota-maintenance-mode: Added C++17, Arduino core for ESP32 (arduino-esp32 3.3.7) + WiFi, ArduinoOTA, ESPmDNS (Arduino-ESP32 built-ins), FastLED (already used); ESP-NOW via `esp_now`/`esp_wifi` (normal path only)
- 2026-06-11: **V3-Firmware implementiert** (`SenderV3/`, `EmpfaengerV3/`): ESP-NOW-Transport,
  e-Paper-UI, Soft-Power, NVS, Lokalregler — beide Firmwares bauen mit PlatformIO
  (arduino-esp32 3.3.7); HIL-Abnahme offen (quickstart.md #1-#21)
- 004-v3-esp32-port: Added C++ (Arduino core for ESP32 / arduino-esp32 3.x, C++17; V2 logic is C++11 and ports without changes)


<!-- MANUAL ADDITIONS START -->
Kommunikationssprache ist deutsch, informell und immer per Du
<!-- MANUAL ADDITIONS END -->
