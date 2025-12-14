# Feature-Spezifikation: Schützengruppen-Anzeige (A/B und C/D)

**Feature-Branch**: `002-shooter-groups`
**Erstellt**: 2025-12-04
**Status**: Entwurf
**Eingabe**: Benutzerbeschreibung: "Die Ampel soll auch anzeigen, welche Bogenschützen in der aktuellen Passe gerade schießen..."

## Clarifications

### Session 2025-12-04

- Q: When the timer is manually stopped (STOP command sent before timer expires naturally), should the group display switch to the next group? → A: Group ALWAYS switches when timer stops (manual or natural expiry)

- Q: When the Standaufsicht manually toggles the group and then starts a timer, what should happen to the rhythm tracking? → A: 4-state cycle with position indicator: (1) A/B+"1" → A/B dann C/D, (2) A/B+"2" → A/B dann neue Passe mit A/B, (3) C/D+"1" → C/D noch einmal dann A/B, (4) C/D+"2" → C/D zweimal. Knopf rotiert durch Zustände innerhalb 3-Sekunden-Fenster, danach wird gewählter Zustand aktiv.

- Q: When the system undergoes a power-cycle (restart) - for example, if powered off in middle of Pass 3 - what should happen on restart? → A: Always restart at Pass 1, Group A/B+Position1 regardless of previous state (no state persistence)

## Benutzerszenarien & Tests *(verpflichtend)*

### Benutzer-Story 1 - Automatische Gruppenanzeige während Schießphasen (Priorität: P1)

Die Anzeigeeinheit zeigt während jeder Schießphase an, welche Schützengruppe (A/B oder C/D) gerade schießen darf. Die Anzeige wechselt automatisch nach jedem abgelaufenen Timer und folgt dabei dem Bogensport-Rhythmus: Passe 1 beginnt mit A/B→C/D, Passe 2 mit C/D→A/B, Passe 3 wieder A/B→C/D, usw.

**Warum diese Priorität**: Dies ist die Kernfunktion, die Schützen klar signalisiert, welche Gruppe gerade aktiv ist. Ohne diese Anzeige können Schützen nicht erkennen, ob sie an der Reihe sind.

**Unabhängiger Test**: Kann getestet werden, indem mehrere Timer-Zyklen durchlaufen werden und überprüft wird, ob die Gruppenanzeige dem korrekten Rhythmus folgt (Passe 1: A/B→C/D, Passe 2: C/D→A/B, Passe 3: A/B→C/D). Liefert sofortigen Wert durch klare Gruppenzuordnung.

**Akzeptanzszenarien**:

1. **Gegeben** System ist im Stopp-Zustand am Anfang von Passe 1, **Wenn** der Timer gestartet wird, **Dann** leuchtet die A/B-Anzeige während der Schießphase
2. **Gegeben** Timer läuft in Passe 1 mit A/B-Anzeige aktiv, **Wenn** der Timer abläuft, **Dann** wechselt die Anzeige automatisch zu C/D für die nächste Schießphase
3. **Gegeben** Timer läuft in Passe 1 mit C/D-Anzeige aktiv, **Wenn** der Timer abläuft, **Dann** wechselt die Anzeige zu C/D (Start von Passe 2)
4. **Gegeben** Timer läuft in Passe 2 mit C/D-Anzeige aktiv, **Wenn** der Timer abläuft, **Dann** wechselt die Anzeige zu A/B (zweiter Teil von Passe 2)
5. **Gegeben** Timer läuft in Passe 2 mit A/B-Anzeige aktiv, **Wenn** der Timer abläuft, **Dann** wechselt die Anzeige zu A/B (Start von Passe 3, Rhythmus wiederholt sich)

---

### Benutzer-Story 2 - Visuelle Gruppenanzeige auf Display (Priorität: P2)

Die Anzeigeeinheit verfügt über zwei separate LED-Felder ("A/B" und "C/D"), von denen immer genau eines leuchtet, um die aktive Schützengruppe anzuzeigen. Die Felder sind aus der Schießlinie klar erkennbar und unterscheidbar.

**Warum diese Priorität**: Visuelle Klarheit ist essenziell, aber die automatische Umschaltlogik (P1) muss zuerst funktionieren.

**Unabhängiger Test**: Kann getestet werden, indem die Anzeigeeinheit während verschiedener Phasen beobachtet wird und überprüft wird, ob immer genau ein Feld leuchtet und die Anzeige aus 20+ Metern lesbar ist. Liefert Wert durch klare visuelle Unterscheidung der Gruppen.

**Akzeptanzszenarien**:

1. **Gegeben** A/B ist die aktive Gruppe, **Wenn** ein Schütze auf die Anzeige schaut, **Dann** sieht er das A/B-Feld leuchtend und C/D-Feld aus
2. **Gegeben** C/D ist die aktive Gruppe, **Wenn** ein Schütze auf die Anzeige schaut, **Dann** sieht er das C/D-Feld leuchtend und A/B-Feld aus
3. **Gegeben** die Anzeige ist 20+ Meter entfernt, **Wenn** ein Schütze die Gruppenanzeige betrachtet, **Dann** kann er klar unterscheiden, welches Feld aktiv ist

---

### Benutzer-Story 3 - Manuelle Gruppenumschaltung bei Bedarf (Priorität: P3)

Die Standaufsicht kann mit einem zusätzlichen Taster am Sender die Gruppenanzeige manuell umschalten (A/B ↔ C/D), allerdings nur wenn aktuell kein Timer läuft. Dies ermöglicht Korrekturen bei unerwarteten Unterbrechungen oder Anpassungen.

**Warum diese Priorität**: Manuelle Korrektur ist nützlich, aber nicht kritisch. Das System sollte primär automatisch funktionieren (P1, P2).

**Unabhängiger Test**: Kann getestet werden, indem im Stopp-Zustand der Gruppenwechsel-Taster gedrückt wird und überprüft wird, ob die Anzeige wechselt. Bei laufendem Timer sollte der Taster keine Wirkung haben. Liefert Wert durch Flexibilität bei unerwarteten Situationen.

**Akzeptanzszenarien**:

1. **Gegeben** Timer ist gestoppt und A/B-Feld leuchtet, **Wenn** die Standaufsicht den Gruppenwechsel-Taster drückt, **Dann** wechselt die Anzeige zu C/D
2. **Gegeben** Timer ist gestoppt und C/D-Feld leuchtet, **Wenn** die Standaufsicht den Gruppenwechsel-Taster drückt, **Dann** wechselt die Anzeige zu A/B
3. **Gegeben** Timer läuft (Countdown aktiv), **Wenn** die Standaufsicht den Gruppenwechsel-Taster drückt, **Dann** bleibt die aktuelle Gruppenanzeige unverändert (Taste wird ignoriert)

---

### Benutzer-Story 4 - Passen-Rhythmus nach Bogensport-Regeln (Priorität: P1)

Das System folgt automatisch dem Bogensport-Rhythmus: Ungerade Passen (1, 3, 5...) beginnen mit A/B, gerade Passen (2, 4, 6...) beginnen mit C/D. Innerhalb jeder Passe wechseln die Gruppen einmal (A/B→C/D oder C/D→A/B).

**Warum diese Priorität**: Regelkonformität ist kritisch für den Wettkampfbetrieb. Das System muss den offiziellen Ablauf korrekt implementieren.

**Unabhängiger Test**: Kann getestet werden, indem 6+ komplette Timer-Zyklen durchlaufen werden und der Rhythmus protokolliert wird. Muster muss sein: P1(AB,CD), P2(CD,AB), P3(AB,CD), P4(CD,AB), P5(AB,CD), P6(CD,AB). Liefert Wert durch Regelkonformität.

**Akzeptanzszenarien**:

1. **Gegeben** System startet neu (Passe 1), **Wenn** der erste Timer gestartet wird, **Dann** leuchtet A/B
2. **Gegeben** Passe 1 ist abgeschlossen (zwei Timer-Zyklen), **Wenn** der dritte Timer (Passe 2) gestartet wird, **Dann** leuchtet C/D
3. **Gegeben** Passe 2 ist abgeschlossen, **Wenn** der fünfte Timer (Passe 3) gestartet wird, **Dann** leuchtet A/B
4. **Gegeben** Passe 3 ist abgeschlossen, **Wenn** der siebte Timer (Passe 4) gestartet wird, **Dann** leuchtet C/D

---

### Grenzfälle

- Wie verhält sich das System beim Power-Cycle (Neustart) - startet es immer bei Passe 1 mit A/B?
- Wie wird der Passen-Zähler bei manuellen Eingriffen angepasst?

## Anforderungen *(verpflichtend)*

### Funktionale Anforderungen

- **FR-001**: Anzeigeeinheit MUSS drei visuelle Elemente haben: Timer-Anzeige (bestehend), Gruppenanzeige (A/B oder C/D), und Positions-Indikator (1 oder 2 zur Anzeige der Position innerhalb der Passe)
- **FR-002**: Genau ein LED-Feld MUSS zu jedem Zeitpunkt aktiv (leuchtend) sein, das andere muss aus sein
- **FR-003**: Sender MUSS einen zusätzlichen Taster für Gruppenwechsel haben (neben dem Start/Stop-Taster)
- **FR-004**: Gruppenwechsel-Taster MUSS nur im Stopp-Zustand (Timer läuft nicht) funktionieren
- **FR-005**: Gruppenwechsel-Taster MUSS durch einen 4-Zustands-Zyklus rotieren: (1) A/B+Position1 → nach Timer-Ablauf folgt C/D, dann normale Rhythmus-Fortsetzung; (2) A/B+Position2 → nach Timer-Ablauf beginnt neue Passe mit A/B; (3) C/D+Position1 → nach Timer-Ablauf folgt A/B; (4) C/D+Position2 → nach Timer-Ablauf folgt C/D erneut. Nach dem 4. Zustand springt System zurück zu Zustand 1.
- **FR-005a**: Wenn Gruppenwechsel-Taster innerhalb von 3 Sekunden nach letztem Drücken erneut gedrückt wird, MUSS das System zum nächsten Zustand im Zyklus wechseln. Nach 3 Sekunden ohne weitere Betätigung wird der aktuell angezeigte Zustand aktiv (fixiert).
- **FR-006**: System MUSS die Gruppenanzeige wechseln, wenn ein Timer endet - entweder durch natürlichen Ablauf (Countdown erreicht 0) ODER durch manuellen STOP-Befehl
- **FR-007**: System MUSS dem Bogensport-Rhythmus folgen:
  - Passe 1: A/B → C/D
  - Passe 2: C/D → A/B
  - Passe 3: A/B → C/D
  - Passe 4: C/D → A/B
  - (Muster wiederholt sich)
- **FR-008**: System MUSS einen internen Passen-Zähler führen, um den Rhythmus zu bestimmen
- **FR-009**: System MUSS beim ersten Start (Power-On) mit Passe 1, Gruppe A/B beginnen
- **FR-010**: Sender MUSS Gruppenwechsel-Befehle per Funk an Empfänger senden
- **FR-011**: Empfänger MUSS Gruppenwechsel-Befehle empfangen und die Anzeige entsprechend aktualisieren
- **FR-012**: LED-Felder MÜSSEN aus Schießentfernung (20+ Meter) klar erkennbar und unterscheidbar sein

### Schlüssel-Entitäten

- **Schützengruppe**: Repräsentiert die aktive Gruppe (A/B oder C/D) mit Zustand (aktiv/inaktiv)
- **Passen-Zustand**: Repräsentiert die aktuelle Passe (Nummer) und Position innerhalb der Passe (erster oder zweiter Timer)
- **Rhythmus-Regel**: Definiert die Reihenfolge der Gruppen basierend auf Passen-Nummer (ungerade: A/B→C/D, gerade: C/D→A/B)
- **Gruppenwechsel-Befehl**: Repräsentiert manuelle oder automatische Umschaltung der aktiven Gruppe

## Erfolgskriterien *(verpflichtend)*

### Messbare Ergebnisse

- **SC-001**: Schützen können aus 20+ Metern Entfernung klar erkennen, welche Gruppe (A/B oder C/D) gerade aktiv ist
- **SC-002**: System folgt dem Bogensport-Rhythmus korrekt über mindestens 10 komplette Passen ohne Abweichung
- **SC-003**: Automatischer Gruppenwechsel erfolgt innerhalb von 2 Sekunden nach Timer-Ablauf
- **SC-004**: Manuelle Gruppenwechsel (bei gestopptem Timer) werden innerhalb von 500ms auf der Anzeige sichtbar
- **SC-005**: Gruppenwechsel-Taster wird in 100% der Fälle ignoriert, wenn Timer läuft (keine unerwarteten Wechsel)
- **SC-006**: 95% der Standaufsichten können das Gruppenwechsel-System ohne Schulung korrekt bedienen nach initialer Demonstration
- **SC-007**: LED-Felder sind bei Tageslicht und Dämmerung aus 20+ Metern lesbar
- **SC-008**: System startet nach Power-Cycle zuverlässig bei Passe 1 mit Gruppe A/B in 100% der Fälle

## Annahmen

1. **LED-Feld-Größe**: Die A/B und C/D Felder nutzen ähnliche LED-Technologie wie die Timer-Anzeige (WS2812B), mit ausreichend LEDs für Lesbarkeit aus 20+ Metern
2. **Taster-Verfügbarkeit**: Am Sender ist ein zusätzlicher GPIO-Pin verfügbar für den Gruppenwechsel-Taster (siehe `/Schaltung/Schaltplan.pdf` für freie Pins)
3. **Funk-Kapazität**: Das bestehende Funkprotokoll (nRF24L01+) kann um einen TOGGLE_GROUP Befehl erweitert werden ohne Performance-Einbußen
4. **Persistenz**: Der Passen-Zähler wird nicht persistent gespeichert (EEPROM) - bei Neustart beginnt immer Passe 1
5. **Manuelle Korrektur**: Manueller Gruppenwechsel setzt den automatischen Rhythmus NICHT zurück - der Passen-Zähler läuft weiter
