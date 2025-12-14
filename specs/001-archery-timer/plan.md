# Implementierungsplan: Drahtloses Timer-System für Bogenschießplätze

**Branch**: `001-archery-timer` | **Datum**: 2025-12-04 | **Spec**: [spec.md](./spec.md)
**Eingabe**: Feature-Spezifikation aus `/specs/001-archery-timer/spec.md`
**Benutzerhinweis**: Der gleiche Schaltplan wird für Sender und Empfänger benutzt. Lediglich die Bestückung ist jeweils anders gewählt.

## Zusammenfassung

Das Bogenampel-System ist eine funkgesteuerte Timer-Anzeige für Bogenschießplätze, bestehend aus zwei Arduino Nano-basierten Einheiten: einem Sender (Fernbedienung) an der Schießlinie und einem Empfänger (Anzeigeeinheit) am Ziel. Das System koordiniert Schießphasen durch drahtlose Kommunikation (nRF24L01+, 2.4 GHz) und visualisiert den Timer-Status durch WS2812B-LED-Anzeige in Ampelfarben mit akustischen Signalen.

**Technischer Ansatz**: Zwei identische PCB-Designs mit unterschiedlicher Bestückung für Sender und Empfänger. State-Machine-basierte Firmware für deterministische Zustandsübergänge. Fail-Safe-Design mit autonomem Empfänger-Betrieb bei Kommunikationsverlust.

## Technischer Kontext

**Sprache/Version**: C/C++ (Arduino), Arduino IDE 1.8.x / 2.x, PlatformIO
**Primäre Abhängigkeiten**:
- RF24 (nRF24L01+ Funkmodul-Treiber)
- FastLED oder Adafruit_NeoPixel (WS2812B LED-Strip Steuerung)
- Arduino Core für ATmega328P

**Speicher**: EEPROM (1KB für Konfiguration), SRAM (2KB für Runtime-State)
**Testing**: Hardware-in-the-Loop Tests, Unit-Tests für State-Machine-Logik
**Zielplattform**: Arduino Nano (ATmega328P, 5V/16MHz), Bare-Metal (keine OS)
**Projekttyp**: Embedded Dual-Firmware (Sender + Empfänger)

**Performance-Ziele**:
- Timer-Genauigkeit: ±1 Sekunde über 240 Sekunden
- Phasenübergang-Latenz: <500ms
- Funkübertragung: <100ms Latenz
- System-Initialisierung: <5 Sekunden

**Einschränkungen**:
- Flash-Speicher: 32KB (ATmega328P)
- SRAM: 2KB (begrenzt Stack/Heap)
- USB-Stromversorgung (Entwicklung): 0,5-3A
- Powerbank-Stromversorgung (Betrieb): bis 12A
- Funkreichweite: 100m Freifeld, 50m Indoor

**Umfang**:
- 2 Firmware-Programme (Sender, Empfänger)
- ~5-7 Zustandsmaschinen-Zustände (Timer-Phasen)
- ~155 WS2812B LEDs (3x 7-Segment-Anzeige)
- 2 Befehle (START, STOP)

## Constitution Check

*GATE: Muss vor Phase 0 Research bestanden werden. Erneute Prüfung nach Phase 1 Design.*

### Prinzip I: Sicherheit Zuerst ✅

- **Fail-Safe bei Kommunikationsausfall**: Empfänger läuft autonom weiter bis Timer-Ende
- **Deterministische Zustandsübergänge**: State-Machine mit klaren Transitionen
- **Eindeutige Stopp-Signale**: Rote Ampelphase visuell und akustisch unterscheidbar
- **Funksignal-Validierung**: CRC-Prüfung durch nRF24L01+ Hardware
- **USB-Port-Schutz**: Entwicklungsmodus mit PWM-Helligkeitsbegrenzung (3%)

**Status**: ✅ BESTANDEN

### Prinzip II: Einfachheit & Zuverlässigkeit ✅

- **Einfacher Code**: State-Machine-Pattern, keine komplexen Algorithmen
- **Ein-Knopf-Bedienung**: Einzelner Taster für START/STOP
- **Plug-and-Play**: Keine Konfiguration erforderlich, sofort betriebsbereit
- **Standardkomponenten**: Arduino Nano, nRF24L01+, WS2812B (alle weit verbreitet)
- **Energieverwaltung**: Sleep-Modi zwischen Timer-Updates

**Status**: ✅ BESTANDEN

### Prinzip III: Embedded-Hardware-Standards ✅

- **Arduino/PlatformIO-Kompatibilität**: Standard Arduino Core
- **Stabile Bibliotheken**: RF24 (TMRh20), FastLED/NeoPixel (etablierte Libs)
- **Pin-Belegung als Konstanten**: Alle Pins als `#define` oder `const` deklariert
- **Schaltplan-Compliance**: Pin-Zuweisungen aus `/Schaltung/Schaltplan.pdf`
- **Hardware-Schutz**: Pull-up/Pull-down Widerstände im Schaltplan
- **Stromversorgungsgrenzen**: PWM-Dimmung für USB-Compliance

**Status**: ✅ BESTANDEN - Pin-Zuweisungen MÜSSEN aus `/Schaltung/Schaltplan.pdf` extrahiert werden

### Prinzip IV: Testbarkeit & Validierung ✅

- **Testbare Timer-Phasen**: Jeder Zustand isoliert testbar
- **Realistische Funktests**: Reichweitentests in Labor und Feld
- **Signal-Validierung**: Frequenz-Messungen (1500, 2000, 2500 Hz), Farbüberprüfung
- **Definierte Grenzfälle**: Siehe research.md für Verhalten bei Funkausfall, etc.
- **Stromverbrauchs-Messung**: Multimeter-Messungen bei verschiedenen Modi

**Status**: ✅ BESTANDEN - Grenzfall-Verhalten muss in Phase 0 definiert werden

### Prinzip V: Wartbarkeit & Dokumentation ✅

- **Pin-Dokumentation**: Alle Pins mit Connector-Referenzen kommentiert (z.B. `// J9: WS2812B`)
- **Schaltplan-Synchronisation**: Code folgt `/Schaltung/Schaltplan.pdf`
- **Selbsterklärende Namen**: `STATE_PREPARATION`, `STATE_SHOOTING`, etc.
- **Kommentierte Zustandsübergänge**: State-Machine-Transitionen dokumentiert
- **Teilenummern**: Alle Komponenten in README.md und Schaltplan

**Status**: ✅ BESTANDEN

### Gesamtergebnis: ✅ ALLE GATES BESTANDEN

Keine Complexity-Violations. Projekt folgt allen Constitution-Prinzipien.

## Projektstruktur

### Dokumentation (dieses Feature)

```text
specs/001-archery-timer/
├── plan.md              # Diese Datei (/speckit.plan Befehl)
├── spec.md              # Feature-Spezifikation
├── research.md          # Phase 0: Technologie-Entscheidungen
├── data-model.md        # Phase 1: State-Machine & Datenstrukturen
├── quickstart.md        # Phase 1: Hardware-Setup und Programmier-Anleitung
├── contracts/           # Phase 1: Funkprotokoll-Spezifikation
└── tasks.md             # Phase 2: Aufgabenliste (/speckit.tasks - noch nicht erstellt)
```

### Quellcode (Repository Root)

```text
# Embedded Dual-Firmware Struktur

Sender/
├── Sender.ino           # Hauptprogramm für Sender (Arduino IDE)
├── config.h             # Pin-Definitionen aus /Schaltung/Schaltplan.pdf
├── radio.h/.cpp         # nRF24L01+ Kommunikation
├── button.h/.cpp        # Taster-Debouncing und Event-Handling
└── power.h/.cpp         # Energieverwaltung (Sleep-Modi)

Empfaenger/
├── Empfaenger.ino       # Hauptprogramm für Empfänger (Arduino IDE)
├── config.h             # Pin-Definitionen aus /Schaltung/Schaltplan.pdf
├── radio.h/.cpp         # nRF24L01+ Kommunikation
├── state_machine.h/.cpp # Timer State-Machine
├── display.h/.cpp       # WS2812B 7-Segment-Anzeige
├── buzzer.h/.cpp        # Piezo-Buzzer Töne
└── power.h/.cpp         # Energieverwaltung

platformio.ini           # PlatformIO-Konfiguration (optional)
lib/
└── common/              # Gemeinsame Definitionen (Befehle, Konstanten)
    └── protocol.h       # Funkprotokoll-Definitionen

tests/
├── state_machine_test/  # Unit-Tests für Zustandsmaschine
└── integration/         # Hardware-in-the-Loop Test-Sketches
```

**Struktur-Entscheidung**: Embedded Dual-Firmware mit getrennten Arduino-Sketches für Sender und Empfänger. Gemeinsame Definitionen in `lib/common/`. Diese Struktur folgt Arduino-Konventionen und ermöglicht einfaches Flashen mit Arduino IDE oder PlatformIO.

## Komplexitäts-Tracking

*Wird nur ausgefüllt, wenn Constitution Check Violations hat, die gerechtfertigt werden müssen.*

Keine Violations - Tabelle nicht erforderlich.

---

## Phase 0: Gliederung & Forschung

### Status: ✅ Abgeschlossen

### Forschungsaufgaben

1. **RF24-Bibliothek Best Practices**
   - Optimale Konfiguration für 100m Reichweite
   - Retry-Strategien und ACK-Handling
   - Power-Amplifier/Low-Noise-Amplifier Einstellungen

2. **WS2812B-Bibliothek Auswahl**
   - FastLED vs. Adafruit_NeoPixel Performance-Vergleich
   - 7-Segment-Encoding für 3-stellige Anzeige
   - Helligkeits-PWM für Entwicklungsmodus (3%)

3. **ATmega328P Timer-Strategien**
   - Millisekunden-genaue Countdown-Implementierung
   - Timer-Interrupts ohne WS2812B-Interferenz
   - Watchdog-Timer für Fail-Safe

4. **Energieverwaltung**
   - Sleep-Modi zwischen Timer-Updates
   - Batterielaufzeit-Optimierung (8h Ziel)
   - USB vs. Powerbank Stromerkennung

5. **Grenzfall-Verhalten**
   - Funkausfall während Countdown
   - Rapid Button-Presses (Debouncing)
   - Modusschalter-Änderung während Betrieb
   - Initialer Power-On State

**Ausgabe**: [research.md](./research.md) - Wird in Phase 0 erstellt

---

## Phase 1: Design & Contracts

### Status: ✅ Abgeschlossen

**Voraussetzungen**: research.md abgeschlossen

### Zu generierende Artefakte

1. **data-model.md**: State-Machine-Diagramm und Datenstrukturen
   - Timer-Zustände (STOPPED, PREPARATION, SHOOTING, WARNING, EXPIRED)
   - Zustandsübergänge mit Bedingungen
   - Datenstrukturen (TimerState, RadioCommand, DisplayConfig)

2. **contracts/radio-protocol.md**: Funkprotokoll-Spezifikation
   - Befehlsformat (START, STOP)
   - Payload-Struktur
   - CRC und Fehlerbehandlung

3. **quickstart.md**: Hardware-Setup und Flash-Anleitung
   - PCB-Bestückung (Sender vs. Empfänger)
   - Arduino IDE Setup
   - Programmier-Schritte (Development-Mode Jumper!)
   - Initiales Testen

**Ausgabe**: data-model.md, contracts/, quickstart.md

---

## Phase 2: Aufgabenplanung

**Wird NICHT von `/speckit.plan` erstellt - verwenden Sie `/speckit.tasks`**

Dieser Befehl (`/speckit.plan`) endet nach Phase 1. Verwenden Sie `/speckit.tasks`, um die Aufgabenliste zu generieren.

---

## Nächste Schritte

1. ✅ Constitution Check bestanden
2. ✅ Phase 0: Research-Dokument erstellt (research.md)
3. ✅ Phase 1: Design-Artefakte generiert (data-model.md, contracts/, quickstart.md)
4. ⏭️ `/speckit.tasks` ausführen für Aufgabenliste (Task-Generierung)
