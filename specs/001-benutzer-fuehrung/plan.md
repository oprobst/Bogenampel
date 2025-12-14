# Implementation Plan: Archery Tournament Control System - User Guidance

**Branch**: `001-benutzer-fuehrung` | **Date**: 2025-12-13 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-benutzer-fuehrung/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/commands/plan.md` for the execution workflow.

## Summary

This feature implements the complete user guidance workflow for the archery tournament control system (Bogenampel). The sender device provides an interactive configuration menu with button navigation (Left/Right/OK) to set shooting time (120s or 240s) and shooter count (1-2 or 3-4 per target), followed by a tournament mode that alternates between "Pfeile holen" (arrow retrieval) and "Schießbetrieb" (active shooting). The receiver executes commands (Stop, Start 120Sek, Start 240Sek, Init, Alarm) received via nRF24L01+ radio communication. The system includes a 3-second splash screen on startup, configuration persistence on restart, and a 3-second long-press emergency alarm function.

## Technical Context

**Language/Version**: C++ (Arduino, C++11 compatible with Arduino IDE/PlatformIO)
**Primary Dependencies**:
  - nRF24L01+ radio library (RF24)
  - WS2812B LED strip library (FastLED or Adafruit_NeoPixel)
  - Arduino core libraries (standard button handling, timing)
**Storage**: EEPROM (Arduino internal, for configuration persistence across power cycles)
**Testing**: Hardware-in-the-loop testing (physical devices), unit testing for state machine logic
**Target Platform**: Arduino Nano (ATmega328P, 5V/16MHz) - both Sender and Receiver devices
**Project Type**: Embedded firmware (dual-device system: Sender + Receiver)
**Performance Goals**:
  - Button response time: <100ms
  - Radio transmission latency: <500ms
  - Display update latency: <200ms
  - Splash screen duration: 3 seconds
  - State transition time: <1 second
**Constraints**:
  - Memory: 2KB SRAM, 32KB Flash (Arduino Nano)
  - Power: Sender 9V battery or USB, Receiver USB powerbank (8+ hour operation)
  - Radio range: minimum 50 meters
  - Display: Multi-line text display with button navigation
  - Long-press detection: 3 seconds for alarm trigger
**Scale/Scope**:
  - 2 device firmware projects (Sender, Receiver)
  - ~5-8 UI screens (splash, config menu, tournament modes)
  - 5 radio commands (Stop, Start 120Sek, Start 240Sek, Init, Alarm)
  - 2 tournament states (Pfeile holen, Schießbetrieb)
  - 3-button interface (Left, Right, OK)

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

### I. Sicherheit Zuerst (Safety First) ✅

- ✅ **Fail-Safe**: Emergency alarm (3-second long-press) accessible from all states
- ✅ **Deterministic state transitions**: Configuration → Tournament → Pfeile holen ↔ Schießbetrieb clearly defined
- ✅ **Command validation**: Radio commands must be validated before execution (Stop, Start, Init, Alarm)
- ✅ **Clear state indication**: Display always shows current state at top ("Pfeile holen" or "Schießbetrieb")
- ⚠️ **Radio failure handling**: Edge case identified - behavior on radio communication failure needs research

**Action Required**: Phase 0 research must address radio communication failure scenarios and timeout handling.

### II. Einfachheit & Zuverlässigkeit (Simplicity & Reliability) ✅

- ✅ **Minimal inputs**: 3-button interface (Left, Right, OK) for all operations
- ✅ **Plug-and-play**: Configuration menu accessible immediately after 3-second splash screen
- ✅ **Configuration persistence**: Settings retained on "Neustart" for convenience
- ✅ **Standard components**: Uses existing Arduino, nRF24L01+, WS2812B, button infrastructure
- ⚠️ **Power management**: EEPROM writes on configuration changes - need to minimize wear

**Action Required**: Phase 0 research must determine optimal EEPROM write strategy (immediate vs. on tournament start).

### III. Embedded-Hardware-Standards (Embedded Hardware Standards) ✅

- ✅ **Arduino compatibility**: C++ code targeting ATmega328P with Arduino core
- ✅ **Pin assignments**: Must reference KiCad schematic `/Schaltung/Schaltplan.pdf`
- ✅ **Open source libraries**: RF24 (nRF24), FastLED/NeoPixel (WS2812B), Arduino core
- ✅ **Pin documentation**: All new button pins (Left, Right, OK) must be documented as constants
- ⚠️ **Display hardware**: Specification mentions "Monitor" but existing hardware uses WS2812B LEDs

**Action Required**: Phase 0 research must clarify display hardware - is this a new LCD/OLED display component or visualization using existing WS2812B LEDs?

### IV. Testbarkeit & Validierung (Testability & Validation) ✅

- ✅ **State machine testability**: Each configuration step and tournament state independently testable
- ✅ **Button testing**: Left/Right/OK navigation can be tested per acceptance scenarios
- ✅ **Radio testing**: Commands (Stop, Start 120/240, Init, Alarm) individually testable
- ✅ **Boundary conditions**: Edge cases identified (simultaneous buttons, exact 3s press, radio failures)
- ✅ **Long-press detection**: 3-second alarm trigger testable with timing validation

**Passes**: All functional requirements mapped to testable acceptance scenarios.

### V. Wartbarkeit & Dokumentation (Maintainability & Documentation) ✅

- ✅ **Pin assignments**: All button pins must be defined as constants in Config.h
- ✅ **State transitions**: State machine changes must be documented in code comments
- ✅ **Schematic compliance**: Pin assignments must reference KiCad schematic connectors (J1, J2, J3, J5)
- ✅ **Clear naming**: Configuration states, tournament modes, and button handlers need descriptive names
- ⚠️ **Hardware documentation**: New display component needs schematic update if LCD/OLED

**Action Required**: Phase 0 research must confirm display hardware and ensure schematic alignment before implementation.

### Constitution Gate Status: ⚠️ CONDITIONAL PASS

**Passing**: Core safety, simplicity, testability, and documentation principles satisfied.

**Blockers for Phase 0**:
1. **Display hardware clarification** (Principle III) - MUST resolve before design phase
2. **Radio failure handling** (Principle I) - MUST define timeout/retry strategy
3. **EEPROM write strategy** (Principle II) - SHOULD optimize to minimize wear

**Gate Decision**: PROCEED to Phase 0 research with mandate to resolve all ⚠️ items before Phase 1.

## Project Structure

### Documentation (this feature)

```text
specs/001-benutzer-fuehrung/
├── spec.md              # Feature specification (user requirements)
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
│   ├── radio-protocol.md      # nRF24 command definitions
│   └── state-machine.md       # UI state transitions
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

**Embedded dual-device structure** - Sender and Receiver firmware projects:

```text
Sender/                         # Sender device firmware (existing)
├── Sender.ino                  # Main sketch
├── Config.h                    # Pin assignments and constants
├── Commands.h                  # Radio command definitions
├── ButtonManager.h/.cpp        # Button handling (existing - to be extended)
├── StateMachine.h/.cpp         # State machine (existing - to be extended)
├── SplashScreen.h/.cpp         # Splash screen (existing)
├── ConfigMenu.h/.cpp           # NEW: Configuration menu UI
├── TournamentMode.h/.cpp       # NEW: Tournament mode state handler
└── README.md                   # Sender documentation

Empfaenger/                     # Receiver device firmware (existing)
├── Empfaenger.ino              # Main sketch
├── Config.h                    # Pin assignments
├── Commands.h                  # Radio command definitions (matches Sender)
├── CommandHandler.h/.cpp       # NEW: Execute received commands
└── README.md                   # Receiver documentation

Schaltung/                      # Hardware schematics (KiCad)
└── Schaltplan.pdf              # Authoritative pin assignments

tests/                          # Hardware-in-the-loop and unit tests
├── sender-state-tests/         # NEW: Sender state machine tests
├── button-navigation-tests/    # NEW: Configuration menu navigation tests
└── radio-protocol-tests/       # NEW: Command transmission/reception tests
```

**Structure Decision**:
- **Embedded dual-device**: Sender and Receiver are separate Arduino projects with independent firmware
- **Existing codebase extension**: This feature extends existing Sender/StateMachine.cpp and adds new modules (ConfigMenu, TournamentMode)
- **Shared definitions**: Commands.h must be synchronized between Sender and Empfaenger
- **Hardware reference**: All pin assignments reference `/Schaltung/Schaltplan.pdf` (KiCad schematic)

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No constitution violations requiring justification. All ⚠️ items in Constitution Check are research tasks, not complexity violations.

---

## Constitution Re-Check (Post Phase 1 Design)

*Re-evaluation after Phase 0 research and Phase 1 design completion*

### I. Sicherheit Zuerst (Safety First) ✅

- ✅ **Radio failure handling RESOLVED**: Timeout strategy defined (500ms), error feedback implemented
- ✅ **Fail-Safe maintained**: Alarm accessible from all states, radio retries for critical commands
- ✅ **State transitions deterministic**: State machine contract defines all transitions
- ✅ **Command validation**: Checksum validation on receiver side

**Status**: PASS - All safety requirements addressed in design

### II. Einfachheit & Zuverlässigkeit (Simplicity & Reliability) ✅

- ✅ **EEPROM strategy RESOLVED**: Write on "Start" press only, minimizes wear to acceptable levels
- ✅ **Simple UI maintained**: 3-button interface, clear navigation paths
- ✅ **Configuration persistence**: Settings retained across power cycles and "Neustart"

**Status**: PASS - Simplicity and reliability maintained

### III. Embedded-Hardware-Standards (Embedded Hardware Standards) ✅

- ✅ **Display hardware RESOLVED**: 16x2 LCD with I2C selected (standard Arduino component)
- ✅ **Pin assignments documented**: LCD I2C (A4/A5), buttons (D2-D4) specified in quickstart
- ⚠️ **Schematic update PENDING**: LCD connector must be added to `/Schaltung/` before implementation

**Status**: CONDITIONAL PASS - Schematic update required before code implementation (documented in quickstart Phase 1)

### IV. Testbarkeit & Validierung (Testability & Validation) ✅

- ✅ **All states testable**: Contracts define expected behavior for each state and transition
- ✅ **Test cases defined**: Quickstart includes comprehensive testing checklist
- ✅ **Hardware-in-the-loop**: Test procedures documented for radio, buttons, display

**Status**: PASS - Testability ensured through contracts and test plan

### V. Wartbarkeit & Dokumentation (Maintainability & Documentation) ✅

- ✅ **Pin assignments documented**: Config.h references schematic connectors
- ✅ **State machine documented**: Complete contract with all transitions
- ✅ **Radio protocol documented**: Packet format, timing, error handling specified
- ✅ **Developer guide**: Quickstart provides clear implementation roadmap

**Status**: PASS - Comprehensive documentation in place

### Final Constitution Gate Status: ✅ PASS WITH CONDITIONS

**All ⚠️ items from initial check RESOLVED**:
1. ✅ Display hardware clarified (16x2 LCD I2C)
2. ✅ Radio failure handling defined (timeout + retry strategy)
3. ✅ EEPROM write strategy optimized (write on "Start" only)

**Remaining Action Item**:
- **Schematic Update**: Add LCD connector to `/Schaltung/` KiCad project before implementation begins (documented in quickstart.md Phase 1, Task 1.1)

**Gate Decision**: ✅ APPROVED to proceed to Phase 2 (Tasks generation via `/speckit.tasks`)

**Design Completeness**: All contracts, data models, and implementation guidance complete. Ready for task breakdown and implementation.
