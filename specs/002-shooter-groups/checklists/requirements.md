# Spezifikations-Qualitäts-Checkliste: Schützengruppen-Anzeige

**Feature**: 002-shooter-groups
**Datum**: 2025-12-04
**Version**: 1.0

## Inhaltsqualität

### ✅ Benutzer-fokussiert (keine Implementierungsdetails)

- [ ] **US1 (P1)**: Automatische Gruppenanzeige - beschreibt WAS (automatische Anzeige, Rhythmus), nicht WIE (GPIO-Pins, LED-Steuerung)
- [ ] **US2 (P2)**: Visuelle Gruppenanzeige - beschreibt Benutzererfahrung (erkennbar aus 20m), nicht Hardware-Details
- [ ] **US3 (P3)**: Manuelle Umschaltung - beschreibt Bedienung (Taster bei gestopptem Timer), nicht Code-Logik
- [ ] **US4 (P1)**: Passen-Rhythmus - beschreibt Regelkonformität, nicht State-Machine-Implementierung

### ✅ Testbare Akzeptanzkriterien

- [ ] **US1**: 5 Akzeptanzszenarien mit "Gegeben-Wenn-Dann" Format (P1→AB, AB→CD, P2→CD, CD→AB, P3→AB)
- [ ] **US2**: 3 Akzeptanzszenarien (AB aktiv sichtbar, CD aktiv sichtbar, 20m lesbar)
- [ ] **US3**: 3 Akzeptanzszenarien (AB→CD toggle, CD→AB toggle, Timer läuft = ignore)
- [ ] **US4**: 4 Akzeptanzszenarien (P1 Start AB, P2 Start CD, P3 Start AB, P4 Start CD)

### ✅ Messbare Erfolgskriterien

- [ ] **SC-001**: "20+ Meter Entfernung" - klar messbar mit Maßband
- [ ] **SC-002**: "10 komplette Passen ohne Abweichung" - zählbar
- [ ] **SC-003**: "2 Sekunden nach Timer-Ablauf" - messbar mit Stoppuhr
- [ ] **SC-004**: "500ms Latenz" - messbar mit Oszilloskop/Kamera
- [ ] **SC-005**: "100% der Fälle ignoriert" - binär testbar
- [ ] **SC-006**: "95% der Standaufsichten" - quantifizierbar durch User Testing
- [ ] **SC-007**: "Tageslicht und Dämmerung" - qualitativ, aber klar definiert
- [ ] **SC-008**: "100% der Fälle startet bei Passe 1" - binär testbar

## Anforderungsvollständigkeit

### ⚠️ Funktionale Anforderungen

**Vollständig spezifiziert**:
- [x] **FR-001**: LED-Felder (A/B und C/D)
- [x] **FR-002**: Exklusives Leuchten (genau eines aktiv)
- [x] **FR-003**: Gruppenwechsel-Taster am Sender
- [x] **FR-004**: Taster nur bei Stopp-Zustand
- [x] **FR-005**: Toggle-Funktion (A/B ↔ C/D)
- [x] **FR-006**: Automatischer Wechsel bei Timer-Ablauf
- [x] **FR-007**: Bogensport-Rhythmus (P1: AB→CD, P2: CD→AB, etc.)
- [x] **FR-008**: Passen-Zähler für Rhythmus
- [x] **FR-009**: Power-On State (Passe 1, A/B)
- [x] **FR-010**: Sender sendet Gruppenwechsel per Funk
- [x] **FR-011**: Empfänger empfängt Gruppenwechsel
- [x] **FR-012**: Lesbarkeit aus 20+ Meter

**Fehlend oder unterspecifiziert**:
- [ ] ⚠️ **Grenzfall 1**: Was passiert bei manuellem Gruppenwechsel + Timer-Start? Bleibt Rhythmus synchron oder Reset?
  - **Annahme #5** sagt "setzt NICHT zurück", aber keine FR dafür
  - **Empfehlung**: FR-013 hinzufügen: "Manueller Gruppenwechsel DARF den Passen-Zähler NICHT zurücksetzen"

- [ ] ⚠️ **Grenzfall 2**: Power-Cycle Verhalten klar?
  - **FR-009** sagt "Passe 1, A/B", aber was wenn System mitten in Passe 3 neugestartet wird?
  - **Empfehlung**: Klarstellen in FR-009: "Nach JEDEM Power-Cycle startet System bei Passe 1, Gruppe A/B"

- [ ] ⚠️ **Grenzfall 3**: Manueller Stopp (nicht Ablauf) - Gruppe wechselt?
  - **FR-006** sagt "bei Timer-Ablauf", aber was bei manuellem STOP?
  - **Empfehlung**: FR-006 erweitern: "...NUR bei automatischem Ablauf (nicht bei manuellem STOP)"

- [ ] ⚠️ **Grenzfall 4**: Passen-Zähler Anpassung bei manuellen Eingriffen?
  - Wenn Timer manuell gestoppt wird, bleibt Passen-Zähler gleich oder inkrement?
  - **Empfehlung**: FR-008 erweitern mit "Passen-Zähler inkrementiert NUR bei vollständigem Timer-Ablauf"

### ✅ Schlüssel-Entitäten

- [x] **Schützengruppe**: Klar definiert (A/B oder C/D, Zustand aktiv/inaktiv)
- [x] **Passen-Zustand**: Klar definiert (Nummer, Position innerhalb Passe)
- [x] **Rhythmus-Regel**: Klar definiert (ungerade: AB→CD, gerade: CD→AB)
- [x] **Gruppenwechsel-Befehl**: Klar definiert (manuell oder automatisch)

### ⚠️ Abhängigkeiten

- [x] **Feature 001**: Erwähnt in Annahmen (Funkprotokoll kann erweitert werden)
- [ ] ⚠️ **Integration**: Wie interagiert Gruppenwechsel mit bestehender State-Machine?
  - **Empfehlung**: In Plan-Phase klären, ob neuer State oder paralleler Zustand

## Feature-Bereitschaft

### ✅ Prioritäten

- [x] **P1 Stories**: US1 (Auto-Anzeige), US4 (Rhythmus) - kritisch für Kernfunktion
- [x] **P2 Stories**: US2 (Visuelle Anzeige) - essenziell, aber US1 muss zuerst funktionieren
- [x] **P3 Stories**: US3 (Manuelle Umschaltung) - Nice-to-have, nicht kritisch

### ✅ Unabhängige Testbarkeit

- [x] **US1**: Kann isoliert getestet werden (mehrere Timer-Zyklen, Rhythmus protokollieren)
- [x] **US2**: Kann isoliert getestet werden (Sichtbarkeit aus 20m messen)
- [x] **US3**: Kann isoliert getestet werden (Taster im Stopp-Zustand, Timer läuft)
- [x] **US4**: Kann isoliert getestet werden (6+ Passen durchlaufen, Muster validieren)

### ✅ Inkrementeller Wert

- [x] **US1**: Liefert sofort Wert (Schützen sehen aktive Gruppe)
- [x] **US2**: Liefert Wert durch klare Unterscheidung
- [x] **US3**: Liefert Flexibilität bei Unterbrechungen
- [x] **US4**: Liefert Regelkonformität für Wettkampf

## Annahmen-Validierung

### ✅ Technische Machbarkeit

- [x] **Annahme 1**: WS2812B für LED-Felder - konsistent mit Feature 001
- [x] **Annahme 2**: GPIO-Pin verfügbar - Verweis auf `/Schaltung/Schaltplan.pdf` korrekt
- [x] **Annahme 3**: Funkprotokoll erweiterbar - realistisch (nRF24 Payload ist flexibel)
- [x] **Annahme 4**: Keine EEPROM-Persistenz - Vereinfachung für MVP, akzeptabel
- [x] **Annahme 5**: Manueller Wechsel setzt Rhythmus NICHT zurück - Design-Entscheidung getroffen

### ⚠️ Fehlende Annahmen

- [ ] ⚠️ **LED-Anzahl**: Wie viele LEDs pro Feld (A/B, C/D)? Reichen sie für 20m Sichtbarkeit?
  - **Empfehlung**: Annahme hinzufügen mit konkreter LED-Anzahl (z.B. "mindestens 20 LEDs pro Feld")

- [ ] ⚠️ **Stromverbrauch**: Zusätzliche LEDs erhöhen Last - passt USB/Powerbank noch?
  - **Empfehlung**: Annahme hinzufügen über Gesamt-LED-Count und Strombudget

## Zusammenfassung

### ✅ Stärken

1. **Klare Prioritäten**: P1/P2/P3 logisch begründet
2. **Testbare Szenarien**: Alle User Stories haben Gegeben-Wenn-Dann Format
3. **Messbare Erfolge**: SC-001 bis SC-008 sind quantifizierbar
4. **Regelkonformität**: Bogensport-Rhythmus korrekt dokumentiert
5. **Annahmen dokumentiert**: 5 klare Annahmen mit Rationale

### ⚠️ Verbesserungspotenzial

1. **Grenzfälle → FRs**: 4 Grenzfall-Fragen sollten als funktionale Anforderungen formuliert werden:
   - FR-013: Manueller Wechsel setzt Rhythmus NICHT zurück
   - FR-009 erweitern: Power-Cycle startet IMMER bei Passe 1
   - FR-006 erweitern: Gruppenwechsel NUR bei automatischem Ablauf
   - FR-008 erweitern: Passen-Zähler inkrement NUR bei Ablauf

2. **LED-Spezifikation**: Fehlende Annahme über LED-Anzahl pro Feld (A/B, C/D)

3. **Strombudget**: Fehlende Annahme über Gesamt-LED-Count und Powerbank-Kapazität

## Empfohlene Aktionen

### Priorität 1 (Blocker für Implementation)

1. **Grenzfall-Klärung**: Wandle die 4 Grenzfall-Fragen in funktionale Anforderungen um (FR-013, FR-006/FR-008/FR-009 erweitern)
2. **LED-Count**: Definiere Anzahl LEDs pro Feld (A/B, C/D) für 20m Sichtbarkeit

### Priorität 2 (Nice-to-have für Plan-Phase)

3. **Strombudget**: Berechne Gesamt-LED-Count (Timer: ~155 + A/B: X + C/D: X) und validiere gegen Powerbank
4. **Integration Diagramm**: Skizziere wie Gruppenwechsel in bestehende State-Machine integriert wird

## Validierungs-Ergebnis

**Status**: ⚠️ **BEDINGT BESTANDEN** - Spec ist gut strukturiert, aber 4 Grenzfälle müssen als FRs formalisiert werden

**Nächster Schritt**:
1. Entweder `/speckit.clarify` ausführen (um die 4 Grenzfälle interaktiv zu klären)
2. Oder direkt FRs manuell hinzufügen basierend auf Annahme #5

**Empfehlung**: `/speckit.clarify` verwenden, da einige Grenzfälle Design-Entscheidungen erfordern (z.B. Passen-Zähler bei manuellem Stopp)
