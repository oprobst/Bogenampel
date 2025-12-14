# Setup-Anleitung: Bogenampel Sender

## 1. Arduino IDE Installation

1. Arduino IDE herunterladen: https://www.arduino.cc/en/software
2. Arduino Nano Board-Support installieren:
   - Tools → Board → Boards Manager → "Arduino AVR Boards"

## 2. Bibliotheken installieren

### Adafruit ST7789 (Display)
```
Sketch → Include Library → Manage Libraries
```

Installiere folgende Bibliotheken:
1. **"Adafruit ST7735 and ST7789 Library" by Adafruit**
2. **"Adafruit GFX Library" by Adafruit** (wird automatisch als Abhängigkeit installiert)
3. **"Adafruit BusIO" by Adafruit** (wird automatisch als Abhängigkeit installiert)

**Keine manuelle Konfiguration nötig!** Die Pins werden direkt im Code festgelegt.

### RF24 (Funkmodul)
```
Sketch → Include Library → Manage Libraries → "RF24" by TMRh20
```

## 3. Hardware anschließen

Siehe `HARDWARE.md` für Pin-Belegung.

**Stromversorgung:**
- USB-Kabel anschließen (für Programmierung)
- ODER 9V-Block mit Schalter

**Wichtig:** Display benötigt 3.3V via Level Shifter!

## 4. Code kompilieren

1. Öffne `Sender.ino` im Basisverzeichnis
2. Wähle Board:
   - Tools → Board → Arduino AVR Boards → Arduino Nano
3. Wähle Prozessor:
   - Tools → Processor → ATmega328P (Old Bootloader)
     ODER ATmega328P
4. Wähle Port:
   - Tools → Port → COM3 (Windows) oder /dev/ttyUSB0 (Linux)
5. Klicke "Upload" (Pfeil-Symbol)

## 5. Fehlerbehebung

### "avrdude: stk500_recv(): programmer is not responding"
→ Falscher Prozessor gewählt. Versuche "ATmega328P (Old Bootloader)"

### "Adafruit_ST7789.h: No such file or directory"
→ Adafruit ST7789 Library nicht installiert. Siehe Schritt 2.

### Display bleibt schwarz
1. Prüfe Stromversorgung (3.3V am Display?)
2. Prüfe Pin-Verbindungen (siehe HARDWARE.md)
3. Prüfe ob das richtige Display verwendet wird (ST7789 vs ILI9341)

### Kompilierung zu groß (>32KB)
→ Flash-Speicher voll. Optimierungen:
   - Config.h: `#define DEBUG_ENABLED 0` setzen
   - Adafruit GFX nutzt weniger Speicher als TFT_eSPI

## 6. Serial Monitor

Debug-Ausgaben über Serial Monitor ansehen:
1. Tools → Serial Monitor
2. Baud Rate: 115200

Erwartete Ausgabe:
```
======================================
  Bogenampel Sender V1.0
======================================
Initialisiere Display...
Display initialisiert
Zeige Splash Screen...
Setup abgeschlossen

Splash Screen beendet
```

## 7. Testen

1. Display sollte Splash Screen zeigen: "BOGENAMPEL" + "V1.0"
2. Nach 3 Sekunden oder Tastendruck: "Bereit"
3. Grüne LED sollte leuchten (wenn implementiert)

## PlatformIO (Alternative)

```ini
[env:nanoatmega328]
platform = atmelavr
board = nanoatmega328
framework = arduino
lib_deps =
    adafruit/Adafruit ST7735 and ST7789 Library@^1.10.0
    adafruit/Adafruit GFX Library@^1.11.0
    nRF24/RF24@^1.4.0
```

