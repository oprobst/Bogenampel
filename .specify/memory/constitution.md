<!--
SYNC IMPACT REPORT
==================
Version Change: 1.0.0 → 1.1.0
Rationale: Added authoritative pin assignments from KiCad schematic (/Schaltung/Schaltplan.pdf)

Changes Made:
- Added complete pin assignment reference section
- Linked to authoritative hardware source (/Schaltung/)
- Enhanced Principle III with mandatory schematic compliance
- Enhanced Principle V with schematic synchronization requirement

Modified Principles:
- III. Embedded-Hardware-Standards - Added "Pin-Belegungen MÜSSEN mit /Schaltung/Schaltplan.pdf übereinstimmen"
- V. Wartbarkeit & Dokumentation - Added "Änderungen an Pin-Belegungen MÜSSEN in KiCad-Schaltplan unter /Schaltung/ dokumentiert werden"

Sections Added:
- Pin-Belegungen (Pin Assignments) - Complete reference from KiCad schematic

Templates Status:
✅ plan-template.md - Verified alignment
✅ spec-template.md - Verified alignment
✅ tasks-template.md - Verified alignment

Follow-up TODOs: None
-->

# Bogenampel Constitution

## Kernprinzipien

### I. Sicherheit Zuerst (Safety First)

**NON-NEGOTIABLE**: Sicherheit hat absolute Priorität in allen Designentscheidungen.

- Das System MUSS bei Kommunikationsausfall in einen sicheren Zustand übergehen (Fail-Safe-Prinzip)
- Alle Zustandsübergänge MÜSSEN vorhersehbar und deterministisch sein
- Rote Ampelphase (Stopp-Signal) MUSS eindeutig erkennbar sein und darf nicht mit anderen Zuständen verwechselbar sein
- Funksignale MÜSSEN validiert werden - keine ungeprüfte Ausführung empfangener Befehle
- Entwicklungsmodus MUSS USB-Port-Beschädigung durch Strombegrenzung verhindern

**Rationale**: Die Bogenampel ist ein Sicherheitssystem für Bogenschießplätze. Fehlfunktionen könnten zu gefährlichen Situationen führen, wenn Schützen fälschlicherweise annehmen, das Schießen sei erlaubt oder verboten.

### II. Einfachheit & Zuverlässigkeit (Simplicity & Reliability)

Minimale Benutzereingaben, maximale Betriebssicherheit.

- Code MUSS einfach und verständlich sein - keine unnötige Komplexität
- Ein-Knopf-Bedienung für Hauptfunktionen (Start/Stop) MUSS erhalten bleiben
- System MUSS ohne Konfiguration oder Setup sofort betriebsbereit sein (Plug-and-Play)
- Hardware-Komponenten MÜSSEN standardisiert und leicht austauschbar sein
- Energieverwaltung MUSS Batterielaufzeit maximieren (Sleep-Modi nutzen)

**Rationale**: Das Bedienkonzept basiert auf "möglichst einfach, mit minimalen Benutzereingaben". Komplexität erhöht Fehlerrisiko und senkt Benutzerakzeptanz.

### III. Embedded-Hardware-Standards (Embedded Hardware Standards)

Arduino-Platform und etablierte Standards nutzen.

- Code MUSS mit Arduino IDE und PlatformIO kompatibel sein
- Verwendete Bibliotheken MÜSSEN stabil, wartbar und Open Source sein
- Pin-Belegungen MÜSSEN dokumentiert und im Code als Konstanten definiert sein
- **Pin-Belegungen MÜSSEN exakt mit dem KiCad-Schaltplan unter `/Schaltung/Schaltplan.pdf` übereinstimmen (NON-NEGOTIABLE)**
- Hardware-Schnittstellen MÜSSEN gegen Fehlbeschaltung geschützt sein (Pull-up/Pull-down Widerstände)
- Stromversorgung MUSS innerhalb spezifizierter Grenzen bleiben (USB: 0,5-3A, Powerbank: bis 12A)

**Rationale**: Standardisierung erleichtert Wartung, Reparatur und Weiterentwicklung. Arduino-Ökosystem bietet bewährte Komponenten und Community-Support. Der KiCad-Schaltplan ist die Single Source of Truth für alle Hardware-Verbindungen.

### IV. Testbarkeit & Validierung (Testability & Validation)

Alle Funktionen MÜSSEN testbar und validierbar sein.

- Jede Timer-Phase MUSS einzeln testbar sein (Stopp, Vorbereitung, Aktiv, Abgelaufen)
- Funkübertragung MUSS unter realistischen Bedingungen getestet werden (Reichweite, Interferenzen)
- Akustische und visuelle Signale MÜSSEN auf Korrektheit geprüft werden (Frequenzen, Farben, Timing)
- Grenzfälle MÜSSEN definierte erwartete Verhaltensweisen haben und getestet werden
- Stromverbrauch MUSS gemessen und gegen Spezifikationen validiert werden

**Rationale**: Embedded Systems sind schwer zu debuggen im Feld. Umfassende Tests vor Deployment sind kritisch für Zuverlässigkeit.

### V. Wartbarkeit & Dokumentation (Maintainability & Documentation)

Code und Hardware MÜSSEN vollständig dokumentiert sein.

- Alle Pin-Belegungen MÜSSEN im Code und in Hardware-Dokumentation übereinstimmen
- **Änderungen an Pin-Belegungen MÜSSEN zuerst im KiCad-Schaltplan unter `/Schaltung/` vorgenommen werden, bevor Code angepasst wird**
- Funktionen MÜSSEN klare, selbsterklärende Namen haben
- Zustandsübergänge MÜSSEN mit Kommentaren erklärt sein
- Hardware-Komponenten MÜSSEN mit Teilenummern und Spezifikationen dokumentiert sein
- Schaltpläne und Verkabelungsdiagramme MÜSSEN aktuell gehalten werden
- Code-Kommentare MÜSSEN auf entsprechende Connector-Bezeichner im Schaltplan verweisen (z.B. J1, J8, RV1)

**Rationale**: Das System muss von anderen gewartet werden können. Fehlende Dokumentation führt zu Ausfallzeiten und Fehlern bei Reparaturen. Der KiCad-Schaltplan unter `/Schaltung/` ist die autoritative Quelle für Hardware-Designs.

## Hardware-Anforderungen

### Komponenten-Spezifikationen

- **Mikrocontroller**: Arduino Nano (ATmega328P, 5V/16MHz)
- **Funkmodul**: nRF24L01+ mit PCB-Antenne (2.4 GHz, bis 100m Reichweite)
- **LED-Anzeige**: WS2812B LED-Strip (~155 LEDs für 3x 7-Segment)
- **Audio**: Passiver Piezo-Buzzer KY-006
- **Stromversorgung Sender**: 9V Block-Batterie oder 5V USB
- **Stromversorgung Empfänger**: 5V USB-Powerbank (mindestens 10.000 mAh für 8h Betrieb)

### Betriebsparameter

- **Funkreichweite**: 20-50m Indoor, bis 100m Freifeld
- **Timer-Genauigkeit**: ±1 Sekunde über 240 Sekunden
- **LED-Helligkeit**: Volle Helligkeit im Powerbank-Betrieb, 3% im Development-Modus
- **Batterielaufzeit**: Mindestens 8 Stunden bei normaler Nutzung
- **Initialisierungszeit**: Unter 5 Sekunden von Einschalten bis Betriebsbereit

### Pin-Belegungen

**AUTORITATIVE QUELLE**: `/Schaltung/Schaltplan.pdf` (KiCad-Projekt unter `/Schaltung/`)

**Alle Pin-Zuweisungen im Code MÜSSEN exakt mit dem Schaltplan übereinstimmen.**

#### Anschlüsse & Komponenten (Connector-Referenzen aus Schaltplan)

- **J1**: Input 1 (Taster/Eingang)
- **J2**: Input 2 (Taster/Eingang)
- **J3**: Input 3 (Taster/Eingang)
- **J5**: Eingangstasche (Taster 120/240 sec, Gruppe A/B, etc.)
- **J6**: Anschluss Powerbank (Klemmen)
- **J7**: Batterie 9V (Sender)
- **J8**: Anschluss LED-Stripe / KY-006 Debug Jumper
- **J9**: WS2812B Stripe Timer
- **RV1**: 6_Potentiometer (Lautstärke-Regelung)
- **RV2**: Optionaler Analog Input

#### Arduino Nano Pin-Zuweisungen

**Digital Pins:**
- **D0**: RX (Seriell)
- **D1**: TX (Seriell)
- **D2-D13**: GPIO-Pins (siehe Schaltplan für spezifische Zuweisungen)

**Analog Pins:**
- **A0-A7**: Analog-Eingänge (siehe Schaltplan für spezifische Zuweisungen)

**nRF24L01+ Funkmodul (U1):**
- **VCC**: 3.3V Versorgung
- **GND**: Ground
- **CE**: Chip Enable
- **CSN**: Chip Select Not
- **SCK**: SPI Clock
- **MOSI**: Master Out Slave In
- **MISO**: Master In Slave Out
- **IRQ**: Interrupt Request (optional)

**Stromversorgung:**
- **USB_C_Receptacle_PowerOnly_6P (US01)**: USB-C Port für Programmierung (Empfänger)
- **VBUS**: USB 5V
- **CC1, CC2**: Configuration Channel
- **SHIELD, GND**: Ground-Verbindungen

**Hinweis**: Für detaillierte Verbindungen zwischen Arduino-Pins und Komponenten siehe `/Schaltung/Schaltplan.pdf`. Der Schaltplan zeigt die exakten Pin-Nummern für alle Verbindungen (z.B. welcher Arduino-Pin mit welchem nRF24-Pin verbunden ist).

## Entwicklungs-Workflow

### Code-Standards

- Alle Zustandsvariablen MÜSSEN volatile deklariert sein, wenn in Interrupts verwendet
- Magic Numbers MÜSSEN durch benannte Konstanten ersetzt werden
- Globale Variablen MÜSSEN minimiert und klar dokumentiert sein
- Timer-Interrupts MÜSSEN korrekt konfiguriert und dokumentiert sein

### Test-Workflow

1. **Unit-Tests**: Logik-Funktionen isoliert testen (State-Machine-Übergänge)
2. **Hardware-in-the-Loop**: Testen mit echter Hardware in Labor-Umgebung
3. **Reichweitentests**: Funkübertragung unter realistischen Bedingungen
4. **Langzeittests**: Batterielaufzeit und Stabilitätstests (mindestens 8 Stunden)
5. **Feldtests**: Finale Validierung unter echten Einsatzbedingungen

### Review-Anforderungen

- Code-Reviews MÜSSEN Pin-Belegungen gegen `/Schaltung/Schaltplan.pdf` prüfen
- Code-Kommentare MÜSSEN Connector-Bezeichner aus dem Schaltplan referenzieren (z.B. "// J9: WS2812B Stripe Timer")
- Alle Änderungen an Zustandsmaschine MÜSSEN State-Diagramm-Updates beinhalten
- Stromverbrauchsänderungen MÜSSEN neu gemessen werden
- Safety-kritische Änderungen erfordern zusätzliches Review
- Hardware-Änderungen MÜSSEN zuerst im KiCad-Schaltplan dokumentiert werden, bevor Code geändert wird

## Governance

### Amendment-Prozess

Änderungen an dieser Verfassung erfordern:
1. Dokumentation der vorgeschlagenen Änderung mit Begründung
2. Review und Zustimmung des Projektverantwortlichen
3. Aktualisierung aller abhängigen Templates und Dokumentation
4. Versions-Bump nach Semantic Versioning

### Versioning-Richtlinien

- **MAJOR**: Entfernung oder grundlegende Neudefinition von Prinzipien
- **MINOR**: Neue Prinzipien oder wesentliche Erweiterungen
- **PATCH**: Klarstellungen, Formulierungsverbesserungen, Korrekturen

### Compliance

- Alle Spezifikationen MÜSSEN gegen diese Prinzipien geprüft werden
- Alle Implementierungspläne MÜSSEN Sicherheitsaspekte adressieren
- Alle Code-Reviews MÜSSEN Einfachheit und Wartbarkeit bewerten
- Hardware-Änderungen MÜSSEN gegen Komponentenstandards validiert werden

### Laufzeit-Entwicklungsleitfaden

Für detaillierte Implementierungsrichtlinien siehe:
- **README.md**: Projektübersicht und Funktionsbeschreibung
- **/Schaltung/Schaltplan.pdf**: Autoritative Hardware-Pin-Belegungen (KiCad)
- **/Schaltung/**: Vollständiges KiCad-Projekt für Hardware-Modifikationen

**Version**: 1.1.0 | **Ratifiziert**: 2025-12-04 | **Zuletzt geändert**: 2025-12-04
