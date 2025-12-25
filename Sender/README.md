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
├── Config.h                # Zentrale Konfiguration (Pins, Konstanten, EEPROM)
├── Commands.h              # RF-Kommando-Definitionen (11 Kommandos)
├── StateMachine.h/cpp      # State Machine (5 States, 584 LOC)
├── ButtonManager.h/cpp     # Button-Handling mit Debouncing und Buzzer
├── SplashScreen.h/cpp      # Startup-Logo und Verbindungstest
├── ConfigMenu.h/cpp        # Konfigurations-Menü (340 LOC)
├── SchiessBetriebMenu.h/cpp # Schießbetrieb-Menü (253 LOC)
├── PfeileHolenMenu.h/cpp   # Pfeile-Holen-Menü mit 4-State Cycle (526 LOC)
├── AlarmScreen.h/cpp       # Alarm-Bildschirm (100 LOC)
├── HARDWARE.md             # Pin-Belegung und Hardware-Dokumentation
├── SETUP.md                # Setup-Anleitung
└── README.md               # Diese Datei

Total: 3090 LOC (ohne Libraries)
Hinweis: Flat-File-Structure für Arduino IDE Kompatibilität (keine Unterordner)
```

## Features (basierend auf Spezifikationen)

### ✅ Implementiert (Version 1.0)

- [x] **Splash Screen** (003-startup-logo-splash)
  - Logo und "Bogenampeln V1.0" für 15 Sekunden
  - Überspringen mit beliebiger Taste
  - Verbindungsqualitäts-Test (10 Pings, Anzeige für 5s)

- [x] **Batteriemonitor** (001-battery-monitoring-display)
  - Spannungsmessung über A5 (1:1 Spannungsteiler)
  - Median-Filter (5 Werte) zur Glättung
  - Anzeige in % oder USB-Symbol
  - Low-Battery-Warnung bei <20%

- [x] **Timer-Steuerung** (001-archery-timer)
  - START: 10s Vorbereitung + 120/240s Countdown
  - STOP: Timer sofort anhalten
  - RF-Übertragung via NRF24L01
  - Interrupt-basierte Timer-Synchronisation (Sender ↔ Empfänger)

- [x] **Gruppen-Anzeige** (002-shooter-groups)
  - Toggle zwischen A/B und C/D
  - 4-State Cycle mit Position-Indikator (POS_1, POS_2)
  - Ganze Passe und Halbe Passe Unterstützung
  - Anzeige auf Display und LED-Strip (Empfänger)

- [x] **Menü-System für Einstellungen**
  - Config-Menü: Schießzeit (120/240s), Schützenanzahl (1-2 / 3-4)
  - Schießbetrieb-Menü: Timer-Steuerung, Gruppen-Wechsel
  - Pfeile-Holen-Menü: 4-State Cycle (ganze/halbe Passe)
  - Navigation mit 3 Tastern (Links, OK, Rechts)

- [x] **EEPROM-Konfiguration**
  - Turnier-Einstellungen werden gespeichert (shootingTime, shooterCount)
  - Persistenz über Power-Cycles hinweg
  - CRC8-Checksumme zur Validierung

- [x] **Alarm-System**
  - Auslösung: OK-Taste 2 Sekunden gedrückt halten
  - Sendet CMD_ALARM an Empfänger
  - Empfänger blinkt 8x rot/gelb mit Buzzer-Alarm
  - Alarm-Screen auf Sender-Display

- [x] **Buzzer-Feedback**
  - Tastentöne bei jedem Button-Druck
  - Frequenz: 1600 Hz, Dauer: 25ms
  - Über ButtonManager gesteuert

### 🚧 Geplant (Version 2.0)

- [ ] Batterie-Kalibrierung über Menü
- [ ] Statistiken (Anzahl Durchgänge, Gesamtzeit)

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

Die Anwendung ist als State Machine implementiert (StateMachine.h/cpp):

```
SPLASH_SCREEN → CONFIG_MENU → SCHIESS_BETRIEB ⇄ PFEILE_HOLEN
                                     ↓
                                 ALARM
```

- **STATE_SPLASH_SCREEN**: Zeigt Logo für 15s, führt Verbindungstest durch
- **STATE_CONFIG_MENU**: Einstellungen (Schießzeit, Schützenanzahl)
  - Links/Rechts: Navigation
  - OK: Weiter zu Schießbetrieb
- **STATE_SCHIESS_BETRIEB**: Hauptmenü für Timer-Steuerung
  - OK: Timer Start/Stop
  - Links: Gruppe wechseln (A/B ↔ C/D)
  - Rechts: Halbe Passe (POS_1 ↔ POS_2)
  - OK 2s halten: Alarm auslösen
- **STATE_PFEILE_HOLEN**: 4-State Cycle für Pfeile holen
  - OK: Nächster State
  - Links: Zurück zu Schießbetrieb
- **STATE_ALARM**: Alarm-Screen, sendet CMD_ALARM an Empfänger
  - Automatischer Rückkehr nach 3s

Alle States unterstützen:
- Batterie-Überwachung (Status-Bar oben rechts)
- Gruppen-Anzeige (wenn 3-4 Schützen aktiv)
- Interrupt-basierte Sekunden-Ticks für Timer-Synchronisation

## RF-Protokoll

Siehe `Commands.h` für Details.

**Paket-Format** (2 Bytes):
- Byte 0: Kommando (RadioCommand enum)
- Byte 1: XOR-Checksumme (command ^ 0xFF)

**Verfügbare Kommandos (11 total):**
- `CMD_STOP` (0x01) - Timer stoppen
- `CMD_START_120` (0x02) - Timer 120s starten
- `CMD_START_240` (0x03) - Timer 240s starten
- `CMD_INIT` (0x04) - Empfänger initialisieren
- `CMD_ALARM` (0x05) - Not-Alarm
- `CMD_PING` (0x06) - Verbindungstest
- `CMD_GROUP_AB` (0x08) - Gruppe A/B aktiv (ganze Passe)
- `CMD_GROUP_CD` (0x09) - Gruppe C/D aktiv (ganze Passe)
- `CMD_GROUP_NONE` (0x0A) - Keine Gruppe (1-2 Schützen)
- `CMD_GROUP_FINISH_AB` (0x0B) - Halbe Passe nach A/B
- `CMD_GROUP_FINISH_CD` (0x0C) - Halbe Passe nach C/D

**RF-Konfiguration:**
- Kanal: 76 (2.476 GHz)
- Datenrate: 250 kbps (robust)
- Power: RF24_PA_MIN (Sender) / RF24_PA_HIGH (Empfänger)
- Auto-ACK: aktiviert
- Retry: 15x, Delay 1.5ms

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
