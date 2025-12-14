# Validierungs-Bericht: Schützengruppen-Anzeige (Feature 002)

**Datum**: 2025-12-04
**Status**: ⚠️ **BEDINGT BESTANDEN**
**Reviewer**: Automatische Spezifikations-Validierung

## Übersicht

Die Spezifikation für Feature 002 (Schützengruppen-Anzeige) ist **gut strukturiert und größtenteils vollständig**, hat aber **4 kritische Grenzfälle**, die als funktionale Anforderungen formalisiert werden müssen, bevor die Implementation beginnen kann.

## Validierungs-Ergebnisse

### ✅ Bestanden (9/12 Kategorien)

1. **Benutzer-fokussiert**: Alle User Stories beschreiben WAS, nicht WIE
2. **Testbare Akzeptanzkriterien**: 15 Szenarien im Gegeben-Wenn-Dann Format
3. **Messbare Erfolgskriterien**: 8 quantifizierbare Metriken (SC-001 bis SC-008)
4. **Prioritäten**: Logische P1/P2/P3 Zuweisung mit Begründungen
5. **Unabhängige Testbarkeit**: Alle 4 User Stories isoliert testbar
6. **Inkrementeller Wert**: Jede Story liefert sofort Mehrwert
7. **Schlüssel-Entitäten**: 4 Entitäten klar definiert
8. **Technische Machbarkeit**: 5 Annahmen validiert gegen Feature 001
9. **Regelkonformität**: Bogensport-Rhythmus korrekt dokumentiert

### ⚠️ Verbesserungsbedarf (3/12 Kategorien)

#### 1. Funktionale Anforderungen (Blocker)

**Problem**: Die Spezifikation hat 4 Grenzfall-Fragen ohne klare Antworten:

| Grenzfall | Aktuelle Situation | Fehlende FR | Impact |
|-----------|-------------------|-------------|--------|
| **GF-1**: Manueller Wechsel + Timer-Start | Annahme #5: "setzt NICHT zurück" | FR-013 fehlt | **HOCH** - Betrifft Rhythmus-Logik |
| **GF-2**: Power-Cycle mitten in Passe 3 | FR-009 sagt "Passe 1, A/B" | FR-009 unklar formuliert | **MITTEL** - Betrifft Neustart-Verhalten |
| **GF-3**: Manueller STOP → Gruppe wechselt? | FR-006 sagt "bei Ablauf" | FR-006 nicht explizit | **HOCH** - Betrifft State-Transitions |
| **GF-4**: Passen-Zähler bei manuellem Stopp | Nicht spezifiziert | FR-008 fehlt Details | **HOCH** - Betrifft Rhythmus-Synchronisation |

**Empfohlene Lösung**:

```markdown
### Neue/Erweiterte Anforderungen

- **FR-006** (erweitert): System MUSS automatisch die Gruppenanzeige wechseln, wenn ein Timer **durch Ablauf** (Countdown = 0) endet. Bei manuellem STOP-Befehl DARF die Gruppe NICHT wechseln.

- **FR-008** (erweitert): System MUSS einen internen Passen-Zähler führen, um den Rhythmus zu bestimmen. Der Zähler inkrementiert NUR wenn ein Timer vollständig abläuft (nicht bei manuellem STOP).

- **FR-009** (erweitert): System MUSS bei JEDEM Power-On (unabhängig von vorherigem Zustand) mit Passe 1, Gruppe A/B beginnen. Kein persistenter Zustand wird gespeichert.

- **FR-013** (NEU): Manueller Gruppenwechsel (Taster-Betätigung) DARF den automatischen Passen-Zähler NICHT zurücksetzen. Der Rhythmus läuft intern weiter, auch wenn Anzeige manuell geändert wurde.
```

#### 2. LED-Spezifikation (Nice-to-have)

**Problem**: Keine konkrete Angabe über LED-Anzahl pro Feld (A/B, C/D).

**Empfohlene Lösung**:

```markdown
### Annahme 6 (NEU): LED-Feld-Größe

- Jedes Feld (A/B und C/D) besteht aus mindestens 20 WS2812B LEDs, angeordnet als Buchstaben
- Gesamtzusatz: ~40 LEDs (20 pro Feld)
- Rationale: 20 LEDs pro Feld ermöglichen 20+ Meter Lesbarkeit bei Tageslicht
```

#### 3. Strombudget (Nice-to-have)

**Problem**: Zusätzliche LEDs erhöhen Stromverbrauch, keine Validierung gegen Powerbank.

**Empfohlene Lösung**:

```markdown
### Annahme 7 (NEU): Strombudget

- Timer-Anzeige: ~155 LEDs (bestehend)
- Gruppenfelder: ~40 LEDs (neu)
- **Gesamt: ~195 LEDs**
- Worst-Case: 195 × 60mA = **11.7A** (alle LEDs weiß, 100% Helligkeit)
- Powerbank muss mindestens 12A liefern können (siehe Feature 001, bereits spezifiziert)
- Development-Mode (3% Helligkeit) reduziert auf ~350mA (USB-sicher)
```

## Risiken & Blocker

### 🔴 Kritisch (MUSS vor Implementation gelöst werden)

1. **Grenzfall-Verhalten nicht spezifiziert**: Die 4 Grenzfälle (GF-1 bis GF-4) können zu **widersprüchlichem Verhalten** führen, wenn nicht geklärt:
   - Entwickler könnte annehmen, dass manueller STOP die Gruppe wechselt (wie automatischer Ablauf)
   - Passen-Zähler könnte falsch inkrementieren bei manuellen Eingriffen
   - Rhythmus könnte desynchronisiert werden durch manuelle Wechsel

   **Lösungsweg**: Entweder `/speckit.clarify` ausführen ODER die 4 empfohlenen FRs manuell hinzufügen.

### 🟡 Moderat (Sollte vor Implementation geklärt werden)

2. **LED-Count unbekannt**: Ohne konkrete LED-Anzahl kann Hardware-Design nicht finalisiert werden.
   - **Impact**: Bestückungsliste für PCB kann nicht erstellt werden
   - **Lösungsweg**: Annahme 6 hinzufügen (20 LEDs pro Feld)

3. **Keine Integration-Diagramme**: Unklar, wie Gruppenwechsel in bestehende State-Machine integriert wird.
   - **Impact**: Architektur-Entscheidungen verzögert
   - **Lösungsweg**: In Plan-Phase (data-model.md) klären

## Empfohlene Nächste Schritte

### Option 1: Automatische Klärung (Empfohlen)

```bash
/speckit.clarify
```

**Vorteil**: Interaktiver Prozess stellt bis zu 5 gezielte Fragen zu den Grenzfällen.

**Ergebnis**: Aktualisierte spec.md mit FRs für alle 4 Grenzfälle.

### Option 2: Manuelle Korrektur

1. Öffne `/mnt/d/git/Bogenampel/specs/002-shooter-groups/spec.md`
2. Füge FR-013 hinzu
3. Erweitere FR-006, FR-008, FR-009 wie oben beschrieben
4. Füge Annahmen 6 und 7 hinzu
5. Entferne die 4 Grenzfall-Fragen (jetzt als FRs gelöst)

**Vorteil**: Volle Kontrolle über Formulierungen.

**Nachteil**: Manuelle Arbeit, keine Validierung.

## Qualitäts-Score

| Kategorie | Score | Notizen |
|-----------|-------|---------|
| Inhaltsqualität | ✅ 100% | Benutzer-fokussiert, testbar, messbar |
| Anforderungsvollständigkeit | ⚠️ 67% | 4 Grenzfälle fehlen als FRs |
| Feature-Bereitschaft | ✅ 100% | Prioritäten, Testbarkeit, Wert klar |
| Annahmen-Validierung | ⚠️ 71% | 5/7 Annahmen vorhanden |
| **GESAMT** | ⚠️ **84%** | **GUT, aber 4 Blocker** |

## Fazit

Die Spezifikation ist **fast bereit für Implementation**. Die Struktur, Prioritäten und Akzeptanzkriterien sind **exzellent**. Die 4 fehlenden funktionalen Anforderungen (FR-006/FR-008/FR-009 erweitern, FR-013 hinzufügen) sind der **einzige Blocker**.

**Empfehlung**: Führe `/speckit.clarify` aus, um die Grenzfälle interaktiv zu klären. Dies sollte **<10 Minuten** dauern und die Spezifikation auf **100% Bereitschaft** bringen.

---

**Erstellt**: 2025-12-04 | **Tool**: Automatic Spec Validation | **Version**: 1.0
