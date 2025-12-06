# Bogenampel

Eine funkgesteuerte Timer-Anzeige für Bogenschießplätze, basierend auf zwei Arduino Nanos mit nRF24L01+ Funkmodulen.
Sie funktioniert möglichst einfach, mit minimalen Benutzereingaben.
An der Anzeigeeinheit wird der Modus (120 oder 240 Sekunden) vorausgewählt.
Die Bedieneinheit übermittelt per Taster das Start oder Stop Signal.

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
   - Einstellbare Lautstärke über Drehregler

#### Spannungsversorgung

**Sender:**
- 9V Block-Batterie (über Ein/Aus-Schalter)
- Alternativ: 5V USB-Kabel

**Empfänger:**
- USB Powerbank (5V)
- Beim Programmieren über USB: Development-Mode-Jumper setzen (reduziert LED-Helligkeit auf 3%)

#### Funktionen

**Sender**
* Ein/Aus-Schalter für 9V Block-Batterie
* Taster (J1) für Timer-Steuerung:
  - Timer gestoppt → Taster drücken: Startet 10-Sekunden-Vorbereitung (rot), dann Countdown 120/240s (grün)
  - Timer läuft → Taster drücken: Stoppt Timer sofort (rot)
* 2x Status-LEDs (D1, D2) zeigen Betriebsbereitschaft

**Empfänger**
* **Modus-Schalter (J1)**: Auswahl zwischen 120 oder 240 Sekunden Countdown
* **Lautstärke-Regler (RV1)**: Potentiometer zur Einstellung der Buzzer-Lautstärke
* **Development-Mode-Jumper (J5)**: 
  - Jumper gesetzt (Pin D2 → GND): LED-Helligkeit auf 3% begrenzt für USB-Programmierung
  - Jumper offen: Volle LED-Helligkeit für Powerbank-Betrieb
* **WS2812B LED-Strip (J4)**: 3x 7-Segment-Anzeige (~155 LEDs) für Timer in Ampelfarben
* **Piezo-Buzzer (KY-006, J3)**: Akustische Signale bei Statuswechsel
* 2x Status-LEDs (D1, D2) zeigen Betriebsbereitschaft

## Hardware-Komponenten

### Sender (Fernbedienung)
- 1x Arduino Nano
- 1x nRF24L01+ Funkmodul (2.4 GHz) mit PCB-Antenne
- 1x Taster für Timer-Steuerung (J1)
- 1x Ein/Aus-Schalter (J2)
- 2x Status-LEDs mit 330Ω Vorwiderständen
- 1x 10µF Kondensator für nRF24-Stabilisierung
- Stromversorgung: 9V Block-Batterie oder USB

### Empfänger (Anzeigeeinheit)
- 1x Arduino Nano
- 1x nRF24L01+ Funkmodul (2.4 GHz) mit PCB-Antenne
- 1x WS2812B LED-Strip (~155 LEDs) für 3x 7-Segment-Anzeige (J4)
- 1x KY-006 passiver Piezo-Buzzer (J3)
- 1x Potentiometer (100Ω) für Lautstärke-Regelung (RV1)
- 1x Modus-Schalter 120/240s (J1)
- 1x Development-Mode-Jumper (J5)
- 2x Status-LEDs mit 330Ω Vorwiderständen
- 1x 330Ω Schutzwiderstand für LED-Datenleitung
- 1x 100Ω Schutzwiderstand für Buzzer-Signal
- 1x 1000µF Kondensator für LED-Stromversorgung
- 1x 10µF Kondensator für nRF24-Stabilisierung
- Stromversorgung: USB Powerbank (5V)

## Pin-Belegung

### Sender
| Pin | Funktion |
|-----|----------|
| D3 | nRF24 CE |
| D4 | nRF24 CSN |
| D5 | nRF24 SCK |
| D6 | nRF24 MOSI |
| D7 | nRF24 MISO |
| D2 | Taster (J1) - mit Pull-up |
| 3.3V | nRF24 VCC + Status-LEDs |
| 5V/VIN | Ein/Aus-Schalter (J2) |

### Empfänger
| Pin | Funktion |
|-----|----------|
| D9 | nRF24 CE |
| D10 | nRF24 CSN |
| D13 | nRF24 SCK |
| D11 | nRF24 MOSI |
| D12 | nRF24 MISO |
| D7 | WS2812B Timer-Strip Datenleitung (J4) |
| D8 | KY-006 Buzzer Signal (J3) |
| D3 | Modus-Schalter 120/240s (J1) - mit Pull-up |
| D2 | Development-Mode-Jumper (J5) - mit Pull-up |
| 3.3V | nRF24 VCC + Status-LEDs |
| 5V | LED-Strips + Buzzer + USB-C Powerbank |

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
- Übertragene Befehle: START, STOP
- Der Empfänger wertet den gewählten Modus (120/240s) lokal aus

### Development Mode
Beim Programmieren des Empfängers über USB muss der Development-Mode-Jumper (J5) gesetzt werden:
- **Jumper gesetzt (D2 → GND)**: LED-Helligkeit wird auf 3% reduziert (~2-3A Stromverbrauch)
- **Jumper offen**: Volle LED-Helligkeit (bis 12A möglich - nur mit Powerbank!)

**Wichtig:** Standard-USB-Ports können maximal 0.5-3A liefern. Die volle LED-Helligkeit würde den Port überlasten und zu Schäden führen!

## Projektstruktur