# Bogenampel Development Guidelines

Auto-generated from all feature plans. Last updated: 2025-12-25

## Project Overview

Bogenampel ist eine funkgesteuerte Timer-Anzeige für Bogenschießplätze mit:
- **Sender (Bedieneinheit)**: Arduino Nano + ST7789 TFT Display + NRF24L01
- **Empfänger (Anzeigeeinheit)**: Arduino Nano + WS2812B LED Strip (158 LEDs) + NRF24L01
- **Kommunikation**: 2.4 GHz Funk, 250 kbps, 11 Kommandos
- **Features**: Timer-Steuerung, Gruppen-Anzeige, Alarm-System, EEPROM-Konfiguration

## Active Technologies

- **C++** (Arduino, C++11 compatible with Arduino IDE/PlatformIO)
- **Libraries**:
  - Adafruit ST7789 (Display)
  - Adafruit GFX (Graphics)
  - RF24 (NRF24L01 Funk)
  - FastLED (WS2812B LED Strip)

## Project Structure

```text
Sender/               # Bedieneinheit (362 LOC ino, 2067 LOC cpp/h)
  ├── Sender.ino      # Main file
  ├── Config.h        # Pin definitions, constants, EEPROM config
  ├── StateMachine.*  # 5 States (Splash, Config, Schießbetrieb, Pfeile Holen, Alarm)
  ├── ButtonManager.* # 3 Buttons mit Debouncing + Buzzer
  ├── *Menu.*         # ConfigMenu, SchiessBetriebMenu, PfeileHolenMenu
  └── AlarmScreen.*   # Alarm UI

Empfaenger/           # Anzeigeeinheit (961 LOC ino, 293 LOC cpp/h)
  ├── Empfaenger.ino  # Main file
  ├── Config.h        # Pin definitions, LED strip config (158 LEDs)
  ├── DisplayManager.*# 7-Segment Display + Gruppen-Anzeige
  └── BuzzerManager.* # Akustische Signale

Schaltung/            # KiCad Schaltplan und PCB
TestSender/           # Test-Programme
TestEmpfaenger/
libraries/            # Externe Libraries (Adafruit, RF24, FastLED)
specs/                # Feature-Spezifikationen
```

## Commands

### Build & Upload (Arduino IDE)
1. Open `Sender/Sender.ino` or `Empfaenger/Empfaenger.ino`
2. Select Board: Arduino Nano
3. Select Processor: ATmega328P (Old Bootloader)
4. Install Libraries: Adafruit ST7789, Adafruit GFX, RF24, FastLED
5. Upload

### Build & Upload (PlatformIO)
```bash
cd Sender
pio run -t upload
pio device monitor
```

## Code Style

- **C++ (Arduino, C++11)**: Follow standard conventions
- **Naming**:
  - Classes: PascalCase (StateMachine, ButtonManager)
  - Functions: camelCase (updateDisplay, handleButton)
  - Constants: UPPER_SNAKE_CASE (LED_PIN, MAX_RETRY)
  - Namespaces: PascalCase (Pins, Display, RF, Battery)
- **Files**: Flat structure (no subdirs) for Arduino IDE compatibility
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

## Hardware Pin-Belegung (aktuell)

### Sender
- NRF24: CE=D9, CSN=D8
- Display: CS=A0, DC=D10, RST=A1
- Buttons: Links=D5, OK=D6, Rechts=D7
- Buzzer: D4
- Battery: A5 (Spannungsteiler 1:1)

### Empfänger
- NRF24: CE=D9, CSN=D8
- LED Strip: D3 (158 LEDs)
- Buzzer: D4
- Debug: Taster=D7, Jumper=D2
- Status-LEDs: Grün=A2, Gelb=A3, Rot=A4

## Recent Changes

- 2025-12-25: Dokumentation aktualisiert (README, HARDWARE, Sender/README)
- 2025-12-22: Alarm-System mit Pfeiltasten-Trigger implementiert
- 2025-12-22: Interrupt-basierte Timer-Synchronisation
- 2025-12-21: Halbe Passe-Funktionalität und Code-Refactoring
- 2025-12-20: 7-Segment LED-Display und Gruppen-Anzeige
- 001-benutzer-fuehrung: Added C++ (Arduino, C++11 compatible)

<!-- MANUAL ADDITIONS START -->
Kommunikationssprache ist deutsch, informell und immer per Du
<!-- MANUAL ADDITIONS END -->
