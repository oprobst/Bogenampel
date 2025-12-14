# State Machine Contract: Sender UI Navigation

**Feature**: 001-benutzer-fuehrung
**Date**: 2025-12-13
**Version**: 1.0

## Overview

This document defines the complete state machine for the Sender device user interface. It specifies all states, transitions, button behaviors, and display rendering for the tournament control system.

**Principles**:
- **Deterministic**: Every button press has exactly one defined outcome
- **Safe**: All states reachable, no dead ends (except power-off)
- **Simple**: Minimal state count, clear navigation paths
- **Testable**: Each transition independently verifiable

---

## State Diagram

```
                    [POWER ON]
                        ↓
        ┌───────────────────────────────┐
        │  SPLASH                       │
        │  (3 seconds)                  │
        │  Display: Logo/Title          │
        └───────────────────────────────┘
                        ↓ (auto after 3s)
        ┌───────────────────────────────┐
        │  CONFIG_MENU                  │
        │  Navigate with L/R/OK         │
        │  Lines: Time, Shooters, Btns  │
        └───────────────────────────────┘
                        ↓ "Start" + OK
                        │ (Send CMD_INIT)
        ┌───────────────────────────────┐
        │  PFEILE_HOLEN                 │
        │  Buttons: Nächste Passe,      │
        │           Reihenfolge,        │
        │           Neustart            │
        └───────────────────────────────┘
         ↓ Nächste Passe    ↑ Neustart
         │ (CMD_START_xxx)  │
         ↓                  │
        ┌───────────────────────────────┐
        │  SCHIESS_BETRIEB              │
        │  Button: Passe beenden        │
        └───────────────────────────────┘
                        ↓ Passe beenden
                        │ (Send CMD_STOP)
                        ↑
                (back to PFEILE_HOLEN)

        [ALARM: OK hold > 3s from ANY state]
                → Send CMD_ALARM
                → Return to previous state
```

---

## State Definitions

### STATE_SPLASH

**Purpose**: Display startup logo/title for 3 seconds

**Entry Conditions**:
- Power-on reset
- (No re-entry after initial boot)

**State Variables**:
```cpp
unsigned long splashStartTime;  // millis() when splash entered
#define SPLASH_DURATION_MS 3000
```

**Display**:
```
Line 0: "  Bogenampel   "
Line 1: "  Steuerung    "
Line 2: "  Version 1.0  "
```

**Button Behavior**:
- **Left**: No action
- **Right**: No action
- **OK**: No action (buttons disabled during splash)
- **OK hold > 3s**: No action (alarm disabled during splash for safety)

**Exit Transition**:
```cpp
if (millis() - splashStartTime >= SPLASH_DURATION_MS) {
  setState(STATE_CONFIG_MENU);
}
```

**Radio Commands**: None

---

### STATE_CONFIG_MENU

**Purpose**: Configure tournament parameters (time, shooter count)

**Entry Conditions**:
- From SPLASH after 3 seconds
- From PFEILE_HOLEN via "Neustart" button

**State Variables**:
```cpp
uint8_t cursorLine;        // 0, 1, or 2 (which line is active)
uint8_t selectedTime;      // 120 or 240
uint8_t selectedCount;     // 2 (1-2) or 4 (3-4)
uint8_t selectedButton;    // 0 = "Ändern", 1 = "Start"
```

**Initialization** (on entry from SPLASH):
```cpp
cursorLine = 0;
selectedTime = loadConfigFromEEPROM().shootingTime;    // Or default 120
selectedCount = loadConfigFromEEPROM().shooterCount;   // Or default 2
selectedButton = 1;  // Default to "Start"
```

**Initialization** (on entry from "Neustart"):
```cpp
// Keep current selectedTime and selectedCount (spec requirement)
cursorLine = 0;
selectedButton = 1;
```

**Display**:
```
Line 0: Zeit: [120|240]s [<-]  (cursor on line 0)
Line 1: Schützen: [1-2|3-4]    (cursor on line 1)
Line 2: [< Ändern  Start >]    (cursor on line 2, highlight selected)
```

**Cursor Indicator**: `[<-]` or highlight active line

**Button Behavior**:

#### Line 0 (Time Selection)
- **Left**: Toggle 240s → 120s
- **Right**: Toggle 120s → 240s
- **OK**: Move cursor to line 1 (shooters)

#### Line 1 (Shooter Count Selection)
- **Left**: Toggle 3-4 → 1-2
- **Right**: Toggle 1-2 → 3-4
- **OK**: Move cursor to line 2 (buttons)

#### Line 2 (Action Buttons)
- **Left**: Select "Ändern" (selectedButton = 0)
- **Right**: Select "Start" (selectedButton = 1)
- **OK**:
  - If "Ändern" selected: Move cursor to line 0
  - If "Start" selected: Save config to EEPROM, send CMD_INIT, enter PFEILE_HOLEN

**Exit Transitions**:
```cpp
if (selectedButton == 0 && okPressed) {
  // "Ändern" - loop back to line 0
  cursorLine = 0;
}
else if (selectedButton == 1 && okPressed) {
  // "Start" - enter tournament mode
  saveConfigToEEPROM();
  sendCommand(CMD_INIT);
  setState(STATE_PFEILE_HOLEN);
}
```

**Radio Commands**:
- `CMD_INIT` on "Start" button press

**Alarm Behavior**:
- OK hold > 3s: Send CMD_ALARM, return to CONFIG_MENU (same state)

---

### STATE_PFEILE_HOLEN (Arrow Retrieval Mode)

**Purpose**: Tournament mode - waiting for next shooting round

**Entry Conditions**:
- From CONFIG_MENU via "Start" button
- From SCHIESS_BETRIEB via "Passe beenden" button

**State Variables**:
```cpp
uint8_t selectedAction;    // 0 = Nächste Passe, 1 = Reihenfolge, 2 = Neustart
ShooterOrder shooterOrder; // Display at bottom
```

**Initialization**:
```cpp
selectedAction = 0;  // Default to "Nächste Passe"
// shooterOrder retains previous value or resets to 1-2-3-...
```

**Display**:
```
Line 0: Pfeile holen
Line 1: [Nächste Passe] [Reihenfolge] [Neustart]
        ^^^^^^^^^^^^^^^ (selected button highlighted)
Line 2/3: Order: 1-2-3-4-5-6
```

**Button Behavior**:
- **Left**: Cycle selection left (Neustart ← Reihenfolge ← Nächste Passe ← wrap)
- **Right**: Cycle selection right (Nächste Passe → Reihenfolge → Neustart → wrap)
- **OK**:
  - If "Nächste Passe": Send CMD_START_120 or CMD_START_240 (based on config), enter SCHIESS_BETRIEB
  - If "Reihenfolge": Enter shooter order edit mode (sub-state, see below)
  - If "Neustart": Return to CONFIG_MENU (retains config)

**Exit Transitions**:
```cpp
if (selectedAction == 0 && okPressed) {
  // Nächste Passe
  RadioCommand cmd = (selectedTime == 120) ? CMD_START_120 : CMD_START_240;
  if (sendCommand(cmd) == TX_SUCCESS) {
    setState(STATE_SCHIESS_BETRIEB);
  } else {
    // Display error, stay in PFEILE_HOLEN
    showError("Empfänger?");
  }
}
else if (selectedAction == 1 && okPressed) {
  // Reihenfolge - enter sub-state (not fully specified in this contract)
  enterShooterOrderEdit();
}
else if (selectedAction == 2 && okPressed) {
  // Neustart
  setState(STATE_CONFIG_MENU);  // Config values retained
}
```

**Radio Commands**:
- `CMD_START_120` or `CMD_START_240` on "Nächste Passe"
- None on "Reihenfolge" or "Neustart"

**Alarm Behavior**:
- OK hold > 3s: Send CMD_ALARM, return to PFEILE_HOLEN

---

#### Sub-State: Shooter Order Edit

**Purpose**: Modify shooting order during "Reihenfolge" action

**Display**:
```
Line 0: Reihenfolge
Line 1: Pos 1: [_1_] (editing position 1)
Line 2: Order: _1_-2-3-4
```

**Button Behavior**:
- **Left**: Decrement shooter ID at current position
- **Right**: Increment shooter ID at current position
- **OK**: Move to next position (or exit when complete)

**Exit**: Return to PFEILE_HOLEN state when all positions edited

**Note**: Detailed implementation deferred to tasks phase (not critical for contract)

---

### STATE_SCHIESS_BETRIEB (Active Shooting Mode)

**Purpose**: Tournament mode - shooting round in progress

**Entry Conditions**:
- From PFEILE_HOLEN via "Nächste Passe" button

**State Variables**:
```cpp
// No selections needed - only one button
ShooterOrder shooterOrder;  // Display at bottom (same as PFEILE_HOLEN)
```

**Display**:
```
Line 0: Schießbetrieb
Line 1: [Passe beenden]
        ^^^^^^^^^^^^^^^ (only button, always highlighted)
Line 2/3: Order: 1-2-3-4-5-6
```

**Button Behavior**:
- **Left**: No action (only one button)
- **Right**: No action (only one button)
- **OK**: Send CMD_STOP, return to PFEILE_HOLEN

**Exit Transition**:
```cpp
if (okPressed) {
  if (sendCommand(CMD_STOP) == TX_SUCCESS) {
    setState(STATE_PFEILE_HOLEN);
  } else {
    // Display error, stay in SCHIESS_BETRIEB
    showError("Empfänger?");
  }
}
```

**Radio Commands**:
- `CMD_STOP` on "Passe beenden"

**Alarm Behavior**:
- OK hold > 3s: Send CMD_ALARM, return to SCHIESS_BETRIEB

---

## Alarm State Handling (Global)

**Trigger**: OK button held > 3000ms in ANY state

**Implementation**:
```cpp
// In main loop, before state-specific button handling
if (alarmDetector.isAlarmTriggered()) {
  TournamentState previousState = stateMachine.getState();

  // Send alarm command with retries
  TransmissionResult result = sendAlarmWithRetry();

  if (result == TX_SUCCESS) {
    // Brief feedback (50ms LCD flash or beep)
    showAlarmConfirmation();
    delay(50);
  } else {
    // Error - couldn't send alarm
    showError("Alarm Fehler!");
  }

  // Return to previous state immediately (no confirmation screen)
  // stateMachine.setState(previousState);  // Already at previous state
}
```

**Display Feedback** (brief, 50ms):
```
Line 0: ALARM GESENDET
```

**Return Behavior**: Immediately return to whatever state was active (no separate alarm state)

---

## Button Debouncing

**Requirement**: Prevent multiple triggers from single physical press

**Implementation**:
```cpp
#define DEBOUNCE_DELAY_MS 50

class ButtonManager {
private:
  uint8_t lastButtonState[3];    // LEFT, RIGHT, OK
  unsigned long lastDebounceTime[3];

public:
  bool isButtonPressed(uint8_t button) {
    uint8_t reading = digitalRead(buttonPins[button]);

    if (reading != lastButtonState[button]) {
      lastDebounceTime[button] = millis();
    }

    if ((millis() - lastDebounceTime[button]) > DEBOUNCE_DELAY_MS) {
      if (reading == LOW) {  // Pressed (assuming pull-up)
        lastButtonState[button] = reading;
        return true;
      }
    }

    lastButtonState[button] = reading;
    return false;
  }
};
```

**Rationale**: 50ms debounce prevents accidental double-presses while feeling responsive to user

---

## Display Rendering

### LCD Update Strategy

**Goal**: Minimize flicker, update only when state or selection changes

**Implementation**:
```cpp
class DisplayManager {
private:
  TournamentState lastRenderedState;
  uint8_t lastCursorLine;
  uint8_t lastSelectedAction;

public:
  void render(TournamentState currentState) {
    bool needsFullRedraw = (currentState != lastRenderedState);

    if (needsFullRedraw) {
      lcd.clear();
      renderState(currentState);
      lastRenderedState = currentState;
    } else {
      // Partial update - only changed elements
      updateCursor();
      updateSelections();
    }
  }
};
```

**Full Redraw Triggers**:
- State change (SPLASH → CONFIG_MENU → PFEILE_HOLEN → SCHIESS_BETRIEB)
- Error message display

**Partial Update Triggers**:
- Cursor movement (CONFIG_MENU)
- Selection change (Left/Right button in CONFIG_MENU or PFEILE_HOLEN)

---

## Error Handling

### Radio Transmission Errors

**Scenario**: `sendCommand()` returns `TX_TIMEOUT` or `TX_ERROR`

**Behavior**:
1. Display error message on LCD (3 seconds)
2. Play error beep (500 Hz, 100ms)
3. Remain in current state (do NOT transition)
4. User can retry by pressing button again

**Error Messages**:
```
"Fehler: Empfänger?"  // TX_TIMEOUT - receiver not responding
"Funk-Fehler!"        // TX_ERROR - hardware issue
```

**Example**:
```cpp
if (sendCommand(CMD_START_120) == TX_TIMEOUT) {
  showError("Fehler: Empfänger?");
  // Stay in PFEILE_HOLEN, allow user to retry "Nächste Passe"
} else {
  setState(STATE_SCHIESS_BETRIEB);
}
```

### Invalid State Transitions

**Scenario**: Button press in unexpected state (should never happen with correct implementation)

**Behavior**:
1. Log error to Serial (if debugging enabled)
2. Ignore button press
3. Do NOT change state

**Example**:
```cpp
void handleLeftButton() {
  switch (currentState) {
    case STATE_SPLASH:
    case STATE_SCHIESS_BETRIEB:
      // Left button has no function in these states
      return;  // Ignore

    case STATE_CONFIG_MENU:
    case STATE_PFEILE_HOLEN:
      // Normal handling
      ...
      break;

    default:
      Serial.println("ERROR: Invalid state");
      break;
  }
}
```

---

## State Persistence

**EEPROM Persistence** (only TournamentConfig):
- `shootingTime` and `shooterCount` written on "Start" button press
- Loaded on power-up and "Neustart"

**RAM-Only State** (lost on power cycle):
- `currentState` - always starts at SPLASH
- `shooterOrder` - resets to default 1-2-3-...
- UI selections (cursorLine, selectedAction, etc.)

**Rationale**: Configuration persistence improves UX, but tournament state should reset on power-up for safety

---

## Timing Constraints

| Operation | Max Duration | Rationale |
|-----------|--------------|-----------|
| Button response (UI update) | 100ms | Spec: Button response time < 100ms |
| State transition (with radio) | 1000ms | Spec: State transition time < 1 second |
| Display update (LCD I2C) | 50ms | Measured: I2C write ~1-5ms per operation |
| EEPROM write | 10ms | Measured: ~3.3ms, added safety margin |
| Splash screen duration | 3000ms | Spec: Splash screen 3 seconds |

**Loop Frequency**: Main `loop()` should run at > 20 Hz (< 50ms per iteration) to ensure responsive UI

---

## Testing & Validation

### State Machine Test Cases

1. **Full navigation path**: SPLASH → CONFIG_MENU → PFEILE_HOLEN → SCHIESS_BETRIEB → PFEILE_HOLEN → CONFIG_MENU
2. **Config menu navigation**: Test all Left/Right/OK combinations on each line
3. **Neustart retention**: Verify config values retained after "Neustart"
4. **EEPROM persistence**: Power cycle, verify config loaded correctly
5. **Alarm from each state**: Trigger alarm in all 4 states, verify return
6. **Error handling**: Simulate radio failures, verify UI remains responsive
7. **Button debouncing**: Rapidly press buttons, verify single trigger per press

### Acceptance Criteria

- ✅ All state transitions occur within 1 second
- ✅ Button presses acknowledged within 100ms (UI update visible)
- ✅ No state transition on debounce glitches
- ✅ Alarm accessible from all states (except SPLASH)
- ✅ Configuration persists across power cycles
- ✅ Error states allow retry without reset

---

## State Machine Implementation Template

```cpp
class StateMachine {
private:
  TournamentState currentState;
  TournamentState previousState;
  unsigned long stateEnterTime;

public:
  void setState(TournamentState newState) {
    previousState = currentState;
    currentState = newState;
    stateEnterTime = millis();

    // State entry actions
    onStateEnter(newState);
  }

  void update() {
    // Auto-transitions (e.g., SPLASH timeout)
    checkAutoTransitions();

    // State-specific update logic
    switch (currentState) {
      case STATE_SPLASH:
        updateSplash();
        break;
      case STATE_CONFIG_MENU:
        updateConfigMenu();
        break;
      case STATE_PFEILE_HOLEN:
        updatePfeileHolen();
        break;
      case STATE_SCHIESS_BETRIEB:
        updateSchiessBetrieb();
        break;
    }
  }

  void handleButtons(bool left, bool right, bool ok) {
    // Global alarm check first
    if (alarmDetector.isAlarmTriggered()) {
      handleAlarm();
      return;
    }

    // State-specific button handling
    switch (currentState) {
      case STATE_SPLASH:
        // No button handling during splash
        break;
      case STATE_CONFIG_MENU:
        handleConfigMenuButtons(left, right, ok);
        break;
      case STATE_PFEILE_HOLEN:
        handlePfeileHolenButtons(left, right, ok);
        break;
      case STATE_SCHIESS_BETRIEB:
        handleSchiessBetriebButtons(left, right, ok);
        break;
    }
  }
};
```

---

## Change Log

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-13 | Initial state machine contract |
