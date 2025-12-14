# Tasks: Archery Tournament Control System - User Guidance

**Input**: Design documents from `/specs/001-benutzer-fuehrung/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Hardware-in-the-loop tests included for embedded system validation

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Path Conventions

- **Sender firmware**: `Sender/` directory at repository root
- **Receiver firmware**: `Empfaenger/` directory at repository root
- **Hardware documentation**: `Schaltung/` directory (KiCad project)
- **Tests**: `tests/` directory for hardware-in-the-loop tests

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Hardware setup and shared code infrastructure

- [ ] T001 Update KiCad schematic to add LCD display connector (4 pins: VCC, GND, SDA=A4, SCL=A5) in Schaltung/Bogenampel.kicad_sch
- [ ] T002 Update KiCad schematic to add button connectors (J_LEFT=D2, J_RIGHT=D3, J_OK=D4 with pull-ups) in Schaltung/Bogenampel.kicad_sch
- [ ] T003 Export updated schematic to PDF in Schaltung/Schaltplan.pdf
- [ ] T004 [P] Update Sender/Config.h with LCD I2C address (0x27 or 0x3F), pins (A4/A5), and constants (SPLASH_DURATION_MS=3000, ALARM_THRESHOLD_MS=3000)
- [ ] T005 [P] Update Sender/Config.h with button pin definitions (PIN_BTN_LEFT=2, PIN_BTN_RIGHT=3, PIN_BTN_OK=4, DEBOUNCE_DELAY_MS=50)
- [ ] T006 [P] Update Sender/Config.h with EEPROM address definition (EEPROM_CONFIG_ADDR=0) and TournamentConfig struct
- [ ] T007 [P] Install LiquidCrystal_I2C library (version 1.1.4+ by Frank de Brabander) in Arduino IDE or add to platformio.ini
- [ ] T008 Create or update Common/Commands.h with RadioCommand enum (CMD_STOP=0x01, CMD_START_120=0x02, CMD_START_240=0x03, CMD_INIT=0x04, CMD_ALARM=0x05) and RadioPacket struct
- [ ] T009 Add checksum functions (calculateChecksum, validateChecksum) and static_assert for packet size to Common/Commands.h
- [ ] T010 [P] Ensure Empfaenger/Commands.h is identical to Common/Commands.h or create symlink

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T011 Extend Sender/ButtonManager.h to add alarm detection variables (okPressStartTime, okPressActive, alarmTriggered) and method declarations (isAlarmTriggered, update)
- [ ] T012 Implement button debouncing (50ms delay) in Sender/ButtonManager.cpp for Left/Right/OK buttons using lastDebounceTime array
- [ ] T013 Implement 3-second alarm detection logic in Sender/ButtonManager.cpp update() method (millis()-based timing)
- [ ] T014 Extend Sender/StateMachine.h to add new states (STATE_SPLASH, STATE_CONFIG_MENU, STATE_PFEILE_HOLEN, STATE_SCHIESS_BETRIEB) and previousState tracking
- [ ] T015 Implement state transition logic in Sender/StateMachine.cpp with state entry/exit handlers and splash screen auto-transition (3s timeout)
- [ ] T016 [P] Implement EEPROM config load function in Sender/Sender.ino (loadConfigFromEEPROM with checksum validation, defaults to 120s/1-2 shooters)
- [ ] T017 [P] Implement EEPROM config save function in Sender/Sender.ino (saveConfigToEEPROM with checksum calculation)
- [ ] T018 Implement radio setup and configuration in Sender/Sender.ino setup() (channel 76, PA_HIGH, 250KBPS, retries 5/15, pipe address)
- [ ] T019 Implement sendCommand() function in Sender/Sender.ino with RadioPacket construction, checksum, and transmission result enum (TX_SUCCESS, TX_TIMEOUT, TX_ERROR)
- [ ] T020 [P] Implement sendAlarmWithRetry() function in Sender/Sender.ino (3 attempts with 200ms delay between retries)
- [ ] T021 Update Empfaenger/Empfaenger.ino to implement receiveCommand() with checksum validation and command code range checking
- [ ] T022 Update Empfaenger/Empfaenger.ino to implement handleCommand() dispatcher for all 5 radio commands with receiver actions

**Checkpoint**: Foundation ready - user story implementation can now begin in parallel

---

## Phase 3: User Story 1 - Initial Tournament Setup (Priority: P1) 🎯 MVP

**Goal**: Enable tournament organizer to configure shooting time (120s/240s) and shooter count (1-2/3-4) via 3-button LCD menu, with splash screen on startup and transition to tournament mode

**Independent Test**: Power on sender, verify 3-second splash screen displays, navigate config menu with Left/Right/OK buttons through all 3 lines (time, shooters, action buttons), select different values, press "Ändern" to loop back, press "Start" to enter tournament mode (verify CMD_INIT sent and "Pfeile holen" state entered)

### Implementation for User Story 1

- [ ] T023 [P] [US1] Update Sender/SplashScreen.cpp to render 3-line LCD splash (Line 0: "Bogenampel", Line 1: "Steuerung", Line 2: "Version 1.0") for 3 seconds
- [ ] T024 [US1] Create Sender/ConfigMenu.h with ConfigMenu class (cursorLine, selectedTime, selectedCount, selectedButton variables and handleLeftButton, handleRightButton, handleOKButton, render methods)
- [ ] T025 [US1] Implement ConfigMenu navigation logic in Sender/ConfigMenu.cpp for line 0 (time: 120s ↔ 240s toggle with Left/Right, OK moves to line 1)
- [ ] T026 [US1] Implement ConfigMenu navigation logic in Sender/ConfigMenu.cpp for line 1 (shooters: 1-2 ↔ 3-4 toggle with Left/Right, OK moves to line 2)
- [ ] T027 [US1] Implement ConfigMenu navigation logic in Sender/ConfigMenu.cpp for line 2 (buttons: Ändern ↔ Start toggle with Left/Right, OK on "Ändern" goes to line 0, OK on "Start" triggers state transition)
- [ ] T028 [US1] Implement ConfigMenu LCD rendering in Sender/ConfigMenu.cpp (3-line display with cursor indicator, selected values highlighted)
- [ ] T029 [US1] Integrate ConfigMenu into Sender/Sender.ino main loop for STATE_CONFIG_MENU handling (button presses → ConfigMenu methods → LCD render)
- [ ] T030 [US1] Implement "Start" button action in ConfigMenu to save config to EEPROM, send CMD_INIT via radio, and transition to STATE_PFEILE_HOLEN
- [ ] T031 [US1] Initialize LCD (lcd.init, lcd.backlight) and load saved config from EEPROM in Sender/Sender.ino setup() to restore previous settings on startup

### Hardware-in-the-Loop Tests for User Story 1

- [ ] T032 [P] [US1] Test splash screen displays for exactly 3 seconds and auto-transitions to config menu
- [ ] T033 [P] [US1] Test Left/Right buttons toggle time selection (120s ↔ 240s) on config menu line 0
- [ ] T034 [P] [US1] Test Left/Right buttons toggle shooter count (1-2 ↔ 3-4) on config menu line 1
- [ ] T035 [P] [US1] Test OK button moves cursor from line 0 → line 1 → line 2 in config menu
- [ ] T036 [P] [US1] Test "Ändern" button returns cursor to line 0 when selected and OK pressed
- [ ] T037 [P] [US1] Test "Start" button sends CMD_INIT to receiver (verify with radio sniffer or receiver Serial output)
- [ ] T038 [P] [US1] Test config values saved to EEPROM on "Start" press (power cycle, verify values restored)
- [ ] T039 [P] [US1] Test EEPROM checksum validation (corrupt EEPROM → defaults to 120s/1-2 shooters)
- [ ] T040 [P] [US1] Test LCD displays all 3 config menu lines with correct formatting and cursor indication

**Checkpoint**: At this point, User Story 1 should be fully functional and testable independently

---

## Phase 4: User Story 2 - Managing Shooting Rounds (Priority: P2)

**Goal**: Enable organizer to alternate between "Pfeile holen" (arrow retrieval) and "Schießbetrieb" (active shooting) states by pressing "Nächste Passe" and "Passe beenden" buttons, with radio commands sent to receiver

**Independent Test**: Manually enter STATE_PFEILE_HOLEN (or complete US1 setup), press "Nächste Passe" button, verify CMD_START_120 or CMD_START_240 sent (based on config), verify transition to STATE_SCHIESS_BETRIEB, press "Passe beenden", verify CMD_STOP sent and return to STATE_PFEILE_HOLEN

### Implementation for User Story 2

- [ ] T041 [US2] Create Sender/TournamentMode.h with TournamentMode class (selectedAction, shooterOrder variables and handleButtons, render methods)
- [ ] T042 [P] [US2] Implement STATE_PFEILE_HOLEN button navigation in Sender/TournamentMode.cpp (Left/Right cycles through "Nächste Passe", "Reihenfolge", "Neustart" buttons)
- [ ] T043 [US2] Implement "Nächste Passe" action in Sender/TournamentMode.cpp (send CMD_START_120 or CMD_START_240 based on config.shootingTime, transition to STATE_SCHIESS_BETRIEB on success)
- [ ] T044 [P] [US2] Implement STATE_SCHIESS_BETRIEB button handling in Sender/TournamentMode.cpp ("Passe beenden" button sends CMD_STOP and transitions to STATE_PFEILE_HOLEN)
- [ ] T045 [P] [US2] Implement STATE_PFEILE_HOLEN LCD rendering in Sender/TournamentMode.cpp (Line 0: "Pfeile holen", Line 1: button selections with highlight, Line 2/3: shooter order)
- [ ] T046 [P] [US2] Implement STATE_SCHIESS_BETRIEB LCD rendering in Sender/TournamentMode.cpp (Line 0: "Schießbetrieb", Line 1: "Passe beenden" button, Line 2/3: shooter order)
- [ ] T047 [US2] Integrate TournamentMode into Sender/Sender.ino main loop for STATE_PFEILE_HOLEN and STATE_SCHIESS_BETRIEB handling
- [ ] T048 [US2] Implement radio error handling for transmission failures (display "Fehler: Empfänger?" or "Funk-Fehler!" on LCD, stay in current state, allow retry)
- [ ] T049 [US2] Update Empfaenger/Empfaenger.ino handleCommand() to implement CMD_START_120 action (start 10s red prep, then 120s green countdown with buzzer tones and LED color transitions)
- [ ] T050 [US2] Update Empfaenger/Empfaenger.ino handleCommand() to implement CMD_START_240 action (same as CMD_START_120 but 240s countdown)
- [ ] T051 [US2] Update Empfaenger/Empfaenger.ino handleCommand() to implement CMD_STOP action (stop timer, set LEDs red, display "000", play stop tone)
- [ ] T052 [US2] Update Empfaenger/Empfaenger.ino handleCommand() to implement CMD_INIT action (reset state, clear display, flash status LED 3 times)

### Hardware-in-the-Loop Tests for User Story 2

- [ ] T053 [P] [US2] Test "Nächste Passe" button sends CMD_START_120 when config is 120s (verify receiver starts 120s countdown)
- [ ] T054 [P] [US2] Test "Nächste Passe" button sends CMD_START_240 when config is 240s (verify receiver starts 240s countdown)
- [ ] T055 [P] [US2] Test "Passe beenden" button sends CMD_STOP (verify receiver stops timer and shows red LEDs)
- [ ] T056 [P] [US2] Test state transition from PFEILE_HOLEN to SCHIESS_BETRIEB occurs within 1 second of button press
- [ ] T057 [P] [US2] Test state transition from SCHIESS_BETRIEB to PFEILE_HOLEN occurs within 1 second of button press
- [ ] T058 [P] [US2] Test LCD displays correct state name at top ("Pfeile holen" or "Schießbetrieb")
- [ ] T059 [P] [US2] Test radio transmission error handling (power off receiver, press button, verify error message on sender LCD)
- [ ] T060 [P] [US2] Test radio command latency is < 500ms (button press to receiver acknowledgment)
- [ ] T061 [P] [US2] Test receiver executes CMD_INIT correctly (status LED flashes 3 times, state reset)

**Checkpoint**: At this point, User Stories 1 AND 2 should both work independently

---

## Phase 5: User Story 3 - Managing Shooter Order (Priority: P3)

**Goal**: Enable organizer to view and modify shooting order during "Pfeile holen" state, with order displayed at bottom of LCD and changes persisted within tournament session

**Independent Test**: Enter STATE_PFEILE_HOLEN, verify default shooter order (1-2-3-...) displayed at bottom of LCD, select "Reihenfolge" button, modify order, verify updated order displayed and used for subsequent rounds

### Implementation for User Story 3

- [ ] T062 [P] [US3] Create Sender/ShooterOrder.h with ShooterOrder class (order[] array, count variable, setOrder, getOrder, formatForDisplay methods)
- [ ] T063 [US3] Implement ShooterOrder default initialization in Sender/ShooterOrder.cpp (sequential order 1, 2, 3, ..., N)
- [ ] T064 [US3] Implement ShooterOrder formatForDisplay() in Sender/ShooterOrder.cpp (returns string "Order: 1-2-3-4..." for LCD bottom line)
- [ ] T065 [US3] Integrate ShooterOrder display into TournamentMode rendering (both PFEILE_HOLEN and SCHIESS_BETRIEB states show order at bottom)
- [ ] T066 [US3] Implement "Reihenfolge" button action in Sender/TournamentMode.cpp (enter shooter order edit sub-state)
- [ ] T067 [US3] Implement shooter order edit UI in Sender/TournamentMode.cpp (select position with Left/Right, increment/decrement shooter ID, OK confirms and moves to next position)
- [ ] T068 [US3] Implement order validation in Sender/ShooterOrder.cpp (ensure unique shooter IDs, count ≤ MAX_SHOOTERS)
- [ ] T069 [US3] Store modified order in RAM (persists within tournament session, resets to default on "Neustart" or power cycle)

### Hardware-in-the-Loop Tests for User Story 3

- [ ] T070 [P] [US3] Test default shooter order displays at bottom of LCD in PFEILE_HOLEN state (e.g., "Order: 1-2-3-4-5-6")
- [ ] T071 [P] [US3] Test default shooter order displays at bottom of LCD in SCHIESS_BETRIEB state (same as PFEILE_HOLEN)
- [ ] T072 [P] [US3] Test "Reihenfolge" button enters order edit mode (LCD shows position editing UI)
- [ ] T073 [P] [US3] Test Left/Right buttons modify shooter ID at selected position in edit mode
- [ ] T074 [P] [US3] Test OK button moves to next position in order edit mode
- [ ] T075 [P] [US3] Test modified order displays correctly after exiting edit mode
- [ ] T076 [P] [US3] Test order persists between PFEILE_HOLEN ↔ SCHIESS_BETRIEB transitions
- [ ] T077 [P] [US3] Test order resets to default on "Neustart" button press

**Checkpoint**: At this point, User Stories 1, 2, AND 3 should all work independently

---

## Phase 6: User Story 4 - Returning to Configuration (Priority: P4)

**Goal**: Enable organizer to return to configuration menu from tournament mode via "Neustart" button, with previous config settings retained (not reset to defaults)

**Independent Test**: Enter tournament mode, change state to PFEILE_HOLEN, press "Neustart" button, verify system returns to CONFIG_MENU state, verify previous time and shooter count selections retained (not defaults)

### Implementation for User Story 4

- [ ] T078 [US4] Implement "Neustart" button action in Sender/TournamentMode.cpp (transition from STATE_PFEILE_HOLEN to STATE_CONFIG_MENU)
- [ ] T079 [US4] Ensure ConfigMenu retains config values on re-entry from "Neustart" (load from current variables, not EEPROM or defaults)
- [ ] T080 [US4] Implement ConfigMenu initialization logic to handle both fresh startup (load EEPROM) and "Neustart" return (keep current values)

### Hardware-in-the-Loop Tests for User Story 4

- [ ] T081 [P] [US4] Test "Neustart" button visible and selectable in PFEILE_HOLEN state
- [ ] T082 [P] [US4] Test "Neustart" button transitions from PFEILE_HOLEN to CONFIG_MENU state
- [ ] T083 [P] [US4] Test config menu displays previous settings after "Neustart" (e.g., if 240s/3-4 was selected before tournament start, it's still 240s/3-4 after "Neustart")
- [ ] T084 [P] [US4] Test config values can be changed after "Neustart" and saved again on "Start" press

**Checkpoint**: At this point, User Stories 1-4 should all work independently

---

## Phase 7: User Story 5 - Emergency Alarm Function (Priority: P5)

**Goal**: Enable organizer to trigger emergency alarm by holding OK button for > 3 seconds in any state, sending CMD_ALARM to receiver, and immediately returning to previous state without confirmation

**Independent Test**: From any state (CONFIG_MENU, PFEILE_HOLEN, or SCHIESS_BETRIEB), hold OK button for 3.1 seconds, verify CMD_ALARM sent to receiver (with 3 retry attempts if needed), verify sender returns to previous state immediately, verify receiver triggers alarm (flashing red LEDs, alarm buzzer)

### Implementation for User Story 5

- [ ] T085 [P] [US5] Integrate alarm detection into Sender/Sender.ino main loop (check alarmDetector.isAlarmTriggered() before state-specific button handling)
- [ ] T086 [US5] Implement handleAlarm() function in Sender/Sender.ino (call sendAlarmWithRetry(), display brief "Alarm gesendet" or error message, return to previous state)
- [ ] T087 [US5] Ensure alarm detection works in all states (SPLASH excluded for safety, CONFIG_MENU, PFEILE_HOLEN, SCHIESS_BETRIEB enabled)
- [ ] T088 [US5] Implement CMD_ALARM action in Empfaenger/Empfaenger.ino handleCommand() (stop timers, flash red LEDs 10Hz, play alternating alarm tones 1000/2000 Hz, continue until CMD_STOP or CMD_INIT)
- [ ] T089 [US5] Ensure OK button presses < 3s still execute normal functions (no alarm trigger)

### Hardware-in-the-Loop Tests for User Story 5

- [ ] T090 [P] [US5] Test OK button held 2.9 seconds does NOT trigger alarm (normal OK function executes)
- [ ] T091 [P] [US5] Test OK button held 3.1 seconds triggers alarm from CONFIG_MENU state
- [ ] T092 [P] [US5] Test OK button held 3.1 seconds triggers alarm from PFEILE_HOLEN state
- [ ] T093 [P] [US5] Test OK button held 3.1 seconds triggers alarm from SCHIESS_BETRIEB state
- [ ] T094 [P] [US5] Test CMD_ALARM sent to receiver within 4 seconds total (3s hold + 1s transmission retries)
- [ ] T095 [P] [US5] Test sender displays brief "Alarm gesendet" confirmation (<50ms) and returns to previous state
- [ ] T096 [P] [US5] Test receiver executes alarm action correctly (flashing red LEDs, alternating alarm tones)
- [ ] T097 [P] [US5] Test alarm retry mechanism (power off receiver, hold OK 3s, verify 3 retry attempts with 200ms delays)
- [ ] T098 [P] [US5] Test alarm does NOT trigger during splash screen (safety requirement)

**Checkpoint**: All user stories should now be independently functional

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories and final validation

- [ ] T099 [P] Update Sender/README.md with new LCD, button, and configuration menu documentation
- [ ] T100 [P] Update Empfaenger/README.md with all 5 radio command descriptions and expected behaviors
- [ ] T101 [P] Update Schaltung/README.md with LCD connector and button connector documentation
- [ ] T102 [P] Update constitution.md with LCD pin assignments (A4/A5) and button pins (D2-D4) referencing KiCad schematic
- [ ] T103 [P] Add code comments referencing KiCad schematic connectors (e.g., "// J_LCD: I2C display connector, see Schaltplan.pdf")
- [ ] T104 Code review: Verify all pin assignments match /Schaltung/Schaltplan.pdf (constitution requirement)
- [ ] T105 Code review: Verify Commands.h synchronized between Sender and Empfaenger (identical enum and struct)
- [ ] T106 [P] Performance validation: Measure button response time (target: <100ms)
- [ ] T107 [P] Performance validation: Measure state transition time (target: <1s)
- [ ] T108 [P] Performance validation: Measure LCD update latency (target: <200ms)
- [ ] T109 [P] Radio range testing: Test at 10m, 25m, 50m distances (target: 99% success at 50m)
- [ ] T110 [P] Memory usage validation: Check SRAM usage (target: <1700 bytes used of 2048)
- [ ] T111 [P] Flash usage validation: Check program size (target: <30KB of 32KB)
- [ ] T112 End-to-end test: Complete full tournament workflow (power on → configure → start → shoot → stop → neustart) 100 times without errors
- [ ] T113 [P] EEPROM wear test: Write config 1000 times, verify data integrity (ensure < 1% of 100,000 write endurance)
- [ ] T114 Acceptance validation: Run all success criteria from spec.md (SC-001 through SC-008)
- [ ] T115 [P] Create user manual: How to operate tournament system (setup, configuration, running tournaments)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phase 3-7)**: All depend on Foundational phase completion
  - User stories can then proceed in parallel (if staffed)
  - Or sequentially in priority order (P1 → P2 → P3 → P4 → P5)
- **Polish (Phase 8)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) - No dependencies on other stories
- **User Story 2 (P2)**: Can start after Foundational (Phase 2) - Requires US1's config to know which timer command to send, but independently testable
- **User Story 3 (P3)**: Can start after Foundational (Phase 2) - Uses TournamentMode from US2 but adds independent feature (shooter order)
- **User Story 4 (P4)**: Can start after Foundational (Phase 2) - Uses ConfigMenu from US1 but adds independent feature (neustart with retention)
- **User Story 5 (P5)**: Can start after Foundational (Phase 2) - Completely independent alarm functionality, works in any state

### Within Each User Story

- Hardware setup (Phase 1) before any firmware changes
- Foundational infrastructure (Phase 2) before story-specific features
- Tests written and verified to FAIL before implementation begins (for test-driven approach)
- UI components before integration into main loop
- Radio commands on sender before receiver command handlers
- Error handling after basic functionality works

### Parallel Opportunities

- All Setup tasks marked [P] can run in parallel (T001-T003 hardware, T004-T010 code can run separately)
- All Foundational tasks marked [P] can run in parallel within Phase 2
- Once Foundational phase completes, all user stories can start in parallel (if team capacity allows)
- All tests for a user story marked [P] can run in parallel
- Independent firmware modules (ButtonManager, StateMachine, ConfigMenu, TournamentMode, ShooterOrder) can be developed in parallel
- Sender and Receiver firmware updates can be developed in parallel once protocol (Commands.h) is defined

---

## Parallel Example: User Story 1

```bash
# These can all run in parallel (different files, no dependencies):
Task: "T023 [P] [US1] Update Sender/SplashScreen.cpp to render 3-line LCD splash"
Task: "T032 [P] [US1] Test splash screen displays for exactly 3 seconds"
Task: "T033 [P] [US1] Test Left/Right buttons toggle time selection"
Task: "T034 [P] [US1] Test Left/Right buttons toggle shooter count"

# These must run sequentially (dependencies):
Task: "T024 [US1] Create Sender/ConfigMenu.h" BEFORE
Task: "T025 [US1] Implement ConfigMenu navigation for line 0" BEFORE
Task: "T029 [US1] Integrate ConfigMenu into main loop"
```

---

## Parallel Example: User Story 2

```bash
# These can run in parallel (different files):
Task: "T042 [P] [US2] Implement STATE_PFEILE_HOLEN button navigation"
Task: "T044 [P] [US2] Implement STATE_SCHIESS_BETRIEB button handling"
Task: "T045 [P] [US2] Implement STATE_PFEILE_HOLEN LCD rendering"
Task: "T046 [P] [US2] Implement STATE_SCHIESS_BETRIEB LCD rendering"
Task: "T049 [US2] Update receiver CMD_START_120 handler"
Task: "T050 [US2] Update receiver CMD_START_240 handler"
Task: "T051 [US2] Update receiver CMD_STOP handler"

# All US2 tests can run in parallel after implementation:
Task: "T053 [P] [US2] Test CMD_START_120 transmission"
Task: "T054 [P] [US2] Test CMD_START_240 transmission"
Task: "T055 [P] [US2] Test CMD_STOP transmission"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T010) - ~2 hours
2. Complete Phase 2: Foundational (T011-T022) - ~4 hours
3. Complete Phase 3: User Story 1 (T023-T040) - ~6 hours
4. **STOP and VALIDATE**: Test User Story 1 independently (~2 hours)
5. Deploy/demo if ready

**Total MVP effort**: ~14 hours

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready (~6 hours)
2. Add User Story 1 → Test independently → Deploy/Demo (MVP, ~8 hours total)
3. Add User Story 2 → Test independently → Deploy/Demo (~6 hours additional)
4. Add User Story 3 → Test independently → Deploy/Demo (~4 hours additional)
5. Add User Story 4 → Test independently → Deploy/Demo (~2 hours additional)
6. Add User Story 5 → Test independently → Deploy/Demo (~4 hours additional)
7. Polish phase → Final validation and documentation (~4 hours)

**Total full feature effort**: ~34 hours

### Parallel Team Strategy

With 2 developers:

1. Together: Complete Setup + Foundational (~6 hours)
2. Once Foundational is done:
   - Developer A: User Story 1 + User Story 3 (~10 hours)
   - Developer B: User Story 2 + User Story 4 (~8 hours)
3. Together: User Story 5 + Polish (~6 hours)

**Total elapsed time**: ~20 hours (1.7x speedup)

With 3 developers:

1. Together: Setup + Foundational (~6 hours)
2. Split user stories:
   - Developer A: US1 + tests (~8 hours)
   - Developer B: US2 + tests (~6 hours)
   - Developer C: US3 + US4 + tests (~6 hours)
3. Together: US5 + Polish (~6 hours)

**Total elapsed time**: ~14 hours (2.4x speedup)

---

## Notes

- [P] tasks = different files, no dependencies, can run in parallel
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Hardware-in-the-loop tests require physical Arduino Nano hardware with LCD, buttons, nRF24 modules
- Tests validate against acceptance criteria from spec.md and success criteria (SC-001 through SC-008)
- Commit after each task or logical group for incremental progress tracking
- Stop at any checkpoint to validate story independently before proceeding
- Constitution compliance checked in T104 (pin assignments) and T105 (command synchronization)
- Schematic updates (T001-T003) are CRITICAL prerequisite before firmware implementation begins

---

## Task Summary

**Total Tasks**: 115

**Tasks per Phase**:
- Phase 1 (Setup): 10 tasks
- Phase 2 (Foundational): 12 tasks
- Phase 3 (User Story 1): 18 tasks (9 implementation + 9 tests)
- Phase 4 (User Story 2): 21 tasks (12 implementation + 9 tests)
- Phase 5 (User Story 3): 16 tasks (8 implementation + 8 tests)
- Phase 6 (User Story 4): 7 tasks (3 implementation + 4 tests)
- Phase 7 (User Story 5): 14 tasks (5 implementation + 9 tests)
- Phase 8 (Polish): 17 tasks

**Parallelizable Tasks**: 72 tasks marked with [P]

**Parallel Opportunities**:
- Phase 1: 7 of 10 tasks can run in parallel
- Phase 2: 5 of 12 tasks can run in parallel
- Phase 3: 10 of 18 tasks can run in parallel
- Phase 4: 14 of 21 tasks can run in parallel
- Phase 5: 13 of 16 tasks can run in parallel
- Phase 6: 6 of 7 tasks can run in parallel
- Phase 7: 10 of 14 tasks can run in parallel
- Phase 8: 13 of 17 tasks can run in parallel

**Suggested MVP Scope**: Phase 1 + Phase 2 + Phase 3 (User Story 1 only) = 40 tasks = ~14 hours

**Format Validation**: ✅ ALL tasks follow checklist format with checkbox, ID, [P]/[Story] labels, and file paths
