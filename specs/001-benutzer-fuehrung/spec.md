# Feature Specification: Archery Tournament Control System - User Guidance

**Feature Branch**: `001-benutzer-fuehrung`
**Created**: 2025-12-13
**Status**: Draft
**Input**: User description: "Kommen wir nun zum allgemeinen Prozess der Benutzerführung. Dieser findet ausschließlich auf dem Sender statt. Der Empfänger führt immer nur exakt eine Aktion aus, wenn es ihm per Funk mitgeteilt wird. Der Empfänger kennt die Aktionen Stop, Start 240Sek, Start 120Sek, Init und Alarm. Diese werden ihm vom Sender übermittelt. Der Sender hingegen hat einen komplexeren Prozess: Erst zeigt er den Splash an. Danach geht er in eine Konfigurationsmenü. In diesem kann der Nutzer zunächst mit den \"Rechts\" und \"Links\" Tasten auf dem Monitor auswählen, ob 120 Sekunden oder 240 Sekunden geschossen werden. Wird die Eingabe mit der \"Okay\" Taste bestätigt, so kann er in der nächsten Zeile auswählen ob 1-2 oder 3-4 Schützen pro Scheibe schießen. Bestätiget er das abermals kann er in der nächsten Zeile mit der Rechts und Links Taste zwischen zwei Buttons auf dem Display wählen. Der (nicht per default ausgewählte Button \"Ändern\" springt wieder in die erste Zeile zur Auswahl. Der andere Knopf heißt \"Start\", worauf hin wir in den Tourniermodus wechseln. Hier gibt es zwei Zustände, die sich immer abwechseln: \"Pfeile holen\" und \"Schießbetrieb\". Das Display stellt den aktuellen Status oben dar. Nach der konfiguration sind wir erst mal im \"Pfeile holen\" Modus. Hier hat der Nutzer die Wahl die \"Nächste Passe\" zu starten. Dann geht es in den Modus \"Schießbetrieb\". Alternativ kann er auch die Reihenfolge der Schützen ändern. Diese Reihenfolge wird ebenfalls unten im Display angezeigt. Ein weitere Knopf \"Neustart\" lässt den Nutzer zurück in das Konfigurationsmenü kommen. Im Schießbetrieb gibt es nur einen einzigen Knopf: \"Passe beenden\". Mit diesem Knopf kommt man wieder in den Prozess \"Pfeile holen\". Drückt man irgendwann die OK Taste länger als 3 Sekunden, so sendet die Ampelsteuerung einen \"Alarm\" an den Empfänger."

## User Scenarios & Testing

### User Story 1 - Initial Tournament Setup (Priority: P1)

The tournament organizer needs to configure the basic tournament parameters before starting any shooting session. This is the first interaction users have with the system after powering on the sender device.

**Why this priority**: This is the absolute minimum required to make the system usable. Without configuration, no shooting session can begin. This represents the core value of the system - allowing organizers to set up tournaments with different time and shooter configurations.

**Independent Test**: Can be fully tested by powering on the sender, navigating through the configuration menu using the Left/Right/OK buttons, and verifying that all configuration options are selectable and that the "Start" button transitions to tournament mode. Delivers immediate value by allowing basic tournament setup.

**Acceptance Scenarios**:

1. **Given** the sender device is powered on and shows the splash screen, **When** the splash screen finishes displaying, **Then** the configuration menu is displayed showing the first configuration option (shooting time duration)
2. **Given** the configuration menu is displayed on the shooting time selection, **When** the user presses the "Right" button, **Then** the selection changes from 120 seconds to 240 seconds
3. **Given** the configuration menu is displayed on the shooting time selection, **When** the user presses the "Left" button, **Then** the selection changes from 240 seconds to 120 seconds
4. **Given** the shooting time is selected, **When** the user presses the "OK" button, **Then** the cursor moves to the next line allowing selection of number of shooters per target
5. **Given** the shooter count selection is active, **When** the user presses "Right" or "Left" buttons, **Then** the selection toggles between "1-2 shooters" and "3-4 shooters"
6. **Given** the shooter count is selected, **When** the user presses "OK" button, **Then** the cursor moves to the button selection line showing "Ändern" and "Start" buttons
7. **Given** the button selection line is active, **When** the user presses "Left" or "Right" buttons, **Then** the selection toggles between "Ändern" and "Start" buttons with "Start" as default
8. **Given** the "Ändern" button is selected, **When** the user presses "OK", **Then** the cursor returns to the first configuration line (shooting time selection)
9. **Given** the "Start" button is selected, **When** the user presses "OK", **Then** the system transitions to tournament mode in "Pfeile holen" (arrow retrieval) state

---

### User Story 2 - Managing Shooting Rounds (Priority: P2)

During a tournament, the organizer needs to control when shooting begins and when archers can retrieve their arrows. This alternating pattern between shooting and retrieval is the core operational flow of an archery tournament.

**Why this priority**: This is the primary operational function once configuration is complete. It enables the actual tournament to proceed with proper timing and safety controls. Without this, the system cannot manage the tournament flow.

**Independent Test**: Can be tested independently by starting from tournament mode (assuming P1 is implemented or manually setting initial state to "Pfeile holen"), pressing "Nächste Passe" to start shooting, and verifying the transition to "Schießbetrieb" mode, then pressing "Passe beenden" to return to "Pfeile holen" mode. Delivers the core tournament management functionality.

**Acceptance Scenarios**:

1. **Given** the system is in tournament mode in "Pfeile holen" state, **When** the system enters this state, **Then** the display shows "Pfeile holen" status at the top and shows the "Nächste Passe" button
2. **Given** the system is in "Pfeile holen" state, **When** the user presses the "Nächste Passe" button, **Then** the system transitions to "Schießbetrieb" state and sends the appropriate start command (Start 120Sek or Start 240Sek based on configuration) to the receiver via radio
3. **Given** the system is in "Schießbetrieb" state, **When** the system enters this state, **Then** the display shows "Schießbetrieb" status at the top and shows only the "Passe beenden" button
4. **Given** the system is in "Schießbetrieb" state, **When** the user presses the "Passe beenden" button, **Then** the system transitions back to "Pfeile holen" state and sends the Stop command to the receiver via radio
5. **Given** the system is in "Schießbetrieb" state with a 240-second timer configured, **When** the user presses "Nächste Passe", **Then** the receiver receives the "Start 240Sek" command
6. **Given** the system is in "Schießbetrieb" state with a 120-second timer configured, **When** the user presses "Nächste Passe", **Then** the receiver receives the "Start 120Sek" command

---

### User Story 3 - Managing Shooter Order (Priority: P3)

The organizer needs to adjust the shooting order during the tournament to accommodate different archer groups or lanes. This allows flexibility in managing which groups shoot on which targets.

**Why this priority**: This adds operational flexibility but is not required for basic tournament operation. The tournament can run without changing shooter order if the initial configuration is correct.

**Independent Test**: Can be tested by entering "Pfeile holen" mode, selecting the shooter order management option, verifying that the current order is displayed at the bottom of the screen, and confirming that changes to the order are reflected in the display. Delivers value by providing operational flexibility during tournaments.

**Acceptance Scenarios**:

1. **Given** the system is in "Pfeile holen" state, **When** the display is shown, **Then** the current shooter order is displayed at the bottom of the screen
2. **Given** the system is in "Pfeile holen" state, **When** the user selects the shooter order change option, **Then** the system allows modification of the shooting order
3. **Given** the shooting order has been modified, **When** the changes are confirmed, **Then** the updated order is displayed at the bottom of the screen and used for subsequent shooting rounds

---

### User Story 4 - Returning to Configuration (Priority: P4)

The organizer needs the ability to restart the tournament with different settings, such as changing from 120-second to 240-second rounds or adjusting the number of shooters per target.

**Why this priority**: This is a convenience feature that allows reconfiguration without restarting the device. Useful but not critical for initial tournament operation.

**Independent Test**: Can be tested by entering "Pfeile holen" mode, pressing the "Neustart" button, and verifying that the system returns to the configuration menu with previous settings cleared or retained. Delivers value by allowing tournament reconfiguration without device restart.

**Acceptance Scenarios**:

1. **Given** the system is in "Pfeile holen" state, **When** the display is shown, **Then** the "Neustart" button is visible and selectable
2. **Given** the system is in "Pfeile holen" state, **When** the user presses the "Neustart" button, **Then** the system returns to the configuration menu
3. **Given** the system has returned to the configuration menu from tournament mode, **When** the menu is displayed, **Then** the configuration options retain the previous tournament settings (shooting time and shooter count)

---

### User Story 5 - Emergency Alarm Function (Priority: P5)

In case of an emergency on the range, the organizer needs to immediately alert all participants by triggering an alarm that is broadcast to all receivers.

**Why this priority**: Safety feature that is critical in emergencies but not part of normal tournament operation. Should be available but is used infrequently.

**Independent Test**: Can be tested from any state by holding the OK button for longer than 3 seconds and verifying that an "Alarm" command is sent to the receiver. Delivers safety value independently of other features.

**Acceptance Scenarios**:

1. **Given** the system is in any state (configuration, Pfeile holen, or Schießbetrieb), **When** the user presses and holds the "OK" button for more than 3 seconds, **Then** the system sends an "Alarm" command to the receiver via radio
2. **Given** the OK button is held for less than 3 seconds, **When** the button is released, **Then** no alarm is triggered and normal button function is executed
3. **Given** an alarm has been triggered, **When** the alarm command is sent, **Then** the system immediately returns to the previous state without displaying a confirmation message

---

### Edge Cases

- What happens when the user presses multiple buttons simultaneously (e.g., Left + Right + OK at the same time)?
- What happens if radio communication with the receiver fails during command transmission (Stop, Start, Init, Alarm)?
- What happens if the user presses the OK button for exactly 3 seconds - is this considered a normal press or an alarm trigger?
- What happens when the system is powered on while in the middle of a configuration or tournament state? Should it resume or reset?
- What happens to the receiver if it receives conflicting commands in quick succession (e.g., Start 240Sek immediately followed by Stop)?
- What happens during the splash screen display if the user presses buttons - are they queued, ignored, or does the splash skip?
- What happens if the user navigates away from the configuration menu using "Ändern" multiple times - should there be a limit or timeout?
- What happens when transitioning from Schießbetrieb to Pfeile holen if the receiver hasn't completed the full timing cycle?

## Requirements

### Functional Requirements

#### Sender Device - Startup and Splash Screen

- **FR-001**: System MUST display a splash screen immediately upon power-on
- **FR-002**: System MUST automatically transition from splash screen to configuration menu after splash display completes
- **FR-003**: Splash screen duration MUST be 3 seconds

#### Sender Device - Configuration Menu

- **FR-004**: Configuration menu MUST provide three configuration lines: shooting time duration, number of shooters per target, and action buttons
- **FR-005**: System MUST allow selection between 120 seconds and 240 seconds shooting time using Left and Right buttons
- **FR-006**: System MUST allow selection between "1-2 shooters" and "3-4 shooters" per target using Left and Right buttons
- **FR-007**: System MUST require OK button press to confirm each configuration selection and move to the next line
- **FR-008**: System MUST provide two action buttons on the third line: "Ändern" (change) and "Start"
- **FR-009**: "Start" button MUST be selected by default when reaching the action button line
- **FR-010**: "Ändern" button MUST return the cursor to the first configuration line (shooting time) when selected
- **FR-011**: "Start" button MUST transition the system to tournament mode in "Pfeile holen" state when selected
- **FR-012**: System MUST send "Init" command to receiver when entering tournament mode

#### Sender Device - Tournament Mode States

- **FR-013**: Tournament mode MUST alternate between two states: "Pfeile holen" (arrow retrieval) and "Schießbetrieb" (shooting operation)
- **FR-014**: System MUST display the current state name at the top of the display
- **FR-015**: System MUST start in "Pfeile holen" state immediately after leaving configuration menu
- **FR-016**: System MUST display shooter order at the bottom of the display in both tournament states

#### Sender Device - Pfeile Holen (Arrow Retrieval) State

- **FR-017**: "Pfeile holen" state MUST display three available actions: "Nächste Passe" (next round), shooter order management, and "Neustart" (restart)
- **FR-018**: "Nächste Passe" button MUST transition the system to "Schießbetrieb" state when pressed
- **FR-019**: System MUST send "Start 240Sek" command to receiver when transitioning to Schießbetrieb if 240 seconds was configured
- **FR-020**: System MUST send "Start 120Sek" command to receiver when transitioning to Schießbetrieb if 120 seconds was configured
- **FR-021**: Shooter order management option MUST allow modification of the shooting order
- **FR-022**: Modified shooter order MUST be displayed at the bottom of the display
- **FR-023**: "Neustart" button MUST return the system to the configuration menu when pressed

#### Sender Device - Schießbetrieb (Shooting Operation) State

- **FR-024**: "Schießbetrieb" state MUST display only one action button: "Passe beenden" (end round)
- **FR-025**: "Passe beenden" button MUST transition the system back to "Pfeile holen" state when pressed
- **FR-026**: System MUST send "Stop" command to receiver when transitioning back to "Pfeile holen" state

#### Sender Device - Emergency Alarm Function

- **FR-027**: System MUST trigger emergency alarm when OK button is pressed and held for longer than 3 seconds
- **FR-028**: Alarm trigger MUST be available in all states (configuration menu, Pfeile holen, Schießbetrieb)
- **FR-029**: System MUST send "Alarm" command to receiver when alarm is triggered
- **FR-030**: System MUST distinguish between normal OK button presses (less than 3 seconds) and alarm trigger (more than 3 seconds)

#### Receiver Device - Command Reception and Execution

- **FR-031**: Receiver MUST support five distinct commands: Stop, Start 240Sek, Start 120Sek, Init, and Alarm
- **FR-032**: Receiver MUST execute only one action at a time based on the command received via radio
- **FR-033**: Receiver MUST execute the Stop command when received from sender
- **FR-034**: Receiver MUST execute the Start 240Sek command (240-second timer) when received from sender
- **FR-035**: Receiver MUST execute the Start 120Sek command (120-second timer) when received from sender
- **FR-036**: Receiver MUST execute the Init command when received from sender
- **FR-037**: Receiver MUST execute the Alarm command when received from sender

#### Radio Communication

- **FR-038**: Sender MUST transmit commands to receiver via radio link
- **FR-039**: All commands (Stop, Start 240Sek, Start 120Sek, Init, Alarm) MUST be transmitted reliably to the receiver
- **FR-040**: Radio communication MUST support sufficient range for typical archery range dimensions (minimum 50 meters suggested)

### Key Entities

- **Tournament Configuration**: Represents the settings for a tournament session including shooting time duration (120s or 240s) and number of shooters per target (1-2 or 3-4)
- **Tournament State**: Represents the current operational mode with two primary values: "Pfeile holen" (arrow retrieval mode) and "Schießbetrieb" (active shooting mode)
- **Shooter Order**: Represents the sequence in which shooting groups or lanes operate, displayed persistently during tournament mode
- **Radio Command**: Represents a control message sent from sender to receiver, with five command types: Stop, Start 240Sek, Start 120Sek, Init, and Alarm
- **User Interface State**: Represents the current screen and navigation context on the sender display, including configuration menu, tournament mode screens, and button selections

## Success Criteria

### Measurable Outcomes

- **SC-001**: Tournament organizers can complete the full configuration process (time selection, shooter count, and start confirmation) in under 30 seconds
- **SC-002**: System transitions between Pfeile holen and Schießbetrieb states occur within 1 second of button press
- **SC-003**: Radio commands (Stop, Start 240Sek, Start 120Sek, Init, Alarm) are received by the receiver within 500 milliseconds of transmission from sender
- **SC-004**: 95% of users can navigate the configuration menu successfully on their first attempt without requiring external assistance
- **SC-005**: Emergency alarm activation (3-second OK button hold) is recognizable and triggerable within 4 seconds in any system state
- **SC-006**: The system successfully completes 100 consecutive state transitions (configuration to tournament, Pfeile holen to Schießbetrieb and back) without errors or freezing
- **SC-007**: Display updates reflect the current system state (configuration selections, tournament mode, shooter order) within 200 milliseconds of state changes
- **SC-008**: Radio communication operates reliably at distances up to 50 meters without command loss or corruption
