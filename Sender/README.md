# Bogenampel Sender (Bedieneinheit)

Sender-Software für die Bogenampel auf Basis eines Arduino Nano V3.

## Hardware

- **Mikrocontroller**: Arduino Nano V3 (ATmega328P @ 16 MHz)
- **Display**: ST7789 TFT LCD (240x320) über TXS0108EPW Level Shifter
- **Funk**: NRF24L01 (2.4 GHz)
- **Eingänge**: 3x Taster (Start/Stop, Gruppen-Toggle, Menü)
- **Ausgänge**: 3x Status-LEDs (Grün, Gelb, Rot)
- **Stromversorgung**: 9V Block (schaltbar) oder USB

Detaillierte Pin-Belegung: siehe [HARDWARE.md](../HARDWARE.md)

## Projektstruktur

```
Sender/
├── Sender.ino              # Hauptdatei mit setup() und loop()
├── Config.h                # Zentrale Konfiguration (Pins, Konstanten)
├── Commands.h              # RF-Kommando-Definitionen
├── SplashScreen.h/cpp      # Startup-Logo
├── HARDWARE.md             # Pin-Belegung und Hardware-Dokumentation
├── SETUP.md                # Setup-Anleitung
└── README.md               # Diese Datei

Hinweis: Flat-File-Structure für Arduino IDE Kompatibilität (keine Unterordner)
```

## Features (basierend auf Spezifikationen)

### ✅ Implementiert (Version 1.0)

- [ ] **Splash Screen** (003-startup-logo-splash)
  - Logo und "Bogenampeln V1.0" für 3 Sekunden
  - Überspringen mit beliebiger Taste

- [ ] **Batteriemonitor** (001-battery-monitoring-display)
  - Spannungsmessung über A7 (1:1 Spannungsteiler)
  - Median-Filter (5 Werte) zur Glättung
  - Anzeige in % oder USB-Symbol
  - Low-Battery-Warnung bei <20%

- [ ] **Timer-Steuerung** (001-archery-timer)
  - START: 10s Vorbereitung + 120/240s Countdown
  - STOP: Timer sofort anhalten
  - RF-Übertragung via NRF24L01

- [ ] **Gruppen-Anzeige** (002-shooter-groups)
  - Toggle zwischen A/B und C/D
  - 4-State Cycle mit Position-Indikator
  - Anzeige auf Display

### 🚧 Geplant (Version 2.0)

- [ ] Menü-System für Einstellungen
- [ ] Lautstärke-Regelung für Empfänger-Buzzer
- [ ] Speichern von Einstellungen im EEPROM
- [ ] Batterie-Kalibrierung

## Abhängigkeiten (Libraries)

### Erforderlich

- **Adafruit ST7735 and ST7789 Library** - ST7789 Display
  - Installation: Arduino IDE Library Manager → "Adafruit ST7735 and ST7789 Library"
  - PlatformIO: `adafruit/Adafruit ST7735 and ST7789 Library@^1.10.0`

- **Adafruit GFX Library** - Grafik-Grundfunktionen
  - Installation: Arduino IDE Library Manager → "Adafruit GFX Library"
  - PlatformIO: `adafruit/Adafruit GFX Library@^1.11.0`

- **RF24** (v1.4.0+) - NRF24L01 Funkmodul
  - Installation: Arduino IDE Library Manager → "RF24"
  - PlatformIO: `nRF24/RF24@^1.4.0`

## Kompilierung

### Mit PlatformIO (empfohlen)

```bash
cd Sender
pio run                # Kompilieren
pio run -t upload      # Upload zum Arduino
pio device monitor     # Serial Monitor
```

### Mit Arduino IDE

1. Öffne `Sender.ino`
2. Wähle Board: **Arduino Nano**
3. Wähle Prozessor: **ATmega328P (Old Bootloader)** oder **ATmega328P**
4. Installiere erforderliche Libraries über Library Manager
5. Kompiliere und lade hoch

## Konfiguration

### Display (Adafruit ST7789)

**Keine manuelle Konfiguration nötig!** Die Pins werden direkt im Code festgelegt:

```cpp
// In Sender.ino
Adafruit_ST7789 tft = Adafruit_ST7789(Pins::TFT_CS, Pins::TFT_DC, Pins::TFT_RST);
```

### Pins

Alle Pin-Definitionen in `Config.h` anpassen (bereits für Hardware konfiguriert).

## Debugging

Serial-Debug-Ausgaben aktivieren in `Config.h`:

```cpp
#define DEBUG_ENABLED 1  // 1 = an, 0 = aus
```

Debug-Ausgabe über USB-Serial (115200 Baud):
```cpp
DEBUG_PRINTLN("Sender gestartet");
DEBUG_PRINT("Batterie: "); DEBUG_PRINT(percent); DEBUG_PRINTLN("%");
```

## Speicherverbrauch

**Geschätzt** (Arduino Nano: 32 KB Flash, 2 KB SRAM):

- Flash: ~28-30 KB (ca. 90%)
- SRAM: ~500-700 Bytes (ca. 30%)

**Optimierungen bei knappem Speicher:**
- `#define DEBUG_ENABLED 0` in Config.h
- `#define RF24_TINY` in RF24-Library
- Compiler-Flag `-Os` (Size-Optimierung)

## State Machine

Die Anwendung ist als State Machine implementiert:

```
SPLASH_SCREEN → IDLE ⇄ TRANSMITTING
                  ↓
            LOW_BATTERY_WARNING
```

- **SPLASH_SCREEN**: Zeigt Logo für 3 Sekunden
- **IDLE**: Wartet auf Benutzereingabe, zeigt Status
- **TRANSMITTING**: Sendet RF-Kommando, zeigt Feedback
- **LOW_BATTERY_WARNING**: Warnung bei <20% Batterie

## RF-Protokoll

Siehe `Commands.h` für Details.

**Paket-Format** (4 Bytes):
- Byte 0: Kommando (START_TIMER, STOP_TIMER, TOGGLE_GROUP)
- Byte 1: Daten (z.B. Gruppenstatus)
- Byte 2: Sequenznummer
- Byte 3: XOR-Checksumme

## Testing

### Hardware-Tests

1. **Display**: Splash Screen sollte erscheinen
2. **Taster**: LEDs sollten auf Tastendruck reagieren
3. **Batteriemonitor**: Prozentanzeige sollte realistisch sein
4. **RF**: Empfänger sollte Kommandos empfangen

### Serial-Debug

```cpp
// In loop() oder StateMachine::update()
DEBUG_PRINTLN("=== SENDER STATUS ===");
DEBUG_PRINT("State: "); DEBUG_PRINTLN(currentState);
DEBUG_PRINT("Battery: "); DEBUG_PRINT(batteryPercent); DEBUG_PRINTLN("%");
DEBUG_PRINT("Group: "); DEBUG_PRINTLN(currentGroup);
```

## Lizenz

Siehe Haupt-Repository.

## Kontakt

Siehe Haupt-README.md
