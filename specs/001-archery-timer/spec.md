# Feature-Spezifikation: Drahtloses Timer-System für Bogenschießplätze

**Feature-Branch**: `001-archery-timer`
**Erstellt**: 2025-12-04
**Status**: Entwurf
**Eingabe**: Benutzerbeschreibung: "Bitte nimm die Readme.md im verzeichnis als initiales Anforderungsdokument."

## Benutzerszenarien & Tests *(verpflichtend)*

### Benutzer-Story 1 - Fernsteuerung des Timers von der Schießlinie (Priorität: P1)

Der Standaufsicht steht an der Schießlinie und steuert die Timer-Anzeige im Zielbereich mit einer drahtlosen Fernbedienung. Wenn das Schießen beginnt, drückt er eine Taste, um einen Vorbereitungs-Countdown zu starten, gefolgt vom Haupt-Schieß-Timer. Wenn es Zeit ist, die Scheiben zu wechseln, stoppt er den Timer per Fernsteuerung.

**Warum diese Priorität**: Dies ist die Kernfunktionalität, die einen sicheren Standbetrieb ermöglicht. Ohne Fernsteuerung des Timers kann das System seinen Hauptzweck - die Koordination der Schießphasen aus der Distanz - nicht erfüllen.

**Unabhängiger Test**: Kann vollständig getestet werden, indem der Sender in Schießentfernung vom Empfänger platziert wird, die Steuertaste gedrückt wird und überprüft wird, ob der Timer wie erwartet startet, herunterzählt und stoppt. Liefert sofortigen Wert durch sichere Fernsteuerung der Schießphasen.

**Akzeptanzszenarien**:

1. **Gegeben** der Timer ist gestoppt (zeigt 000, rotes Licht), **Wenn** die Standaufsicht die Start-Taste drückt, **Dann** zeigt die Anzeige einen 10-Sekunden-Vorbereitungs-Countdown in Gelb, gefolgt von der konfigurierten Schießzeit (120s oder 240s) in Grün
2. **Gegeben** der Timer zählt während der Schießphase herunter, **Wenn** die Standaufsicht die Stop-Taste drückt, **Dann** stoppt der Timer sofort und zeigt 000 mit rotem Licht
3. **Gegeben** der Timer erreicht während des Countdowns Null, **Wenn** die Zeit abläuft, **Dann** zeigt die Anzeige 000 mit rotem Licht und spielt ein hörbares Signal

---

### Benutzer-Story 2 - Visuelle Timer-Statusanzeige für Schützen (Priorität: P2)

Schützen am Stand benötigen klare visuelle Rückmeldung über die aktuelle Schießphase. Die Timer-Anzeige verwendet Ampelfarben (Rot, Gelb, Grün) und einen großen 3-stelligen Countdown, um die verbleibende Zeit anzuzeigen, damit Schützen aus der Entfernung sehen können, ob das Schießen erlaubt ist und wie viel Zeit verbleibt.

**Warum diese Priorität**: Visuelles Feedback ist essenziell für Standsicherheit und Schützenkoordination, aber die Fernsteuerungsfunktionalität (P1) muss zuerst funktionieren, bevor die visuelle Anzeige relevant wird.

**Unabhängiger Test**: Kann getestet werden, indem die Anzeigeeinheit während eines kompletten Timer-Zyklus beobachtet wird und überprüft wird, ob die Farben den korrekten Phasen entsprechen und die Countdown-Zahlen klar sichtbar und akkurat sind. Liefert Wert durch klare visuelle Kommunikation an alle Standbenutzer.

**Akzeptanzszenarien**:

1. **Gegeben** der Timer ist im Stopp-Zustand, **Wenn** ein Schütze auf die Anzeige schaut, **Dann** sieht er "000" in rotem Licht, was anzeigt, dass Schießen verboten ist
2. **Gegeben** die Vorbereitungsphase startet, **Wenn** der 10-Sekunden-Countdown beginnt, **Dann** zeigt die Anzeige Countdown-Zahlen in gelbem Licht
3. **Gegeben** die Schießphase ist aktiv, **Wenn** mehr als 30 Sekunden verbleiben, **Dann** zeigt die Anzeige Countdown-Zahlen in grünem Licht
4. **Gegeben** die Schießphase endet, **Wenn** 30 Sekunden oder weniger verbleiben, **Dann** wechselt die Anzeige von Grün zu Gelb, während der Countdown weiterläuft
5. **Gegeben** die Anzeige ist in beliebiger Entfernung bis zu 50 Metern in Betrieb, **Wenn** ein Schütze auf die Anzeige schaut, **Dann** sind die Zahlen und Farben klar lesbar

---

### Benutzer-Story 3 - Akustische Signale für Phasenübergänge (Priorität: P3)

Standbenutzer hören unterschiedliche Töne, wenn sich Timer-Phasen ändern (Vorbereitung startet, Schießen startet, Zeit abgelaufen), sodass sie Bestätigung erhalten, auch wenn sie nicht auf die Anzeige schauen. Die Standaufsicht kann die Lautstärke mit einem Drehregler an der Anzeigeeinheit einstellen.

**Warum diese Priorität**: Akustisches Feedback erhöht Benutzerfreundlichkeit und Sicherheit, ist aber ergänzend zur visuellen Steuerung (P1) und Anzeige (P2). Das System funktioniert angemessen ohne Ton.

**Unabhängiger Test**: Kann getestet werden, indem jeder Phasenübergang ausgelöst wird und auf die korrekte Tonfrequenz gehört wird, dann die Lautstärkeregelung angepasst wird und überprüft wird, ob sich die Lautstärke ändert. Liefert Wert durch multisensorisches Feedback für bessere Standwahrnehmung.

**Akzeptanzszenarien**:

1. **Gegeben** die Vorbereitungsphase startet, **Wenn** der Timer den 10-Sekunden-Countdown beginnt, **Dann** spielt ein tiefer Ton (1500 Hz) über den Buzzer
2. **Gegeben** die Schießphase startet, **Wenn** der Countdown von Gelb zu Grün wechselt, **Dann** spielt ein hoher Ton (2500 Hz)
3. **Gegeben** der Timer läuft ab, **Wenn** der Countdown Null erreicht, **Dann** spielt ein mittlerer Ton (2000 Hz)
4. **Gegeben** der Ton ist zu laut oder zu leise, **Wenn** die Standaufsicht das Lautstärke-Potentiometer anpasst, **Dann** erhöht oder verringert sich die Buzzer-Lautstärke entsprechend

---

### Benutzer-Story 4 - Konfigurierbare Timer-Dauer (Priorität: P4)

Die Standaufsicht wählt zwischen 120-Sekunden- oder 240-Sekunden-Schießzeiten mit einem Schalter an der Anzeigeeinheit vor dem Start einer Session, wodurch das System verschiedene Trainingsszenarien und Wettkampfformate unterstützen kann.

**Warum diese Priorität**: Moduswahl fügt Flexibilität hinzu, ist aber nicht kritisch für den Grundbetrieb. Eine feste Dauer (P1-P3) würde immer noch Kernwert liefern.

**Unabhängiger Test**: Kann getestet werden, indem der Modusschalter zwischen Positionen umgeschaltet wird, der Timer gestartet wird und überprüft wird, ob die korrekte Dauer (120s oder 240s) herunterzählt. Liefert Wert durch Unterstützung verschiedener Trainingsprotokolle.

**Akzeptanzszenarien**:

1. **Gegeben** der Modusschalter ist auf 120s-Position gesetzt, **Wenn** die Standaufsicht den Timer startet, **Dann** zählt die Schießphase von 120 Sekunden herunter
2. **Gegeben** der Modusschalter ist auf 240s-Position gesetzt, **Wenn** die Standaufsicht den Timer startet, **Dann** zählt die Schießphase von 240 Sekunden herunter

---

### Benutzer-Story 5 - Sichere Entwicklung und Wartung (Priorität: P5)

Der Techniker programmiert oder wartet die Anzeigeeinheit, indem er sie per USB mit einem Computer verbindet. Um Schäden am USB-Port durch übermäßige LED-Stromaufnahme zu verhindern, aktiviert er den Entwicklungsmodus mit einem Jumper, der die LED-Helligkeit auf 3% reduziert und sicheren Betrieb an Standard-USB-Stromversorgung ermöglicht.

**Warum diese Priorität**: Dies ist eine technische Wartungsfunktion, keine benutzerseitige Funktionalität. Sie ist wichtig für Systemsicherheit während der Entwicklung, beeinflusst aber nicht den normalen Betrieb.

**Unabhängiger Test**: Kann getestet werden, indem die Anzeigeeinheit mit gesetztem Jumper an einen Standard-USB-Port angeschlossen wird, Firmware hochgeladen wird und überprüft wird, ob die LEDs bei reduzierter Helligkeit betrieben werden ohne den USB-Port zu überlasten. Liefert Wert durch sichere Firmware-Updates und Tests.

**Akzeptanzszenarien**:

1. **Gegeben** der Entwicklungsmodus-Jumper ist installiert (Pin D2 → GND), **Wenn** die Anzeigeeinheit per USB mit Strom versorgt wird, **Dann** ist die LED-Helligkeit auf 3% des Maximums begrenzt
2. **Gegeben** der Entwicklungsmodus-Jumper ist entfernt, **Wenn** die Anzeigeeinheit per USB-Powerbank mit Strom versorgt wird, **Dann** betreiben LEDs mit voller Helligkeit
3. **Gegeben** die Anzeigeeinheit ist an einen Standard-USB-Port angeschlossen (0,5-3A Limit), **Wenn** der Entwicklungsmodus aktiv ist, **Dann** bleibt die Stromaufnahme innerhalb der USB-Spezifikationen und beschädigt den Port nicht

---

### Grenzfälle

- Was passiert, wenn das Funksignal während des Countdowns unterbrochen wird (z.B. Senderbatterie leer, Signal blockiert)?
- Wie handhabt das System schnell wiederholte Tastendrücke am Sender?
- Was passiert, wenn der Modusschalter geändert wird, während der Timer bereits läuft?
- Wie verhält sich das System beim ersten Einschalten (Initialzustand)?
- Was passiert, wenn der Sender außerhalb der Funkreichweite ist (>100m Freifeld, >50m Innenraum)?
- Wie handhabt die Anzeigeeinheit unzureichende Stromversorgung (schwache Powerbank)?

## Anforderungen *(verpflichtend)*

### Funktionale Anforderungen

- **FR-001**: System MUSS aus zwei drahtlos verbundenen Einheiten bestehen: einem Sender (Steuereinheit) an der Schießlinie und einem Empfänger (Anzeigeeinheit) im Zielbereich
- **FR-002**: Sender MUSS START- und STOP-Befehle per 2,4 GHz Funkkommunikation an den Empfänger senden
- **FR-003**: Funkkommunikation MUSS zuverlässig bei Entfernungen von 20-50 Metern innen und bis zu 100 Metern im Freifeld funktionieren
- **FR-004**: Empfänger MUSS Moduswahl (120 Sekunden oder 240 Sekunden) per physischem Schalter vor Timer-Start akzeptieren
- **FR-005**: System MUSS einen 10-Sekunden-Vorbereitungs-Countdown (gelbe Phase) vor Beginn des Haupt-Schieß-Countdowns ausführen
- **FR-006**: Anzeige MUSS verbleibende Zeit als 3-stelligen Countdown (in Sekunden) sichtbar aus Schießentfernung zeigen
- **FR-007**: Anzeige MUSS Ampelfarben verwenden, um Timer-Status anzuzeigen:
  - Rot: Timer gestoppt oder abgelaufen, Schießen verboten
  - Gelb: Vorbereitungsphase (10 Sekunden) oder finale Warnung (letzte 30 Sekunden des Schießens)
  - Grün: Aktive Schießphase (vom Start bis 30 Sekunden verbleibend)
- **FR-008**: Anzeige MUSS "000" in Rot zeigen, wenn Timer gestoppt oder abgelaufen ist
- **FR-009**: System MUSS unterschiedliche Töne bei Phasenübergängen ausgeben:
  - 1500 Hz wenn Vorbereitungs-Countdown startet
  - 2500 Hz wenn Schieß-Countdown startet
  - 2000 Hz wenn Timer abläuft
- **FR-010**: Anzeigeeinheit MUSS einstellbare Audio-Lautstärke per Potentiometer-Steuerung bieten
- **FR-011**: Sender MUSS Betriebsstatus per LED-Indikatoren anzeigen
- **FR-012**: Empfänger MUSS Betriebsstatus per LED-Indikatoren anzeigen
- **FR-013**: System MUSS manuelles Stoppen zu jedem Zeitpunkt während des Countdowns durch Drücken der Sender-Taste unterstützen
- **FR-014**: System MUSS automatisch stoppen und Rot (000) anzeigen, wenn Countdown Null erreicht
- **FR-015**: Empfänger MUSS Entwicklungsmodus per Jumper unterstützen, der LED-Helligkeit auf 3% für sichere USB-betriebene Programmierung reduziert
- **FR-016**: Empfänger MUSS bei voller LED-Helligkeit betrieben werden, wenn Entwicklungsmodus deaktiviert ist und von externer Powerbank mit Strom versorgt wird
- **FR-017**: Sender MUSS Stromversorgung von entweder 9V-Batterie oder 5V-USB-Verbindung mit Ein/Aus-Schalter unterstützen
- **FR-018**: Empfänger MUSS von 5V-USB-Powerbank-Versorgung betrieben werden

### Schlüssel-Entitäten

- **Timer-Zustand**: Repräsentiert den aktuellen Betriebsstatus des Systems (GESTOPPT, VORBEREITUNG, SCHIESSEN, ABGELAUFEN) und verbleibende Zeit in Sekunden
- **Befehlsnachricht**: Repräsentiert drahtlose Kommunikation zwischen Sender und Empfänger (START-, STOP-Befehle)
- **Anzeige-Konfiguration**: Repräsentiert benutzerkonfigurierbare Einstellungen (Timer-Dauer-Modus: 120s/240s, Lautstärkepegel, Entwicklungsmodus: an/aus)
- **Phasenübergang**: Repräsentiert Änderungen im Timer-Zustand, die visuelle und akustische Rückmeldung auslösen (gestoppt→vorbereitung, vorbereitung→schießen, schießen→warnung, schießen→abgelaufen)

## Erfolgskriterien *(verpflichtend)*

### Messbare Ergebnisse

- **SC-001**: Standaufsicht kann Schießphasen aus 20+ Metern Entfernung initiieren und steuern, ohne zum Zielbereich zu gehen
- **SC-002**: Schützen können die Countdown-Anzeige klar ablesen und die aktuelle Phase (Rot/Gelb/Grün) von der Schießlinie 20+ Meter entfernt identifizieren
- **SC-003**: System hält zuverlässige Funkkommunikation über 50 Meter innen und 100 Meter im Freifeld mit weniger als 1% Befehlsausfall-Rate aufrecht
- **SC-004**: Timer-Genauigkeit hält ±1 Sekunde Präzision über volle 240-Sekunden-Countdown-Dauer aufrecht
- **SC-005**: Phasenübergänge erfolgen innerhalb von 500 Millisekunden nachdem Timer Übergangsschwelle erreicht
- **SC-006**: Akustische Signale sind bei typischen Bogenschießplatz-Umgebungsgeräuschpegeln (50-70 dB) hörbar, wenn Lautstärke angemessen eingestellt ist
- **SC-007**: System arbeitet kontinuierlich für mindestens 8 Stunden mit Senderbatterie und 8 Stunden mit Empfänger-Powerbank unter normalen Nutzungsmustern (mehrere Timer-Zyklen pro Stunde)
- **SC-008**: Anzeigeeinheit kann sicher per Standard-USB-Port programmiert werden ohne Hardwareschaden, wenn Entwicklungsmodus aktiviert ist
- **SC-009**: 95% der Standaufsichten können den Sender bedienen (Timer starten/stoppen) ohne Dokumentation zu konsultieren nach initialer Demonstration
- **SC-010**: System-Initialisierung (Einschalten bis Bereitschaftszustand) wird in unter 5 Sekunden abgeschlossen

## Klärungen

### Sitzung 2025-12-04

*Dieser Abschnitt dokumentiert Klärungen zur Spezifikation aus der heutigen Sitzung.*
