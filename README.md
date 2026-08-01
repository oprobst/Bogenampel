# Bogenampel

Eine funkgesteuerte Timer-Anzeige für Bogenschießplätze.
Sie funktioniert möglichst einfach, mit minimalen Benutzereingaben:
An der Bedieneinheit wird der Modus (120 oder 240 Sekunden, 1-2 oder 3-4 Schützen)
vorausgewählt, danach steuern zwei Taster den kompletten Turnierablauf.

> **Aktuelle Hardware-Generation: V3 (ESP32)** — siehe [Version 3](#version-3-aktuell).
> Die Arduino-Nano-Generation V2 wurde am 2026-08-01 vollständig aus dem
> Arbeitsverzeichnis entfernt (Firmware, Bibliotheken, KiCad) und ist nur noch
> über die Git-Historie auffindbar. Die weiter unten stehende V2-Beschreibung
> bleibt als Dokumentation erhalten — beachte aber, dass die Ordnernamen
> `Sender/` und `Empfaenger/` seither die **V3**-Firmware enthalten.

## Version 3 (aktuell)

| | Sender (Bedieneinheit) | Empfänger (Anzeigeeinheit) |
|---|---|---|
| **Controller** | ESP32-S3-WROOM-1U-N16R8 | Seeed XIAO ESP32C3 |
| **Anzeige** | 1.54″ e-Paper (SSD1681, 200×200) | WS2812B-Strip, 158 LEDs (unverändert) |
| **Funk** | ESP-NOW (integriert, Kanal 1) | ESP-NOW (integriert, Kanal 1) |
| **Versorgung** | LiPo + MCP73837-Lader, USB-C | 5 V USB oder 12 V (Jumper); LED-Strip immer extern |
| **Bedienung** | 2 Taster: CONFIG (schaltet ein, Weiter/Ändern), OK (Bestätigen, 2 s = Alarm, 3 s = Aus) | Debug-Taster, 3 Potis (Lautstärke, Helligkeit, Lüfter) |
| **Firmware** | `Sender/` | `Empfaenger/` |
| **Schaltplan** | `Schaltung-Sender/BogenampelV3.kicad_sch` | `Schaltung-Empfaenger/Empfaenger.kicad_sch` |

Neu in V3 gegenüber V2:

- **ESP-NOW statt NRF24**: kein Funkmodul mehr, Discovery zur Laufzeit (kein Pairing),
  6-Byte-Frames mit magic + Checksumme + Sequenznummer (Dedup bei Retries)
- **One-Button-Power**: die CONFIG-Taste schaltet ein (Power-Latch), 3 s halten auf einer der beiden Tasten schaltet geordnet ab;
  Konfiguration bleibt in NVS erhalten
- **e-Paper-UI**: 1-Hz-Countdown per Partial-Refresh (kein Vollbild-Blitzen),
  Statuszeile mit Akku-/Lade-/Funkstatus
- **Lokalregler am Empfänger**: Lautstärke- (D0), Helligkeits- (D1) und Lüfter-Poti (D2)
  wirken live; Lüfter-PWM 25 kHz
- **Unverändert**: die 11 Funk-Kommandos, der WA-Timer-Ablauf, der Gruppen-4-Zyklus und
  das **autonome Passenende des Empfängers** (Zeitablauf braucht keinen Funk)

Build & Flash, Hardware-Voraussetzungen und die komplette Abnahme-Checkliste:
[`specs/004-v3-esp32-port/quickstart.md`](specs/004-v3-esp32-port/quickstart.md).
Verbindliche Pin-Belegung: [`specs/004-v3-esp32-port/contracts/hardware-pins.md`](specs/004-v3-esp32-port/contracts/hardware-pins.md).

```bash
# PlatformIO (einziger unterstützter Build-Pfad, Libraries via lib_deps)
pio run -t upload -e sender      # ESP32-S3, natives USB (BTN1 halten!)
pio run -t upload -e empfaenger  # XIAO ESP32C3
```

---

## Version 2 (Legacy: Arduino Nano + NRF24)

Basierend auf zwei Arduino Nanos mit nRF24L01+ Funkmodulen.

> **Nicht mehr im Arbeitsverzeichnis.** Firmware (`Sender/`, `Empfaenger/`),
> Bibliotheken (`libraries/`) und KiCad-Projekt (`Schaltung/`) wurden am
> 2026-08-01 entfernt; letzter Stand: Commit `e632bfb`. Die folgende
> Beschreibung dokumentiert die Hardware, die so noch gebaut ist.
> Zum Nachschlagen: `git show e632bfb:Sender/Sender.ino`

## Projektbeschreibung

Die Bogenampel ermöglicht eine sichere Kontrolle des Schießbetriebs auf Bogenschießplätzen. Ein Arduino (Sender/Fernbedienung) steuert per Funk die Timer-Anzeige am Ziel (Empfänger), die den Schützen durch farbige Countdown-Anzeige signalisiert, wann geschossen werden darf und wann die Scheiben sicher gewechselt werden können.

### Bedienkonzept
Die Bogenampel besteht aus zwei Komponenten: 

1. **Bedieneinheit (Sender)**
   - Liegt an der Schießlinie
   - Ermöglicht das Starten und Stoppen des Timers per Taster
   - Sendet Steuerbefehle per Funk an die Anzeigeeinheit

2. **Anzeigeeinheit (Empfänger)**
   - Zeigt die verbleibende Zeit auf 3x 7-Segment-Anzeige (in Sekunden)
   - Visualisiert den Status durch Ampelfarben:
     - **Rot**: Schießen verboten 10 Sekunden (Schützen dürfen an die Startlinie treten)
     - **Grün**: Schießen erlaubt (Timer zählt von 120 oder 240 Sekunden herunter)
     - **Gelb**: Schießen endet bald (30 Sekunden vor Ablauf der Zeit)
     - **Rot**: Schießen verboten (Timer gestoppt / abgelaufen), Anzeige: 000
   - Akustische Signale über Piezo-Buzzer

#### Spannungsversorgung

**Sender:**
- 9V Block-Batterie (über Ein/Aus-Schalter)
- Alternativ: 5V USB-Kabel

**Empfänger:**
- USB Powerbank (5V)
- Beim Programmieren über USB: Development-Mode-Jumper setzen (reduziert LED-Helligkeit)

#### Funktionen

**Sender**
* Ein/Aus-Schalter für 9V Block-Batterie
* 3x Taster für Menü-Navigation und Timer-Steuerung:
  - Links (J1): Menü-Navigation / Gruppe wechseln
  - OK (J2): Auswahl bestätigen / Timer starten/stoppen / Alarm (2s gedrückt halten)
  - Rechts (J3): Menü-Navigation / Halbe Passe
* TFT-Display (ST7789, 240x320) zeigt Menüs, Timer-Status, Batterie, Gruppen
* Buzzer für akustisches Feedback bei Tastendruck
* Status-LED (D1) zeigt Betriebsbereitschaft

**Empfänger**
* **WS2812B LED-Strip**:
  - 16 LEDs für Gruppe A/B (mit 4-State Positionsanzeige)
  - 16 LEDs für Gruppe C/D (mit 4-State Positionsanzeige)
  - 3x 7-Segment-Anzeige (126 LEDs) für Timer-Countdown in Ampelfarben
  - **Total: 158 LEDs**
* **Development-Mode-Jumper (D2)**:
  - Jumper gesetzt (D2 → GND): LED-Helligkeit auf 25% begrenzt für USB-Programmierung
  - Jumper offen: Volle LED-Helligkeit (100%) für Powerbank-Betrieb
* **Piezo-Buzzer (KY-006)**: Akustische Signale bei Timer-Start, Warnung und Ablauf
* **Debug-Taster (D7)**: Für Entwicklungs- und Testzwecke
* Status-LEDs (Grün, Gelb, Rot) zeigen Betriebsbereitschaft und Timer-Status

## Hardware-Komponenten

### Sender (Fernbedienung)
- 1x Arduino Nano
- 1x ST7789 TFT Display (240x320) mit TXS0108EPW Level Shifter
- 1x nRF24L01+ Funkmodul (2.4 GHz) mit PCB-Antenne
- 3x Taster für Menü-Navigation (J1, J2, J3)
- 1x KY-006 passiver Piezo-Buzzer für Tastentöne
- 1x Ein/Aus-Schalter
- 1x Status-LED mit 330Ω Vorwiderstand
- 1x 10µF Kondensator für nRF24-Stabilisierung
- 1x Spannungsteiler (10kΩ:10kΩ) für Batterie-Überwachung
- Stromversorgung: 9V Block-Batterie oder USB

### Empfänger (Anzeigeeinheit)
- 1x Arduino Nano
- 1x nRF24L01+ Funkmodul (2.4 GHz) mit PCB-Antenne
- 1x WS2812B LED-Strip (158 LEDs total):
  - 16 LEDs für Gruppe A/B
  - 16 LEDs für Gruppe C/D
  - 126 LEDs für 3x 7-Segment-Anzeige (je 42 LEDs pro Digit)
- 1x KY-006 passiver Piezo-Buzzer
- 1x Debug-Taster
- 1x Development-Mode-Jumper (D2)
- 3x Status-LEDs (Grün, Gelb, Rot) mit 330Ω Vorwiderständen
- 1x 330Ω Schutzwiderstand für LED-Datenleitung
- 1x 1000µF Kondensator für LED-Stromversorgung
- 1x 10µF Kondensator für nRF24-Stabilisierung
- Stromversorgung: USB Powerbank (5V, bis zu 12A bei voller Helligkeit)

## Pin-Belegung

### Sender
| Pin | Funktion |
|-----|----------|
| D9 | nRF24 CE |
| D8 | nRF24 CSN |
| D13 | SPI SCK (nRF24 + Display) |
| D11 | SPI MOSI (nRF24 + Display) |
| D12 | SPI MISO (nRF24 + Display) |
| A0 | TFT CS |
| D10 | TFT DC |
| A1 | TFT RST |
| D5 | Taster Links (J1) - mit Pull-up |
| D6 | Taster OK (J2) - mit Pull-up |
| D7 | Taster Rechts (J3) - mit Pull-up |
| D4 | Buzzer (KY-006) |
| A2 | Status-LED (Rot) |
| A5 | Batteriespannung (Spannungsteiler 1:1) |
| 3.3V | nRF24 VCC + Display (via Level Shifter) |
| 5V/VIN | Ein/Aus-Schalter |

### Empfänger
| Pin | Funktion |
|-----|----------|
| D9 | nRF24 CE |
| D8 | nRF24 CSN |
| D13 | SPI SCK (nRF24) |
| D11 | SPI MOSI (nRF24) |
| D12 | SPI MISO (nRF24) |
| D3 | WS2812B LED Strip Datenleitung (158 LEDs) |
| D4 | KY-006 Buzzer Signal |
| D7 | Debug-Taster - mit Pull-up |
| D2 | Development-Mode-Jumper - mit Pull-up |
| A2 | Status-LED Grün |
| A3 | Status-LED Gelb |
| A4 | Status-LED Rot |
| 3.3V | nRF24 VCC |
| 5V | LED-Strip + Buzzer + USB Powerbank |

## Funktionsweise

### Timer-Phasen und Ampelfarben
1. **Rot (Stop)**: Timer gestoppt oder abgelaufen - Scheiben können sicher gewechselt werden
2. **Gelb (Vorbereitung)**: 10 Sekunden Countdown zur Vorbereitung - Schützen bereit machen
3. **Grün (Aktiv)**: 120 oder 240 Sekunden Countdown - Schießen erlaubt

### Akustische Signale
- Start der Vorbereitung (10s): Tiefer Ton (1500 Hz)
- Start des Countdowns: Hoher Ton (2500 Hz)
- Timer abgelaufen: Mittlerer Ton (2000 Hz)

### Kommunikation
Die Kommunikation zwischen Sender und Empfänger erfolgt über nRF24L01+ Funkmodule auf 2.4 GHz:
- Reichweite: ~20-50m (indoor), bis 100m (Freifeld)
- Kanal: 76 (2.476 GHz)
- Datenrate: 250 kbps (robust bei langen Kabeln)
- Auto-ACK aktiviert für Verbindungskontrolle
- Paketgröße: 2 Bytes (Command + Checksumme)

**Übertragene Befehle (11 Kommandos):**
- `CMD_STOP` - Timer stoppen
- `CMD_START_120` - Timer starten (120s + 10s Vorbereitung)
- `CMD_START_240` - Timer starten (240s + 10s Vorbereitung)
- `CMD_INIT` - Empfänger initialisieren (Turnier-Start)
- `CMD_ALARM` - Not-Alarm auslösen (blinkt 8x rot/gelb)
- `CMD_PING` - Verbindungstest
- `CMD_GROUP_AB` / `CMD_GROUP_CD` - Gruppe wechseln (ganze Passe)
- `CMD_GROUP_NONE` - Keine Gruppe (1-2 Schützen Modus)
- `CMD_GROUP_FINISH_AB` / `CMD_GROUP_FINISH_CD` - Halbe Passe starten

### Development Mode
Beim Programmieren des Empfängers über USB muss der Development-Mode-Jumper gesetzt werden:
- **Jumper gesetzt (D2 → GND)**: LED-Helligkeit wird auf 25% reduziert 
- **Jumper offen**: Volle LED-Helligkeit 100% 

**Wichtig:** Standard-USB-Ports können maximal 0.5-3A liefern. Die volle LED-Helligkeit würde den Port überlasten und kann zu Schäden führen!

## Projektstruktur