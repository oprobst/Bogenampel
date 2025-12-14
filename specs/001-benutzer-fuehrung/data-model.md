# Data Model: User Guidance System

**Feature**: 001-benutzer-fuehrung
**Date**: 2025-12-13
**Status**: Phase 1 - Design

## Overview

This document defines the data structures and state models for the user guidance system. The system consists of:
1. **Sender** (control device with LCD display and button interface)
2. **Receiver** (timer display device executing radio commands)

---

## 1. Tournament Configuration

**Purpose**: Stores user-selected tournament parameters

**Scope**: Sender device only (persisted in EEPROM)

### Structure

```cpp
// Location: Sender/Config.h
struct TournamentConfig {
  uint8_t shootingTime;      // 120 or 240 (seconds)
  uint8_t shooterCount;      // 2 (1-2 shooters) or 4 (3-4 shooters)
  uint8_t checksum;          // CRC8 validation
};

// EEPROM address
#define EEPROM_CONFIG_ADDR 0

// Valid values
enum ShootingTime {
  TIME_120_SEC = 120,
  TIME_240_SEC = 240
};

enum ShooterCount {
  SHOOTERS_1_2 = 2,   // Display as "1-2 Schützen"
  SHOOTERS_3_4 = 4    // Display as "3-4 Schützen"
};
```

### Validation Rules

- `shootingTime`: MUST be exactly 120 or 240
- `shooterCount`: MUST be exactly 2 or 4
- `checksum`: MUST match CRC8 of shootingTime + shooterCount

### State Transitions

```
Power On → Load from EEPROM (with validation) → Apply to UI
Config Menu "Start" → Validate → Save to EEPROM → Enter Tournament Mode
"Neustart" → Load from EEPROM → Restore Config Menu
```

### Persistence

- **Write**: On "Start" button press (entering tournament mode)
- **Read**: On power-up and on "Neustart"
- **Default**: If EEPROM invalid or first boot: `{120, 2, checksum}`

---

## 2. Tournament State

**Purpose**: Represents current operational mode of the tournament

**Scope**: Sender device (drives UI and radio commands)

### Structure

```cpp
// Location: Sender/StateMachine.h
enum TournamentState {
  STATE_SPLASH,             // Initial splash screen (3 seconds)
  STATE_CONFIG_MENU,        // Configuration menu active
  STATE_PFEILE_HOLEN,       // Arrow retrieval mode (tournament running)
  STATE_SCHIESS_BETRIEB     // Active shooting mode (tournament running)
};

class StateMachine {
private:
  TournamentState currentState;
  TournamentState previousState;  // For alarm return
  unsigned long stateEnterTime;   // For splash screen timing

public:
  void setState(TournamentState newState);
  TournamentState getState() const;
  bool isInTournamentMode() const;  // true if PFEILE_HOLEN or SCHIESS_BETRIEB
};
```

### State Transitions

```
[Power On]
  ↓
SPLASH (3 seconds)
  ↓
CONFIG_MENU
  ↓ (Left/Right navigation, OK confirm, "Start" button)
PFEILE_HOLEN ←→ SCHIESS_BETRIEB
  ↑              ↓
  └─ "Neustart" ─┘

[Alarm: OK long-press from any state]
  → Send Alarm command → Return to previous state
```

### Determinism Requirements (Constitution Principle I)

- Each button press MUST result in predictable state transition
- State entry/exit MUST be clearly defined
- No ambiguous states allowed

---

## 3. Shooter Order

**Purpose**: Sequence of shooting groups/lanes during tournament

**Scope**: Sender device (displayed, modified, transmitted to receiver)

### Structure

```cpp
// Location: Sender/TournamentMode.h
#define MAX_SHOOTERS 12  // Example: 12 shooting positions

class ShooterOrder {
private:
  uint8_t order[MAX_SHOOTERS];  // Array of shooter IDs (1-12)
  uint8_t count;                 // Number of active shooters

public:
  void setOrder(const uint8_t* newOrder, uint8_t size);
  const uint8_t* getOrder() const;
  uint8_t getCount() const;
  void reset();  // Reset to default order (1, 2, 3, ..., N)

  // Display formatting
  String formatForDisplay() const;  // e.g., "1-2-3-4-5-6"
};
```

### Default Order

- **On tournament start**: Sequential (1, 2, 3, ..., N where N = number of lanes/targets)
- **User-modified**: Persists within current tournament session
- **Reset on "Neustart"**: Returns to sequential default

### Display Requirements

- **Location**: Bottom of LCD screen (both Pfeile Holen and Schießbetrieb states)
- **Format**: `"Order: 1-3-2-4"` or similar compact representation
- **Maximum length**: Constrained by LCD width (16 or 20 characters)

---

## 4. Radio Command

**Purpose**: Control messages sent from Sender to Receiver via nRF24L01+

**Scope**: Both Sender and Receiver (must match exactly)

### Structure

```cpp
// Location: Sender/Commands.h and Empfaenger/Commands.h (MUST BE IDENTICAL)

enum RadioCommand : uint8_t {
  CMD_STOP = 0x01,          // Stop timer, show red light
  CMD_START_120 = 0x02,     // Start 120-second countdown
  CMD_START_240 = 0x03,     // Start 240-second countdown
  CMD_INIT = 0x04,          // Initialize receiver (tournament start)
  CMD_ALARM = 0x05          // Emergency alarm signal
};

struct RadioPacket {
  RadioCommand command;
  uint8_t checksum;  // Simple XOR checksum for validation
};
```

### Command Semantics

| Command | Sender Context | Receiver Action |
|---------|----------------|-----------------|
| `CMD_STOP` | "Passe beenden" button | Stop timer, set LEDs to red, stop buzzer |
| `CMD_START_120` | "Nächste Passe" + 120s config | Start 120-second countdown, green LEDs |
| `CMD_START_240` | "Nächste Passe" + 240s config | Start 240-second countdown, green LEDs |
| `CMD_INIT` | "Start" button (enter tournament) | Reset receiver state, prepare for first round |
| `CMD_ALARM` | OK button held 3+ seconds | Trigger alarm buzzer and red flashing LEDs |

### Transmission Protocol

```cpp
// Sender transmission
bool sendCommand(RadioCommand cmd) {
  RadioPacket packet;
  packet.command = cmd;
  packet.checksum = cmd ^ 0xFF;  // Simple XOR checksum

  radio.setRetries(5, 15);  // 1.25ms delay, 15 retries
  bool success = radio.write(&packet, sizeof(packet));

  return success;  // true if ACK received, false if timeout
}

// Receiver validation
bool receiveCommand(RadioCommand& cmd) {
  if (radio.available()) {
    RadioPacket packet;
    radio.read(&packet, sizeof(packet));

    // Validate checksum
    if (packet.checksum != (packet.command ^ 0xFF)) {
      return false;  // Corrupted packet
    }

    cmd = packet.command;
    return true;
  }
  return false;
}
```

### Error Handling

- **Timeout**: 500ms max (15 retries with 1.25ms delay ≈ 20ms + safety margin)
- **Validation failure**: Ignore packet, wait for retry
- **User feedback**: LCD error message or LED flash on sender

---

## 5. User Interface State

**Purpose**: Tracks current screen, cursor position, and button selections

**Scope**: Sender device only

### Structure

```cpp
// Location: Sender/ConfigMenu.h
class ConfigMenu {
private:
  uint8_t cursorLine;        // 0, 1, or 2 (three config lines)
  uint8_t selectedTime;      // 120 or 240
  uint8_t selectedCount;     // 2 or 4
  uint8_t selectedButton;    // 0 = "Ändern", 1 = "Start"

public:
  void handleLeftButton();
  void handleRightButton();
  void handleOKButton();
  void render(LiquidCrystal_I2C& lcd);
};

// Location: Sender/TournamentMode.h
class TournamentModeUI {
private:
  TournamentState state;     // PFEILE_HOLEN or SCHIESS_BETRIEB
  ShooterOrder shooterOrder;
  uint8_t selectedAction;    // Button selection index

public:
  void handleButtons();
  void render(LiquidCrystal_I2C& lcd);
  void displayStateHeader();     // "Pfeile holen" or "Schießbetrieb"
  void displayShooterOrder();    // Bottom line: "Order: 1-2-3..."
};
```

### Configuration Menu Screen Layout

```
Line 0: [Zeit: 120s <-]    or  [Zeit: 240s <-]
Line 1: [Schützen: 1-2 <-]  or  [Schützen: 3-4 <-]
Line 2: [< Ändern  Start >]
         ^^^^^^^^  ^^^^^
         selected  default
```

### Tournament Mode Screen Layouts

**Pfeile Holen**:
```
Line 0: Pfeile holen
Line 1: [Nächste Passe] [Reihenfolge] [Neustart]
        ^^^^^^^^^^^^^^^ (selected button highlighted)
Line 2/3: Order: 1-2-3-4-5-6
```

**Schießbetrieb**:
```
Line 0: Schießbetrieb
Line 1: [Passe beenden]
        ^^^^^^^^^^^^^^^ (only button)
Line 2/3: Order: 1-2-3-4-5-6
```

### Navigation Rules

- **Left/Right**: Cycle through options on current line
- **OK**: Confirm selection and move to next line (config) or execute action (tournament)
- **Cursor wrapping**: Left at first option → move to last option; Right at last → move to first

---

## 6. Alarm State

**Purpose**: Emergency alarm trigger state

**Scope**: Sender device (detects long-press, sends command, manages UI)

### Structure

```cpp
// Location: Sender/ButtonManager.h
class AlarmDetector {
private:
  unsigned long okPressStartTime;  // millis() when OK pressed
  bool okPressActive;
  bool alarmTriggered;

public:
  void updateOKButton(bool pressed);  // Call in loop()
  bool isAlarmTriggered();            // Returns true once per alarm
  void reset();                        // Clear alarm state
};

// Timing
#define ALARM_THRESHOLD_MS 3000  // 3 seconds
```

### Detection Logic

```cpp
void AlarmDetector::updateOKButton(bool pressed) {
  if (pressed && !okPressActive) {
    // OK button just pressed
    okPressStartTime = millis();
    okPressActive = true;
    alarmTriggered = false;
  }
  else if (pressed && okPressActive) {
    // OK button held - check duration
    unsigned long duration = millis() - okPressStartTime;
    if (duration >= ALARM_THRESHOLD_MS && !alarmTriggered) {
      alarmTriggered = true;  // Trigger alarm once
    }
  }
  else if (!pressed && okPressActive) {
    // OK button released
    okPressActive = false;
    alarmTriggered = false;
  }
}
```

### Behavior Requirements (from spec)

- **Threshold**: > 3 seconds (exactly 3s is ambiguous, use > 3000ms)
- **Availability**: All states (SPLASH, CONFIG_MENU, PFEILE_HOLEN, SCHIESS_BETRIEB)
- **Action**: Send CMD_ALARM, return to previous state immediately (no confirmation)
- **Normal OK press**: < 3s does not trigger alarm, executes normal function

---

## Entity Relationship Diagram

```
┌─────────────────────┐
│ TournamentConfig    │
│ - shootingTime      │
│ - shooterCount      │
└──────────┬──────────┘
           │ used by
           ↓
┌─────────────────────┐     ┌─────────────────────┐
│ ConfigMenu (UI)     │────→│ StateMachine        │
│ - cursorLine        │     │ - currentState      │
│ - selectedTime      │     │ - previousState     │
│ - selectedCount     │     └──────────┬──────────┘
└─────────────────────┘                │
                                       │ controls
                                       ↓
┌─────────────────────┐     ┌─────────────────────┐
│ TournamentModeUI    │     │ RadioCommand        │
│ - state             │     │ - command           │
│ - shooterOrder      │────→│ - checksum          │
└─────────────────────┘     └─────────────────────┘
           ↓                           ↓
┌─────────────────────┐     ┌─────────────────────┐
│ ShooterOrder        │     │ Receiver Device     │
│ - order[]           │     │ (executes commands) │
│ - count             │     └─────────────────────┘
└─────────────────────┘

┌─────────────────────┐
│ AlarmDetector       │
│ - okPressStartTime  │
│ - alarmTriggered    │
└─────────────────────┘
         │ triggers
         ↓
  RadioCommand(CMD_ALARM)
```

---

## Memory Budget (Arduino Nano ATmega328P)

**SRAM: 2048 bytes**

| Component | Size | Notes |
|-----------|------|-------|
| TournamentConfig | 3 bytes | EEPROM-backed, loaded to SRAM |
| StateMachine | ~10 bytes | State enum + timestamps |
| ShooterOrder | ~14 bytes | 12 shooter IDs + count |
| ConfigMenu | ~8 bytes | Cursor + selections |
| TournamentModeUI | ~20 bytes | State + order + UI state |
| AlarmDetector | ~6 bytes | Timestamps + flags |
| RF24 Library | ~200 bytes | Radio buffers |
| LiquidCrystal_I2C | ~50 bytes | LCD buffers |
| **Total** | ~311 bytes | ~15% of SRAM |

**Remaining**: ~1700 bytes for stack, local variables, and library overhead

**Flash: 32 KB** - Adequate for code (existing firmware ~15-20 KB, new features ~5-8 KB)

---

## Validation Rules Summary

| Entity | Validation | Enforcement |
|--------|------------|-------------|
| TournamentConfig | shootingTime ∈ {120, 240}, shooterCount ∈ {2, 4} | Load from EEPROM, default fallback |
| RadioCommand | checksum == command XOR 0xFF | Receiver discards invalid packets |
| ShooterOrder | count ≤ MAX_SHOOTERS, order[i] unique | UI constraints + validation |
| AlarmDetector | pressDuration > 3000ms | millis() comparison |
| StateMachine | Valid state transitions only | State machine enforcement |

---

## Next Steps

1. ✅ Data model defined
2. → Generate contracts (radio protocol, state machine transitions)
3. → Create quickstart guide for developers
4. → Update agent context with new libraries and structures
