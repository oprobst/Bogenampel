<!--
SYNC IMPACT REPORT
==================
Version Change: 1.1.0 → 2.0.0 → 2.1.0 (2026-06-10) → 2.2.0 (2026-06-11)

2.0.0 — Rationale: Hardware-Generationswechsel V2 → V3 (ESP32 statt Arduino Nano, ESP-NOW
statt NRF24L01, e-Paper statt TFT). Autoritative Hardware-Quelle ist jetzt das KiCad-Projekt
`Schaltung-Sender/` bzw. `Schaltung-Empfaenger/`. MAJOR-Bump wegen grundlegender Neudefinition der
Hardware-Standards-Sektion (Versioning-Richtlinie).

2.1.0 — Rationale: USB-Strombegrenzungs-Regel (Entwicklungsmodus, Prinzip I) entfernt —
in V3 wird der LED-Strip immer aus einer externen Quelle versorgt (5 V oder 12 V per Jumper
im Empfänger), nie aus dem USB-Port des MCU-Boards; die Schutzanforderung ist gegenstandslos.
Entscheidung des Projektverantwortlichen vom 2026-06-10.

2.2.0 — Rationale: Arduino-IDE-Kompatibilitätsanforderung (Prinzip III) entfernt — das
Projekt migriert mit V3 vollständig auf PlatformIO + ESP32; `pio run` ist der einzige
unterstützte und verifizierte Build-Pfad. Die eingefrorene V2 (Arduino Nano) bleibt davon
unberührt. Entscheidung des Projektverantwortlichen vom 2026-06-11.

Changes Made:
- Hardware-Anforderungen vollständig auf V3 umgestellt (ESP32-S3-Sender, XIAO-ESP32C3-Empfänger)
- Betriebsparameter aktualisiert (Initialisierungszeit < 10 s wegen e-Paper + Funktest)
- Pin-Belegungen: autoritative Quelle jetzt `Schaltung-Sender/` bzw. `Schaltung-Empfaenger/` (KiCad);
  eingefrorener Pin-Contract in `specs/004-v3-esp32-port/contracts/hardware-pins.md`
- V2-Hardware (Arduino Nano, NRF24) als eingefrorenes Legacy dokumentiert
- USB-Strombegrenzungs-Bullet aus Prinzip I entfernt (v2.1.0, siehe oben)

Modified Principles:
- I. Sicherheit Zuerst — Entwicklungsmodus-Strombegrenzung entfernt (obsolet durch externe LED-Versorgung)
- III. Embedded-Hardware-Standards — autoritative Schaltplan-Quelle pro Hardware-Generation
- V. Wartbarkeit & Dokumentation — Schaltplan-Pfad auf `Schaltung-Sender/` und `Schaltung-Empfaenger/` aktualisiert

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

- Das System MUSS bei Kommunikationsausfall in einen sicheren Zustand übergehen (Fail-Safe-Prinzip); eine gestartete Passe MUSS auf dem Empfänger autonom korrekt zu Ende laufen, ohne dass dafür ein Funkkommando nötig ist
- Alle Zustandsübergänge MÜSSEN vorhersehbar und deterministisch sein
- Rote Ampelphase (Stopp-Signal) MUSS eindeutig erkennbar sein und darf nicht mit anderen Zuständen verwechselbar sein
- Funksignale MÜSSEN validiert werden - keine ungeprüfte Ausführung empfangener Befehle (Magic + Checksumme + Duplikat-Unterdrückung)

**Rationale**: Die Bogenampel ist ein Sicherheitssystem für Bogenschießplätze. Fehlfunktionen könnten zu gefährlichen Situationen führen, wenn Schützen fälschlicherweise annehmen, das Schießen sei erlaubt oder verboten.

### II. Einfachheit & Zuverlässigkeit (Simplicity & Reliability)

Minimale Benutzereingaben, maximale Betriebssicherheit.

- Code MUSS einfach und verständlich sein - keine unnötige Komplexität
- Ein-Knopf-Bedienung für Hauptfunktionen (Start/Stop) MUSS erhalten bleiben
- System MUSS ohne Konfiguration oder Setup sofort betriebsbereit sein (Plug-and-Play, kein Pairing-Menü)
- Hardware-Komponenten MÜSSEN standardisiert und leicht austauschbar sein
- Energieverwaltung MUSS Batterielaufzeit maximieren (Soft-Power-Latch, vollständiges Abschalten)

**Rationale**: Das Bedienkonzept basiert auf "möglichst einfach, mit minimalen Benutzereingaben". Komplexität erhöht Fehlerrisiko und senkt Benutzerakzeptanz.

### III. Embedded-Hardware-Standards (Embedded Hardware Standards)

Arduino-Platform und etablierte Standards nutzen.

- Code MUSS mit PlatformIO baubar sein (`pio run` pro Firmware-Ordner = verifizierter
  Build-Pfad); Arduino-IDE-Kompatibilität ist seit v2.2.0 (2026-06-11) KEINE Anforderung
  mehr — V3 ist vollständig auf PlatformIO migriert (V2-Legacy bleibt Arduino-IDE-basiert)
- Verwendete Bibliotheken MÜSSEN stabil, wartbar und Open Source sein
- Pin-Belegungen MÜSSEN dokumentiert und im Code als Konstanten definiert sein
- **Pin-Belegungen MÜSSEN exakt mit dem KiCad-Schaltplan der jeweiligen Hardware-Generation übereinstimmen (NON-NEGOTIABLE)**: V3-Firmware (`SenderV3/`, `EmpfaengerV3/`) gegen `Schaltung-Sender/` bzw. `Schaltung-Empfaenger/`; eingefrorene V2-Firmware (`Sender/`, `Empfaenger/`) gegen den V2-Schaltplan aus der Git-Historie (`Schaltung/`, entfernt am 2026-08-01)
- Hardware-Schnittstellen MÜSSEN gegen Fehlbeschaltung geschützt sein (Pull-up/Pull-down Widerstände)
- Analoge Eingänge MÜSSEN auf ADC1-Pins liegen (ESP32: ADC2 ist bei aktivem Funk unbrauchbar)
- Stromversorgung MUSS innerhalb spezifizierter Grenzen bleiben (USB: 0,5-3A, Powerbank/Netzteil: bis 12A)

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
- **Änderungen an Pin-Belegungen MÜSSEN zuerst im KiCad-Schaltplan (V3: `Schaltung-Sender/` bzw. `Schaltung-Empfaenger/`) vorgenommen werden, bevor Code angepasst wird**
- Funktionen MÜSSEN klare, selbsterklärende Namen haben
- Zustandsübergänge MÜSSEN mit Kommentaren erklärt sein
- Hardware-Komponenten MÜSSEN mit Teilenummern und Spezifikationen dokumentiert sein
- Schaltpläne und Verkabelungsdiagramme MÜSSEN aktuell gehalten werden
- Code-Kommentare MÜSSEN auf entsprechende Connector-Bezeichner im Schaltplan verweisen (z.B. J1, J8, RV1)

**Rationale**: Das System muss von anderen gewartet werden können. Fehlende Dokumentation führt zu Ausfallzeiten und Fehlern bei Reparaturen. Der KiCad-Schaltplan ist die autoritative Quelle für Hardware-Designs.

## Hardware-Anforderungen

### Komponenten-Spezifikationen (V3, aktuell)

- **Sender (Bedieneinheit)**: ESP32-S3-WROOM-1U-N16R8 (16 MB Flash, 8 MB Octal-PSRAM); 1.54″ e-Paper 200×200 monochrom (SSD1681, Waveshare-1.54-V2-Rohpanel); LiPo-Akku 3,0–4,2 V mit MCP73837-Lader und USB-C; Ein-Knopf-Soft-Power (Power-Latch)
- **Empfänger (Anzeigeeinheit)**: Seeed XIAO ESP32C3; WS2812B LED-Strip (158 LEDs: 2× Gruppenanzeige + 3× 7-Segment); Piezo-Transducer 12 V über Transistorstufe mit Lautstärke-Poti; Gehäuselüfter mit Tacho; Helligkeits-Poti
- **Funk**: integriertes 2,4-GHz-Funkmodul der ESP32-Chips via ESP-NOW — KEIN externes Funkmodul
- **Stromversorgung Sender**: LiPo-Akku, Laden über USB-C
- **Stromversorgung Empfänger**: extern 12 V oder USB 5 V (keine Akku-Verwaltung)

### Legacy-Hardware (V2, eingefroren)

Die V2-Firmware (`Sender/`, `Empfaenger/`) und ihre Hardware (Arduino Nano ATmega328P,
nRF24L01+, ST7789-TFT) bleiben unverändert im Repository und werden nicht
weiterentwickelt. Die zugehörigen KiCad-Dateien (`Schaltung/`) wurden am 2026-08-01
entfernt; als autoritative V2-Quelle gilt ihr letzter Stand in der Git-Historie.

### Betriebsparameter

- **Funkreichweite**: mindestens 30 m Freifeld (volle Schießplatz-Distanz); Startup-Qualitätstest ≥ 90 %
- **Timer-Genauigkeit**: ±1 Sekunde über 240 Sekunden
- **LED-Helligkeit**: 25–100 % über Helligkeits-Poti (LED-Versorgung immer extern: 5 V oder 12 V je nach Jumper im Empfänger — keine USB-Schutzbegrenzung nötig)
- **Batterielaufzeit Sender**: mindestens 8 Stunden bei normaler Nutzung; im Aus-Zustand nur Hardware-Leckströme (Latch vollständig gelöst)
- **Initialisierungszeit Sender**: unter 10 Sekunden vom Einschalten bis zum bedienbaren Konfigurationsmenü (e-Paper-Voll-Refresh + Funk-Qualitätstest eingerechnet)

### Pin-Belegungen

**AUTORITATIVE QUELLE**: KiCad-Projekt `Schaltung-Sender/` bzw. `Schaltung-Empfaenger/`
(Sender: `BogenampelV3.kicad_sch`, Empfänger: `Zusatzplatine-Empfänger.kicad_sch`)

**Alle Pin-Zuweisungen im Code MÜSSEN exakt mit dem Schaltplan übereinstimmen.**
Der aus den KiCad-Netzlisten eingefrorene, verbindliche Pin-Contract liegt in
`specs/004-v3-esp32-port/contracts/hardware-pins.md` (inkl. Connector-Referenzen und
dokumentierter Hardware-Reworks: Helligkeits-Poti D5→D1, BTN1-Teiler R7 47k→240k).

#### Sender (ESP32-S3, GPIO-Nummern)

- **e-Paper**: BUSY=38, RES=48, DC=47, CS=21, CLK=14, MOSI=13
- **Power**: LATCH=16 (HIGH = an halten), Display-Rail LOAD=7
- **Akku/Lader**: ADC_BAT=10 (Teiler 150k/100k, ×2,5), USB_CON=8, C_ST1=11, C_ST2=12, C_PG=17, C_PRG=18
- **Taster**: BTN1 (Power/OK)=15 (aktiv HIGH), BTN2 (Weiter)=9 (INPUT_PULLUP, aktiv LOW)
- **Reserviert**: GPIO 35–37 (Octal-PSRAM des Moduls)

#### Empfänger (XIAO ESP32C3, D-Nummern)

- **WS2812B-Daten**: D10
- **Taster (Debug)**: D7; **Status-LED**: D9
- **Piezo (PWM)**: D2; **Lautstärke-Poti**: D0 (ADC1); **Helligkeits-Poti**: D1 (ADC1, nach Rework von D5)
- **Lüfter**: PWM=D6, Tacho=D8

## Entwicklungs-Workflow

### Code-Standards

- Alle Zustandsvariablen MÜSSEN volatile deklariert sein, wenn in Interrupts/Timer-Callbacks verwendet
- Magic Numbers MÜSSEN durch benannte Konstanten ersetzt werden
- Globale Variablen MÜSSEN minimiert und klar dokumentiert sein
- Timer-Interrupts/esp_timer-Callbacks MÜSSEN korrekt konfiguriert und dokumentiert sein

### Test-Workflow

1. **Unit-Tests**: Logik-Funktionen isoliert testen (State-Machine-Übergänge)
2. **Hardware-in-the-Loop**: Testen mit echter Hardware in Labor-Umgebung
3. **Reichweitentests**: Funkübertragung unter realistischen Bedingungen
4. **Langzeittests**: Batterielaufzeit und Stabilitätstests (mindestens 8 Stunden)
5. **Feldtests**: Finale Validierung unter echten Einsatzbedingungen

### Review-Anforderungen

- Code-Reviews MÜSSEN Pin-Belegungen gegen den KiCad-Schaltplan der jeweiligen Hardware-Generation prüfen (V3: `Schaltung-Sender/` bzw. `Schaltung-Empfaenger/`)
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
- **Schaltung-Sender/**, **Schaltung-Empfaenger/**: Autoritative Hardware-Pin-Belegungen V3 (KiCad)
- **specs/004-v3-esp32-port/contracts/hardware-pins.md**: Eingefrorener Pin-Contract V3
- **Schaltung/** (entfernt 2026-08-01): KiCad-Projekt der eingefrorenen V2-Hardware — nur noch in der Git-Historie

**Version**: 2.2.0 | **Ratifiziert**: 2025-12-04 | **Zuletzt geändert**: 2026-06-11
