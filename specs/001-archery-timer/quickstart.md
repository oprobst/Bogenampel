# Quickstart-Anleitung: Bogenampel

**Datum**: 2025-12-04
**Phase**: 1 (Design)
**Status**: Abgeschlossen

## Übersicht

Diese Anleitung führt Sie durch Hardware-Setup, Programmierung und initiales Testen des Bogenampel-Systems.

**Geschätzte Zeit**: 60-90 Minuten

---

## Voraussetzungen

### Hardware (für komplettes System)

**2× PCB-Platinen** (identisch, siehe `/Schaltung/`):
- Bogenampel PCB (KiCad-Projekt)

**Komponenten pro Platine** (siehe `/Schaltung/Schaltplan.pdf`):
- 1× Arduino Nano (ATmega328P)
- 1× nRF24L01+ Funkmodul mit PCB-Antenne
- 1× 10µF Kondensator (nRF24-Stabilisierung)
- 2× Status-LEDs + 330Ω Widerstände
- Pin-Header und Steckverbinder (J1-J9, RV1, RV2)

**Zusätzlich für Sender**:
- 1× Taster (J1: Timer-Steuerung)
- 1× Ein/Aus-Schalter (J2)
- 1× 9V Block-Batterie + Batterieclip (J7)

**Zusätzlich für Empfänger**:
- 1× WS2812B LED-Strip (~155 LEDs) (J9)
- 1× KY-006 passiver Piezo-Buzzer (J8)
- 1× Potentiometer 10kΩ (RV1: Lautstärke)
- 1× Schalter (J1: 120/240s Modus)
- 1× Jumper (J5: Development Mode)
- 1× 330Ω Widerstand (LED-Datenleitung Schutz)
- 1× 100Ω Widerstand (Buzzer-Signal Schutz)
- 1× 1000µF Kondensator (LED-Stromversorgung)
- 1× USB-C Powerbank (10.000+ mAh)

### Software

- **Arduino IDE** 1.8.x oder 2.x (https://www.arduino.cc/en/software)
- **Alternative**: PlatformIO (https://platformio.org/)

### Bibliotheken

Folgende Bibliotheken müssen in Arduino IDE installiert werden:

1. **RF24** (TMRh20) - nRF24L01+ Funkmodul
   - Arduino IDE: Tools → Manage Libraries → Suche "RF24" → Version 1.4.x
   - GitHub: https://github.com/nRF24/RF24

2. **FastLED** - WS2812B LED-Steuerung
   - Arduino IDE: Tools → Manage Libraries → Suche "FastLED" → Version 3.6.x
   - GitHub: https://github.com/FastLED/FastLED

---

## Schritt 1: PCB-Bestückung

### 1.1 Sender-Bestückung

**Referenz**: `/Schaltung/Schaltplan.pdf` (linke Seite)

1. **Arduino Nano** auf PCB löten/stecken
2. **nRF24L01+** Modul in Header stecken/löten
3. **10µF Kondensator** (nRF24-Stabilisierung, C1)
4. **Status-LEDs** (D1, D2) mit 330Ω Widerständen
5. **Taster** (J1) für Timer-Steuerung
6. **Ein/Aus-Schalter** (J2) für 9V-Batterie
7. **Batterieclip** (J7) für 9V Block-Batterie

**Wichtig**: Alle Pin-Zuweisungen MÜSSEN mit `/Schaltung/Schaltplan.pdf` übereinstimmen!

### 1.2 Empfänger-Bestückung

**Referenz**: `/Schaltung/Schaltplan.pdf` (rechte Seite)

1. **Arduino Nano** auf PCB löten/stecken
2. **nRF24L01+** Modul in Header stecken/löten
3. **10µF Kondensator** (nRF24-Stabilisierung, C1)
4. **1000µF Kondensator** (LED-Stromversorgung, C2)
5. **Status-LEDs** (D1, D2) mit 330Ω Widerständen
6. **330Ω Widerstand** (LED-Datenleitung Schutz)
7. **100Ω Widerstand** (Buzzer-Signal Schutz)
8. **Potentiometer 10kΩ** (RV1) für Lautstärke
9. **Modusschalter** (J1) für 120s/240s
10. **Development-Mode Jumper-Header** (J5)
11. **Connector für WS2812B-Strip** (J9)
12. **Connector für Buzzer** (J8)
13. **USB-C Connector** (J6) für Powerbank

**Wichtig**:
- **Development-Mode Jumper (J5)**: Pin D2 → GND setzen beim Programmieren über USB!
- LED-Strip-Connector (J9) mit 330Ω Schutzwiderstand in Datenleitung

---

## Schritt 2: Arduino IDE Setup

### 2.1 Arduino IDE Installation

1. Download von https://www.arduino.cc/en/software
2. Installation ausführen (Windows: .exe, macOS: .dmg, Linux: AppImage)
3. Arduino IDE starten

### 2.2 Board-Konfiguration

1. Arduino IDE öffnen
2. **Tools** → **Board** → **Arduino AVR Boards** → **Arduino Nano**
3. **Tools** → **Processor** → **ATmega328P (Old Bootloader)** *oder* **ATmega328P** (je nach Nano-Version)
4. **Tools** → **Port** → USB-Port auswählen (z.B. COM3, /dev/ttyUSB0)

### 2.3 Bibliotheken installieren

1. **Sketch** → **Include Library** → **Manage Libraries...**
2. Suche "**RF24**" → Installiere **RF24 by TMRh20** (Version 1.4.x)
3. Suche "**FastLED**" → Installiere **FastLED** (Version 3.6.x)
4. IDE neustarten

**Alternative (PlatformIO)**:
```ini
[env:nanoatmega328]
platform = atmelavr
board = nanoatmega328
framework = arduino
lib_deps =
    nRF24/RF24@^1.4.8
    fastled/FastLED@^3.6.0
```

---

## Schritt 3: Firmware-Programmierung

### 3.1 Empfänger programmieren (WICHTIG: ZUERST!)

**⚠️ WICHTIG: Development-Mode aktivieren!**

1. **Jumper (J5) setzen**: Pin D2 → GND verbinden
   - Dies reduziert LED-Helligkeit auf 3%
   - **Verhindert USB-Port-Beschädigung** (Standard-USB liefert nur 0,5-3A)

2. **Empfänger per USB an Computer anschließen**

3. **Arduino IDE öffnen**:
   - **File** → **Open** → `Empfaenger/Empfaenger.ino`

4. **Code überprüfen**:
   - Öffne `Empfaenger/config.h`
   - Prüfe Pin-Definitionen gegen `/Schaltung/Schaltplan.pdf`

5. **Upload**:
   - **Sketch** → **Upload** (oder Ctrl+U / Cmd+U)
   - Warte auf "Done uploading"

6. **Testen**:
   - Öffne **Serial Monitor** (Ctrl+Shift+M / Cmd+Shift+M)
   - Baudrate: 115200
   - Erwartete Ausgabe: "Empfänger initialisiert..."
   - LEDs sollten schwach leuchten (3% Helligkeit)

7. **USB trennen, Jumper entfernen**:
   - Jumper (J5) entfernen für Normalbetrieb
   - Powerbank anschließen an J6
   - LEDs sollten jetzt hell leuchten (volle Helligkeit)

### 3.2 Sender programmieren

1. **Sender per USB an Computer anschließen**
   - Kein Jumper nötig (Sender hat keine LEDs)

2. **Arduino IDE öffnen**:
   - **File** → **Open** → `Sender/Sender.ino`

3. **Code überprüfen**:
   - Öffne `Sender/config.h`
   - Prüfe Pin-Definitionen gegen `/Schaltung/Schaltplan.pdf`

4. **Upload**:
   - **Sketch** → **Upload** (oder Ctrl+U / Cmd+U)
   - Warte auf "Done uploading"

5. **Testen**:
   - Öffne **Serial Monitor** (115200 Baud)
   - Erwartete Ausgabe: "Sender initialisiert..."
   - Status-LEDs sollten leuchten

6. **USB trennen, Batterie anschließen**:
   - 9V Block-Batterie an J7 anschließen
   - Ein/Aus-Schalter (J2) einschalten
   - Status-LEDs sollten leuchten

---

## Schritt 4: Initiales Testen

### 4.1 Funkverbindung testen

**Setup**:
- Empfänger mit Powerbank betrieben (J6)
- Sender mit 9V-Batterie betrieben (J7)
- Beide Geräte im gleichen Raum (~5m Entfernung)

**Test 1: START-Befehl**

1. Empfänger sollte "000" in Rot zeigen (STOPPED)
2. **Taster am Sender drücken** (J1)
3. **Erwartetes Verhalten**:
   - Empfänger spielt tiefen Ton (1500 Hz)
   - Anzeige wechselt zu "010" in Gelb (PREPARATION)
   - Nach 10 Sekunden: Hoher Ton (2500 Hz)
   - Anzeige wechselt zu "120" oder "240" in Grün (SHOOTING)
   - Countdown läuft herunter

**Test 2: STOP-Befehl**

1. Während Timer läuft (Grün), **Taster am Sender drücken**
2. **Erwartetes Verhalten**:
   - Timer stoppt sofort
   - Anzeige wechselt zu "000" in Rot

**Test 3: Automatischer Timer-Ablauf**

1. Timer starten (Taster drücken)
2. Timer laufen lassen ohne zu stoppen
3. **Erwartetes Verhalten**:
   - Bei 30 Sekunden verbleibend: Anzeige wechselt von Grün zu Gelb
   - Bei 0 Sekunden: Mittlerer Ton (2000 Hz), Anzeige "000" Rot
   - Nach 2 Sekunden: Automatischer Übergang zu STOPPED

### 4.2 Reichweitentest

**Test 4: Indoor-Reichweite**

1. Empfänger an Startpunkt platzieren
2. Sender in 10m-Schritten wegbewegen
3. Bei jeder Distanz START/STOP senden
4. **Erwartetes Verhalten**:
   - 0-20m: 100% Erfolgsrate
   - 20-50m: >95% Erfolgsrate
   - >50m: Degraded

**Hinweis**: Bei fehlgeschlagenem Befehl blinken Sender-Status-LEDs

### 4.3 Modusschalter testen

**Test 5: 120s vs. 240s Modus**

1. Empfänger ausschalten
2. Modusschalter (J1) auf 120s Position setzen
3. Empfänger einschalten
4. Timer starten
5. **Erwartetes Verhalten**: Countdown von 120 Sekunden

6. Empfänger ausschalten
7. Modusschalter auf 240s Position setzen
8. Empfänger einschalten
9. Timer starten
10. **Erwartetes Verhalten**: Countdown von 240 Sekunden

### 4.4 Lautstärke testen

**Test 6: Potentiometer**

1. Potentiometer (RV1) ganz nach links drehen (leise)
2. Timer starten
3. **Erwartetes Verhalten**: Töne leise/kaum hörbar

4. Potentiometer ganz nach rechts drehen (laut)
5. Timer starten
6. **Erwartetes Verhalten**: Töne laut hörbar

---

## Schritt 5: Troubleshooting

### Problem: Empfänger startet nicht

**Symptome**: Keine LEDs, kein Serial Output

**Lösungen**:
1. ✅ USB/Powerbank korrekt angeschlossen?
2. ✅ Development-Mode Jumper (J5) **entfernt** für Powerbank-Betrieb?
3. ✅ Powerbank ausreichend geladen? (>20%)
4. ✅ Arduino Nano korrekt auf PCB gesteckt?

### Problem: LEDs leuchten nicht

**Symptome**: Empfänger läuft, aber LEDs dunkel

**Lösungen**:
1. ✅ Development-Mode Jumper (J5) gesetzt beim USB-Betrieb? (LEDs dann nur 3% Helligkeit)
2. ✅ WS2812B-Strip korrekt an J9 angeschlossen? (Data, VCC, GND)
3. ✅ 1000µF Kondensator (C2) korrekt verlötet?
4. ✅ Powerbank liefert ausreichend Strom? (mindestens 2A)

### Problem: Kein Funkempfang

**Symptome**: Sender-Taster drücken hat keine Wirkung

**Lösungen**:
1. ✅ Beide Geräte verwenden gleiche Firmware-Version?
2. ✅ nRF24L01+ Module korrekt auf PCB gesteckt?
3. ✅ 10µF Kondensatoren (C1) an beiden nRF24-Modulen?
4. ✅ Entfernung <50m Indoor?
5. ✅ Serial Monitor prüfen: "Radio init OK" Meldung?

### Problem: Sender-Status-LEDs blinken

**Symptome**: Nach Tasterdruck blinken LEDs

**Bedeutung**: Kein ACK vom Empfänger empfangen

**Lösungen**:
1. ✅ Empfänger eingeschaltet und betriebsbereit?
2. ✅ Entfernung zu groß? (näher kommen)
3. ✅ Hindernisse zwischen Sender/Empfänger? (Metall, Wände)

### Problem: Timer ungenau

**Symptome**: Countdown weicht mehr als ±2 Sekunden ab

**Lösungen**:
1. ✅ Arduino-Clock-Frequenz korrekt? (16 MHz Quarz auf Nano)
2. ✅ `millis()` Overflow-Bug? (nach 49 Tagen, unwahrscheinlich)
3. ✅ Code-Modifikationen, die `delay()` verwenden? (Vermeiden!)

### Problem: USB-Port schaltet ab beim Programmieren

**Symptome**: Computer zeigt "USB-Device not recognized"

**⚠️ KRITISCH**: Development-Mode Jumper (J5) NICHT gesetzt!

**Lösungen**:
1. ✅ Sofort USB trennen!
2. ✅ Jumper (J5) setzen: Pin D2 → GND
3. ✅ Erneut verbinden und programmieren

**Warnung**: Ohne Jumper ziehen LEDs zu viel Strom (bis 9.3A) und können USB-Port beschädigen!

---

## Schritt 6: Nächste Schritte

✅ **Quickstart abgeschlossen** - System ist einsatzbereit!

### Weiterführende Aktivitäten

1. **Langzeittest**: System für 8+ Stunden laufen lassen
2. **Reichweitentest**: Outdoor-Test bei 100m
3. **Batterielaufzeit-Messung**: Sender-Batterie und Empfänger-Powerbank ausmessen
4. **Gehäuse**: PCBs in Gehäuse einbauen (Optional)
5. **Dokumentation**: Benutzerhandbuch für Standaufsichten erstellen

### Code-Anpassungen (optional)

**Töne ändern**:
```cpp
// In Empfaenger/buzzer.cpp
#define TONE_PREPARATION 1500  // Hz (ändern auf gewünschte Frequenz)
#define TONE_SHOOTING 2500
#define TONE_EXPIRED 2000
```

**Timer-Dauer ändern**:
```cpp
// In Empfaenger/config.h
#define TIMER_MODE_SHORT 120   // Sekunden (ändern auf gewünschte Dauer)
#define TIMER_MODE_LONG 240
```

**LED-Farben ändern**:
```cpp
// In Empfaenger/display.cpp
#define COLOR_RED CRGB(255, 0, 0)
#define COLOR_YELLOW CRGB(255, 255, 0)  // Ändern für andere Farben
#define COLOR_GREEN CRGB(0, 255, 0)
```

---

## Referenzen

- **Schaltplan**: `/Schaltung/Schaltplan.pdf`
- **KiCad-Projekt**: `/Schaltung/Bogenampel.kicad_pro`
- **Datenmodell**: `data-model.md`
- **Funkprotokoll**: `contracts/radio-protocol.md`
- **Research**: `research.md`

---

## Support & Hilfe

Bei Problemen:
1. Prüfen Sie **Serial Monitor** Output (115200 Baud)
2. Vergleichen Sie Pin-Belegungen mit `/Schaltung/Schaltplan.pdf`
3. Validieren Sie Constitution-Compliance (siehe `plan.md`)

**Viel Erfolg mit der Bogenampel!** 🎯
