# Feature Specification: Startup-Logo und Splash Screen

**Feature Branch**: `003-startup-logo-splash`
**Created**: 2025-12-13
**Status**: Draft
**Input**: User description: "Beim starten soll das Display ein Logo anzeigen. Dies soll 3 Sekunden lang eingeblendet bleiben. Ein zusätzlicher Text "Bogenampeln V1.0" soll eingeblendet werden. Danach soll es in den nächsten Modus gehen."

## Clarifications

### Session 2025-12-13

- Q: Soll der Splash Screen übersprungen werden können oder ignoriert das System Eingaben während des Splash Screens? → A: Beliebige Taste überspringt zum Hauptmodus

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Startup-Logo beim Einschalten anzeigen (Priority: P1)

Als Benutzer möchte ich beim Einschalten des Geräts ein Logo und die Versionsnummer sehen, damit ich weiß, dass das Gerät korrekt startet und welche Software-Version läuft.

**Why this priority**: Dies ist die Kernfunktionalität des Features. Der Splash Screen dient als visuelles Feedback beim Systemstart und vermittelt Professionalität. Die Versionsanzeige ist wichtig für Support und Fehlerdiagnose.

**Independent Test**: Kann vollständig getestet werden, indem das Gerät eingeschaltet wird und beobachtet wird, ob Logo und Versionsnummer für 3 Sekunden angezeigt werden. Liefert eigenständigen Wert durch Bestätigung des erfolgreichen Starts.

**Acceptance Scenarios**:

1. **Given** das Gerät ist ausgeschaltet, **When** der Benutzer das Gerät einschaltet, **Then** erscheint sofort das Logo auf dem Display
2. **Given** das Gerät zeigt den Splash Screen, **When** der Benutzer wartet, **Then** wird zusätzlich der Text "Bogenampeln V1.0" angezeigt
3. **Given** das Gerät zeigt Logo und Versionstext, **When** 3 Sekunden vergangen sind, **Then** wechselt das Display automatisch zum nächsten Modus (Hauptbildschirm)

---

### User Story 2 - Nahtloser Übergang zum Hauptmodus (Priority: P2)

Als Benutzer möchte ich nach dem Splash Screen automatisch zum Hauptbildschirm weitergeleitet werden, damit ich das Gerät ohne weitere Interaktion nutzen kann.

**Why this priority**: Diese Funktion sorgt für einen reibungslosen Ablauf beim Systemstart, ist aber nachrangig zur eigentlichen Anzeige des Splash Screens. Der Übergang verbessert die Benutzererfahrung.

**Independent Test**: Kann getestet werden, indem das Gerät gestartet wird und überprüft wird, ob nach genau 3 Sekunden automatisch der Hauptmodus erscheint. Liefert eigenständigen Wert durch vollautomatischen Startablauf.

**Acceptance Scenarios**:

1. **Given** der Splash Screen wird seit 3 Sekunden angezeigt, **When** die 3-Sekunden-Frist abläuft, **Then** wechselt das Display automatisch zum nächsten Modus
2. **Given** das Gerät wechselt vom Splash Screen zum Hauptmodus, **When** der Übergang erfolgt, **Then** ist der Wechsel ohne Flackern oder schwarzen Bildschirm sichtbar
3. **Given** der Splash Screen wird angezeigt, **When** der Benutzer eine beliebige Taste drückt, **Then** wird der Splash Screen sofort beendet und das System wechselt zum Hauptmodus

---

### Edge Cases

- Was passiert, wenn das Gerät während der Anzeige des Splash Screens ausgeschaltet wird? (Der Splash Screen wird unterbrochen, beim nächsten Start beginnt er von vorne)
- Was passiert, wenn der Benutzer während des Splash Screens eine Taste drückt? (Der Splash Screen wird sofort beendet und das System wechselt zum Hauptmodus)
- Was passiert, wenn das Logo nicht geladen werden kann? (Es wird nur der Text "Bogenampeln V1.0" angezeigt)
- Was passiert bei einem sehr schnellen Power-Cycle (Ein/Aus/Ein)? (Jeder Startvorgang zeigt den vollständigen 3-Sekunden Splash Screen)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST beim Einschalten sofort ein Logo auf dem Display anzeigen
- **FR-002**: System MUST den Text "Bogenampeln V1.0" zusammen mit dem Logo anzeigen
- **FR-003**: System MUST den Splash Screen (Logo und Text) für genau 3 Sekunden anzeigen
- **FR-004**: System MUST nach Ablauf der 3 Sekunden automatisch zum Hauptmodus (nächster Modus) wechseln
- **FR-005**: System MUST den Übergang vom Splash Screen zum Hauptmodus ohne Verzögerung oder Flackern durchführen
- **FR-006**: System MUST bei jedem Neustart den Splash Screen erneut anzeigen
- **FR-007**: System MUST den Splash Screen sofort beenden und zum Hauptmodus wechseln, wenn während der Anzeige eine beliebige Taste gedrückt wird

### Key Entities

- **Splash Screen**: Der Startbildschirm bestehend aus Logo und Versionstext, der beim Systemstart für 3 Sekunden angezeigt wird
- **Logo**: Grafisches Element, das auf dem Splash Screen dargestellt wird
- **Versionstext**: Textanzeige "Bogenampeln V1.0" die die aktuelle Software-Version identifiziert
- **Hauptmodus**: Der Betriebsmodus, zu dem nach dem Splash Screen gewechselt wird

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Der Splash Screen erscheint innerhalb von 0,5 Sekunden nach dem Einschalten des Geräts
- **SC-002**: Logo und Text "Bogenampeln V1.0" sind gleichzeitig und vollständig lesbar auf dem Display sichtbar
- **SC-003**: Der Splash Screen wird für exakt 3 Sekunden (±0,2 Sekunden) angezeigt
- **SC-004**: Der Übergang vom Splash Screen zum Hauptmodus erfolgt innerhalb von 0,5 Sekunden nach Ablauf der 3-Sekunden-Frist
- **SC-005**: Bei 100% der Neustarts wird der Splash Screen korrekt angezeigt
- **SC-006**: Ein Tastendruck während des Splash Screens führt innerhalb von 0,2 Sekunden zum Hauptmodus

### Assumptions

- **A-001**: Das Display ist beim Einschalten betriebsbereit und kann sofort Inhalte anzeigen
- **A-002**: Das Logo ist im Gerät gespeichert und kann ohne externe Ressourcen geladen werden
- **A-003**: Der "nächste Modus" bzw. Hauptmodus ist nach dem Systemstart initialisiert und bereit zur Anzeige
- **A-004**: Die Zeitsteuerung (3 Sekunden) erfolgt präzise genug für eine gute Benutzererfahrung
- **A-005**: Das Display hat ausreichend Auflösung und Platz für Logo und Versionstext
- **A-006**: Der Versionstext "Bogenampeln V1.0" ist fest kodiert (keine dynamische Versionserkennung erforderlich)
