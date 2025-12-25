# Hardware-Spezifikation: Bogenampel Sender

## Übersicht

Der Sender ist die Bedieneinheit der Bogenampel und basiert auf einem Arduino Nano V3. Er steuert per Funk (NRF24L01) die Anzeigeeinheit und verfügt über ein TFT-Display zur Statusanzeige.

## Komponenten

### Mikrocontroller
- **Arduino Nano V3**
  - ATmega328P @ 16 MHz
  - 32 KB Flash (2 KB für Bootloader)
  - 2 KB SRAM
  - 1 KB EEPROM
  - Betriebsspannung: 5V
  - I/O-Pins: 3.3V/5V tolerant (über Level Shifter für Display)

### Display
- **2.4" TFT LCD ST7789**
  - Auflösung: 240 x 320 Pixel
  - Farbtiefe: 65K (16-bit RGB565)
  - Interface: SPI
  - Betriebsspannung: 3.3V (via Level Shifter)
  - Controller: ST7789

### Level Shifter
- **TXS0108EPW**
  - 8-Kanal bidirektionaler Level Shifter
  - Konvertiert 5V (Arduino) ↔ 3.3V (Display)
  - Kanäle für: SCK, MOSI, MISO, CS, DC, RST

### Funk-Modul
- **NRF24L01**
  - 2.4 GHz Transceiver
  - Reichweite: ~20-50m (indoor), bis 100m (Freifeld)
  - Interface: SPI
  - Betriebsspannung: 3.3V
  - Datenstrom 1-2 Mbps

### Spannungsversorgung
- **9V Block-Batterie**
  - Über Ein/Aus-Schalter zuschaltbar
  - Spannungsüberwachung über Spannungsteiler an A7
  - Spannungsteiler: 2x 10kΩ (1:1)

- **USB-Versorgung (5V)**
  - Alternative zur Batterie
  - Für Entwicklung und Programmierung

### Weitere Komponenten
- **Taster** (für Timer-Steuerung und Menü-Navigation)
- **Status-LEDs** (Betriebsanzeige)
- **Kondensatoren** (Spannungsstabilisierung für NRF24L01)

---

## Pin-Belegung Arduino Nano

> **WICHTIG:** Die folgenden Pin-Zuweisungen basieren auf typischen SPI-Konfigurationen.
> Bitte überprüfen Sie diese anhand des KiCad-Schaltplans `Schaltung/Bogenampel.kicad_sch` und korrigieren Sie bei Bedarf!

### SPI-Bus (Gemeinsam für Display und NRF24L01)

| Arduino Pin | Signal | Funktion | Level Shifter | Ziel |
|-------------|--------|----------|---------------|------|
| D13 (SCK) | SPI.SCK | SPI Clock | → 3.3V | Display SCK, NRF24 SCK |
| D11 (MOSI) | SPI.MOSI | SPI Data Out | → 3.3V | Display MOSI (SDI), NRF24 MOSI |
| D12 (MISO) | SPI.MISO | SPI Data In | ← 3.3V | Display MISO (SDO), NRF24 MISO |

### ST7789 TFT Display (über TXS0108EPW Level Shifter)

| Arduino Pin | Signal | Display Pin | Funktion | Level |
|-------------|--------|-------------|----------|-------|
| A0 | UI_CS → DSPL_CS | CS | Chip Select | 5V → 3.3V |
| D10 | UI_DC/RS → DSPL_DC/RS | DC/RS | Data/Command | 5V → 3.3V |
| A1 | UI_RES → DSPL_RES | RESET | Reset | 5V → 3.3V |
| D13 | SPI.SCK → DSPL_SCK | SCK | SPI Clock | 5V → 3.3V |
| D11 | SPI.MOSI → DSPL_MOSI | SDA | SPI Data Out | 5V → 3.3V |
| D12 | SPI.MISO → DSPL_MISO | - | SPI Data In | 5V → 3.3V |

**Aktualisiert für ST7789:**
- A0  → UI_CS → TXS0108 → DSPL_CS (Display Chip Select)
- D10 → UI_DC/RS → TXS0108 → DSPL_DC/RS (Data/Command)
- A1  → UI_RES → TXS0108 → DSPL_RES (Reset)
- Backlight: Fest an 3.3V (kein PWM-Control)

### NRF24L01 Funkmodul

| Arduino Pin | Signal | NRF24 Pin | Funktion | Level |
|-------------|--------|-----------|----------|-------|
| D9 | CE_NRF | CE (Pin 3) | Chip Enable | 5V → 3.3V intern |
| D8 | CS_NRF | CSN (Pin 4) | Chip Select | 5V → 3.3V intern |
| D13 | SPI.SCK | SCK (Pin 5) | SPI Clock | Gemeinsam mit Display |
| D11 | SPI.MOSI | MOSI (Pin 6) | SPI Data Out | Gemeinsam mit Display |
| D12 | SPI.MISO | MISO (Pin 7) | SPI Data In | Gemeinsam mit Display |
| 3.3V | VCC | VCC (Pin 2) | Stromversorgung | 3.3V |
| GND | GND | GND (Pin 1) | Masse | 0V |

**Verifiziert aus Code (Sender/Config.h):**
- D9 → CE_NRF (Chip Enable)
- D8 → CS_NRF (Chip Select)
- 10µF Kondensator (C1) zwischen VCC und GND für Stabilisierung

### Eingänge (Taster & Schalter)

| Arduino Pin | Signal | Funktion | Pull-Up | Bemerkung |
|-------------|--------|----------|---------|-----------|
| D5 | Input 1 (J1) | Menü-Navigation links | Ja | Aktiv LOW |
| D6 | Input 2 (J2) | Menü-Auswahl bestätigen (OK) | Ja | Aktiv LOW |
| D7 | Input 3 (J3) | Menü-Navigation rechts | Ja | Aktiv LOW |

**Verifiziert aus Code (Sender/Config.h):**
- J1 (Input 1) → D5 mit internem Pull-Up (Links)
- J2 (Input 2) → D6 mit internem Pull-Up (OK)
- J3 (Input 3) → D7 mit internem Pull-Up (Rechts)
- Alle Taster sind aktiv LOW (Taster schließt gegen GND)

### Analoge Eingänge

| Arduino Pin | Signal | Funktion | Spannungsteiler | Bemerkung |
|-------------|--------|----------|-----------------|-----------|
| A5 | VOLTAGE_SENSE | Batteriespannung | 10kΩ:10kΩ (1:1) | Median-Filter über 5 Werte |

**Berechnung Batteriespannung:**
```cpp
// Spannungsteiler 1:1 → Vbat = 2 × Vmeasured
uint16_t adcValue = analogRead(A5);
float voltage = (adcValue / 1023.0) * 5.0 * 2.0;  // in Volt
uint8_t percent = map(voltage, 6.0, 9.6, 0, 100); // 6V=0%, 9.6V=100%
```

### Status-LEDs

| Arduino Pin | Signal | Funktion | Widerstand | Bemerkung |
|-------------|--------|----------|------------|-----------|
| A2 | LED_STATUS (D1) | Debug/Status LED | 330Ω | Rot |

**Aktualisiert:**
- D1 (rot) → A2 über 330Ω Widerstand
- Nur eine LED bestückt, verwendet für Debug-Zwecke (z.B. RF-Übertragung)

### Buzzer (Akustisches Feedback)

| Arduino Pin | Signal | Funktion | Bemerkung |
|-------------|--------|----------|-----------|
| D4 | BUZZER | KY-006 Passiver Piezo Buzzer | Für Tastentöne (Click-Feedback) |

**Details:**
- Frequenz: 1600 Hz (satter Klick-Ton)
- Dauer: 25ms pro Klick
- Wird beim Drücken von Tasten aktiviert (ButtonManager)

### Stromversorgung

| Pin | Funktion | Spannung | Bemerkung |
|-----|----------|----------|-----------|
| VIN | Batterie-Eingang | 9V | Über Schalter |
| 5V | Spannungsausgang | 5V | Geregelt durch Nano |
| 3.3V | Spannungsausgang | 3.3V | Für NRF24L01 |
| GND | Masse | 0V | Gemeinsam für alle Komponenten |

---

## SPI-Bus Konfiguration

Der Arduino Nano nutzt **Hardware-SPI** für Display und NRF24L01:

### SPI-Einstellungen ST7789
```cpp
SPISettings displaySPI(40000000, MSBFIRST, SPI_MODE0);
// 40 MHz (max für ST7789), MSB First, Mode 0
```

### SPI-Einstellungen NRF24L01
```cpp
SPISettings nrfSPI(10000000, MSBFIRST, SPI_MODE0);
// 10 MHz (max für NRF24L01), MSB First, Mode 0
```

### Chip Select Management
Da Display und NRF24L01 den gleichen SPI-Bus teilen, ist ein sauberes CS-Management kritisch:

```cpp
// Display aktivieren
digitalWrite(DSPL_CS, LOW);
SPI.beginTransaction(displaySPI);
// ... Display-Kommunikation ...
SPI.endTransaction();
digitalWrite(DSPL_CS, HIGH);

// NRF24 aktivieren
digitalWrite(CS_NRF, LOW);
SPI.beginTransaction(nrfSPI);
// ... NRF-Kommunikation ...
SPI.endTransaction();
digitalWrite(CS_NRF, HIGH);
```

---

## Level Shifter Details (TXS0108EPW)

Der TXS0108EPW konvertiert bidirektional zwischen 5V (Arduino) und 3.3V (Display):

| Kanal | Arduino (5V) | Display (3.3V) | Signal |
|-------|--------------|----------------|--------|
| A1 | DSPL_SCK | SCK | SPI Clock |
| A2 | DSPL_MOSI | MOSI/SDI | SPI Data Out |
| A3 | DSPL_MISO | MISO/SDO | SPI Data In |
| A4 | DSPL_CS | CS | Chip Select |
| A5 | DSPL_DC | DC/RS | Data/Command |
| A6 | DSPL_RST | RESET | Reset |

**Wichtig:**
- VCCB = 5V (Arduino-Seite)
- VCCA = 3.3V (Display-Seite)
- OE (Output Enable) = VCCA (immer aktiviert)

---

## Ressourcen-Übersicht

### SRAM-Nutzung (Arduino Nano: 2 KB)

| Komponente | Geschätzter Verbrauch | Bemerkung |
|------------|----------------------|-----------|
| Display-Buffer (TFT_eSPI) | ~200-400 Bytes | Sprite-Buffer optional |
| NRF24 Library | ~150 Bytes | RF24 Library |
| State Machine | ~50 Bytes | Enum + Zeitstempel |
| Median Filter (5 Werte) | 10 Bytes | uint16_t × 5 |
| Button Manager | ~20 Bytes | 2 Buttons |
| String Konstanten (PROGMEM) | 0 Bytes (Flash) | Im Flash gespeichert |
| **Gesamt (grob)** | **~500-700 Bytes** | ~30% von 2 KB |

### Flash-Nutzung (Arduino Nano: 30 KB nutzbar)

| Komponente | Geschätzter Verbrauch | Bemerkung |
|------------|----------------------|-----------|
| TFT_eSPI Library | ~15-20 KB | Mit ILI9341 Treiber |
| RF24 Library | ~8-10 KB | NRF24L01 |
| Application Code | ~5-8 KB | State Machine, UI |
| **Gesamt (grob)** | **~28-38 KB** | Knapp! Optimierung nötig |

**Optimierungen:**
- `#define TFT_eSPI_DISABLE_ALL_FONTS` - nur benötigte Fonts
- `#define RF24_TINY` - reduzierte RF24-Version
- Strings im PROGMEM (`PROGMEM`)
- `-Os` Compiler-Flag (Size-Optimierung)

---

## Empfohlene Bibliotheken

### Display
- **Adafruit ST7789** (verwendet)
  - Offizielle Adafruit Library
  - Hardware-SPI-Unterstützung
  - ST7789-Treiber integriert
  - `lib_deps = adafruit/Adafruit ST7735 and ST7789 Library@^1.10.0`
  - Benötigt auch: `adafruit/Adafruit GFX Library@^1.11.0`

### Funk
- **RF24** (empfohlen)
  - Optimiert für NRF24L01
  - `lib_deps = nRF24/RF24@^1.4.0`

---

## Schaltplan-Referenz

**KiCad-Dateien:**
- `Schaltung/Bogenampel.kicad_sch` - Hauptschaltplan
- `Schaltung/Bogenampel.kicad_pcb` - PCB-Layout

**Netz-Labels im Schaltplan:**
- `SPI.SCK`, `SPI.MOSI`, `SPI.MISO` - Gemeinsamer SPI-Bus
- `DSPL_CS`, `DSPL_DC`, `DSPL_RST` - Display Control Pins
- `CE_NRF`, `CS_NRF` - NRF24L01 Control Pins

---

## ✅ Pin-Verifikation - Komplett

**Alle Pin-Zuweisungen wurden aus dem Code (Sender/Config.h) verifiziert:**

1. ✅ **ST7789 Display** (über TXS0108EPW Level Shifter):
   - [x] DSPL_CS → A0
   - [x] DSPL_DC/RS → D10
   - [x] DSPL_RES → A1
   - [x] DSPL_LED → 3.3V (fest)

2. ✅ **NRF24L01**:
   - [x] CE_NRF → D9
   - [x] CS_NRF → D8

3. ✅ **Taster**:
   - [x] Input 1 (J1) → D5 (Menü Links)
   - [x] Input 2 (J2) → D6 (Menü OK)
   - [x] Input 3 (J3) → D7 (Menü Rechts)

4. ✅ **Status-LED**:
   - [x] LED 1 (D1 rot) → A2 (Debug/Status)

5. ✅ **Buzzer**:
   - [x] Buzzer → D4 (KY-006 Passiver Piezo)

6. ✅ **Analoge Eingänge**:
   - [x] Batteriespannung → A5 (via 1:1 Spannungsteiler)

7. ✅ **SPI-Bus** (gemeinsam):
   - [x] SCK → D13
   - [x] MOSI → D11
   - [x] MISO → D12

---

## Änderungshistorie

| Datum | Version | Änderung |
|-------|---------|----------|
| 2025-12-13 | 1.0 | Initiale Version mit Platzhalter-Pins |
| 2025-12-13 | 1.1 | Pin-Zuweisungen aus Schaltplan.png verifiziert und aktualisiert |
| 2025-12-13 | 1.2 | Display auf ST7789 korrigiert, Pin-Zuweisungen aktualisiert (TFT_CS→A0, TFT_RST→A1, LEDs→A2/A3/A4) |
| 2025-12-13 | 1.3 | Button-Namen aktualisiert (Links/OK/Rechts), nur LED_RED (A2) bestückt, grüne und gelbe LED entfernt |
| 2025-12-25 | 1.4 | Pin-Zuweisungen aus Code synchronisiert: NRF (D9/D8), Buttons (D5/D6/D7), VOLTAGE_SENSE (A5), Buzzer (D4) hinzugefügt |

---

**Nächste Schritte:**
1. Pin-Zuweisungen aus KiCad-Schaltplan übernehmen
2. Hardware-Tests durchführen
3. Config.h mit korrekten Pins aktualisieren
