# Feature Specification: Batterieüberwachung und Stromquellen-Anzeige

**Feature Branch**: `001-battery-monitoring-display`
**Created**: 2025-12-13
**Status**: Draft
**Input**: User description: "Die Schaltung hat einen Spannungsteiler an A7 bekommen. Er ist mit einem Verhältnis 1:1 (jeweils 10kOhm) bestückt und misst die Batteriespannung, die den Arduino mit einem 9V Block versorgt. Der 9V Block kann mit einem Schalter zugeschaltet werden. Wird der Arduino über den 9V Block versorgt, so zeigt er im Display die aktuell verbleibende Batterieskapazität in Prozent an. Ist der 9V Block nicht per Schalter zugeschaltet, der Arduino ist aber dennoch in Betrieb, so erfolgt die Spannungsversorgung über die USB Buchse. Dies soll ebenfalls auf dem Display angezeigt werden."

## Clarifications

### Session 2025-12-13

- Q: When both the battery switch is ON and USB is connected simultaneously, which power source should take priority? → A: Battery takes priority when switch is ON (recommended hardware design pattern)
- Q: When the measured battery voltage is outside the expected range (e.g., > 9.5V or < 5V), how should the system respond? → A: Display boundary value with visual indicator (e.g., "100%!" or "0%!")
- Q: Where and how should the battery percentage and USB status be displayed on the screen? → A: Dedicated status area (e.g., top-right corner or bottom edge) - always visible
- Q: How should the system handle potentially defective or unreliable voltage measurements (e.g., rapidly fluctuating values, sudden jumps)? → A: Use raw measurements directly without filtering (REVISED: Apply running median filter over last 5 measurements)
- Q: When switching between battery and USB power (or vice versa), should there be any transition indication or immediate switch? → A: Immediate switch - display updates as soon as power source is detected

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Batteriekapazität im Batteriebetrieb anzeigen (Priority: P1)

Als Benutzer möchte ich beim Betrieb mit 9V-Block die verbleibende Batteriekapazität in Prozent auf dem Display sehen, damit ich rechtzeitig erkennen kann, wann ich die Batterie wechseln muss.

**Why this priority**: Dies ist die Kernfunktionalität des Features. Ohne diese Information kann der Benutzer nicht abschätzen, wie lange das Gerät im Batteriebetrieb noch funktioniert. Dies ist besonders wichtig für den mobilen Einsatz.

**Independent Test**: Kann vollständig getestet werden, indem der Schalter auf Batteriebetrieb gestellt wird und die angezeigte Prozentanzeige mit der tatsächlichen Batteriespannung abgeglichen wird. Liefert eigenständigen Wert durch Information über Batteriezustand.

**Acceptance Scenarios**:

1. **Given** der 9V-Block ist per Schalter zugeschaltet und das Gerät ist eingeschaltet, **When** die Batterie vollständig geladen ist (ca. 9V), **Then** zeigt das Display eine Batteriekapazität von ca. 100% an
2. **Given** der 9V-Block ist per Schalter zugeschaltet und das Gerät ist eingeschaltet, **When** die Batteriespannung auf ca. 7V gefallen ist, **Then** zeigt das Display eine entsprechend reduzierte Batteriekapazität an (z.B. ca. 50%)
3. **Given** der 9V-Block ist per Schalter zugeschaltet und das Gerät ist eingeschaltet, **When** die Batteriespannung kritisch niedrig ist (z.B. unter 6V), **Then** zeigt das Display eine niedrige Batteriekapazität an (z.B. unter 20%)

---

### User Story 2 - USB-Betrieb anzeigen (Priority: P2)

Als Benutzer möchte ich auf dem Display sehen, wenn das Gerät über USB mit Strom versorgt wird, damit ich weiß, dass keine Batterie verwendet wird und ich mir keine Sorgen um den Batteriestand machen muss.

**Why this priority**: Diese Funktion ist wichtig für Klarheit über die aktuelle Stromquelle, aber weniger kritisch als die Batterieüberwachung, da im USB-Betrieb keine Überwachung des Ladezustands notwendig ist.

**Independent Test**: Kann vollständig getestet werden, indem der Batterieschalter ausgeschaltet wird, das Gerät aber über USB betrieben wird. Die Anzeige muss deutlich auf USB-Betrieb hinweisen. Liefert eigenständigen Wert durch Klarheit über die Stromversorgungsart.

**Acceptance Scenarios**:

1. **Given** der 9V-Block-Schalter ist ausgeschaltet und das Gerät wird über USB betrieben, **When** das Gerät eingeschaltet ist, **Then** zeigt das Display "USB" oder eine ähnliche Kennzeichnung für USB-Betrieb an
2. **Given** der 9V-Block-Schalter ist ausgeschaltet und das Gerät wird über USB betrieben, **When** das Gerät eingeschaltet ist, **Then** wird keine Batteriekapazität in Prozent angezeigt

---

### User Story 3 - Umschalten zwischen Batterie- und USB-Betrieb (Priority: P3)

Als Benutzer möchte ich nahtlos zwischen Batterie- und USB-Betrieb wechseln können, wobei das Display automatisch die korrekte Stromquelle anzeigt, damit ich flexibel zwischen mobilem und stationärem Betrieb wechseln kann.

**Why this priority**: Diese Funktion erhöht die Benutzerfreundlichkeit, ist aber eine Erweiterung der beiden vorherigen Stories und daher nachrangig.

**Independent Test**: Kann getestet werden, indem während des Betriebs zwischen Batterie und USB gewechselt wird und die Display-Anzeige beobachtet wird. Liefert eigenständigen Wert durch erhöhte Flexibilität.

**Acceptance Scenarios**:

1. **Given** das Gerät läuft im Batteriebetrieb mit angezeigter Prozentanzeige, **When** der Batterieschalter ausgeschaltet und USB angeschlossen wird, **Then** wechselt die Anzeige von Batterieprozent auf USB-Anzeige
2. **Given** das Gerät läuft im USB-Betrieb mit USB-Anzeige, **When** USB getrennt und der Batterieschalter eingeschaltet wird, **Then** wechselt die Anzeige von USB auf Batterieprozentanzeige
3. **Given** das Gerät wechselt die Stromquelle, **When** der Wechsel erfolgt, **Then** erfolgt die Aktualisierung der Anzeige sofort ohne Übergangsanimation (innerhalb der normalen Messzykluszeit von maximal 5 Sekunden)

---

### Edge Cases

- Was passiert, wenn weder Batterie noch USB angeschlossen sind? (Gerät sollte ausgeschaltet sein, keine Anzeige)
- Was passiert, wenn die Batterie während des Betriebs leer wird? (Die Prozentanzeige wird kontinuierlich aktualisiert, das Gerät geht aus wenn die Batterie vollständig entladen ist. Der Benutzer ist selbst dafür verantwortlich, die Prozentanzeige zu beobachten.)
- Was passiert, wenn der Spannungsteiler defekt ist und fehlerhafte Werte liefert? (Der Median-Filter reduziert den Einfluss einzelner fehlerhafter Messungen, persistente Fehler werden jedoch weiterhin angezeigt)
- Was passiert, wenn die Batteriespannung außerhalb des erwarteten Bereichs liegt (z.B. > 9,5V oder < 5V)? (Das System zeigt den Grenzwert 0% oder 100% mit einem visuellen Indikator an, z.B. "0%!" oder "100%!", um auf eine ungewöhnliche Messung hinzuweisen)
- Was passiert, wenn sowohl Batterie als auch USB gleichzeitig angeschlossen sind? (Batterie hat Vorrang wenn der Schalter eingeschaltet ist - die Schalterstellung bestimmt die aktive Stromquelle)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST kontinuierlich die Batteriespannung über den vorhandenen Spannungsteiler messen
- **FR-002**: System MUST aus der gemessenen Spannung die Batteriespannung berechnen (unter Berücksichtigung des 1:1-Teilers)
- **FR-003**: System MUST einen laufenden Median-Filter über die letzten 5 Spannungsmessungen anwenden, um Messfehler und Schwankungen zu reduzieren
- **FR-003a**: System MUST die gefilterte Batteriespannung in eine Prozentanzeige umrechnen (0-100%)
- **FR-004**: System MUST die Batteriekapazität in Prozent auf dem Display anzeigen, wenn das Gerät über den 9V-Block versorgt wird
- **FR-005**: System MUST erkennen, ob die Stromversorgung über USB oder Batterie erfolgt
- **FR-006**: System MUST auf dem Display "USB" oder eine ähnliche Kennzeichnung anzeigen, wenn das Gerät über USB versorgt wird
- **FR-007**: System MUST die Anzeige sofort aktualisieren, wenn zwischen Batterie- und USB-Betrieb gewechselt wird, ohne Übergangsanimation oder Verzögerung
- **FR-008**: System MUST die Batteriespannung in regelmäßigen Abständen aktualisieren (mindestens alle 5 Sekunden)
- **FR-009**: System MUST zwischen Batterie- und USB-Betrieb anhand der Schalterstellung unterscheiden können
- **FR-010**: System MUST die Batterie als primäre Stromquelle verwenden, wenn der Batterieschalter eingeschaltet ist, auch wenn USB gleichzeitig angeschlossen ist
- **FR-011**: System MUST bei Batteriespannungen außerhalb des erwarteten Bereichs (< 6V oder > 9,6V) den entsprechenden Grenzwert (0% bzw. 100%) mit einem visuellen Indikator anzeigen (z.B. "0%!" oder "100%!")
- **FR-012**: System MUST die Stromversorgungsinformationen (Batteriekapazität oder USB-Status) in einem dedizierten Statusbereich des Displays anzeigen (z.B. obere rechte Ecke oder unterer Rand), der dauerhaft sichtbar ist

### Key Entities

- **Batteriespannung**: Die vom Spannungsteiler gemessene Spannung, die die aktuelle Ladung des 9V-Blocks repräsentiert
- **Stromquellentyp**: Entweder "Batterie" oder "USB", gibt an, welche Stromquelle aktuell aktiv ist
- **Batteriekapazität**: Prozentuale Darstellung des Batterieladezustands (0-100%), abgeleitet aus der gemessenen Batteriespannung

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Benutzer können durch einen Blick auf das Display die verbleibende Batteriekapazität ablesen (Prozentanzeige sichtbar innerhalb von 1 Sekunde nach Einschalten)
- **SC-002**: Die angezeigte Batteriekapazität weicht um maximal 10% von der tatsächlichen Kapazität ab (gemessen durch Referenzgerät)
- **SC-003**: Benutzer können eindeutig erkennen, ob das Gerät über USB oder Batterie betrieben wird (unterschiedliche Display-Anzeigen)
- **SC-004**: Der Wechsel zwischen Batterie- und USB-Betrieb wird auf dem Display sofort angezeigt, sobald die nächste Messung erfolgt (innerhalb der normalen Aktualisierungsrate von maximal 5 Sekunden)
- **SC-005**: Die Batteriekapazitätsanzeige auf dem Display aktualisiert sich mindestens alle 5 Sekunden

### Assumptions

- **A-001**: Eine neue 9V-Alkaline-Batterie hat eine Spannung von ca. 9,0-9,6V und gilt als 100% geladen
- **A-002**: Eine 9V-Batterie gilt als leer, wenn die Spannung unter ca. 6,0V fällt (0%)
- **A-003**: Die Umrechnung von Spannung in Prozent erfolgt linear zwischen Mindest- und Maximalspannung
- **A-004**: Der Spannungsteiler funktioniert korrekt und liefert zuverlässige Messwerte
- **A-005**: Das Display hat ausreichend Platz für die Anzeige der Batteriekapazität und des USB-Status in einem dedizierten Statusbereich
- **A-006**: Die Erkennung der Stromquelle erfolgt über die Messung am Spannungsteiler (niedrige/keine Spannung = USB, normale Batteriespannung = Batterie)
- **A-007**: Der Median-Filter über 5 Messungen bietet ausreichende Robustheit gegenüber Messfehlern und elektrischen Störungen, während die Reaktionszeit auf echte Spannungsänderungen akzeptabel bleibt
