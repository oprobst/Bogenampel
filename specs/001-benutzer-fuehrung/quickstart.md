# Quickstart Guide: User Guidance Implementation

**Feature**: 001-benutzer-fuehrung (Archery Tournament Control System - User Guidance)
**Audience**: Developers implementing this feature
**Estimated Effort**: 12-16 hours of development + 4-6 hours testing

## Prerequisites

Before starting implementation, ensure you have:

1. ✅ **Hardware**:
   - 2x Arduino Nano (ATmega328P)
   - 2x nRF24L01+ radio modules
   - 1x 16x2 LCD with I2C interface (NEW - to be added to Sender)
   - 3x tactile buttons (Left, Right, OK) - NEW for Sender
   - Development environment (Arduino IDE or PlatformIO)

2. ✅ **Software**:
   - Arduino IDE 1.8+ or PlatformIO
   - Libraries: RF24, LiquidCrystal_I2C, EEPROM (built-in)
   - Git access to repository

3. ✅ **Documentation Read**:
   - [spec.md](spec.md) - Feature requirements
   - [research.md](research.md) - Design decisions
   - [data-model.md](data-model.md) - Data structures
   - [contracts/radio-protocol.md](contracts/radio-protocol.md) - Communication protocol
   - [contracts/state-machine.md](contracts/state-machine.md) - UI state flow

4. ✅ **KiCad Schematic Access**:
   - `/Schaltung/Schaltplan.pdf` - Authoritative pin assignments
   - Verify LCD I2C pins (A4=SDA, A5=SCL) available on Sender

---

## Implementation Roadmap

### Phase 1: Hardware Setup & Pin Assignment (2 hours)

**Task 1.1: Update KiCad Schematic**
1. Open `/Schaltung/` KiCad project
2. Add LCD connector (4 pins: VCC, GND, SDA, SCL)
3. Add 3 button connectors (J_LEFT, J_RIGHT, J_OK)
4. Document pin assignments:
   - LCD I2C: A4 (SDA), A5 (SCL)
   - Button Left: D2 (with pull-up)
   - Button Right: D3 (with pull-up)
   - Button OK: D4 (with pull-up)
5. Export updated PDF: `/Schaltung/Schaltplan.pdf`
6. Update `/Schaltung/README.md` with changes

**Task 1.2: Update Sender/Config.h**
```cpp
// Add to Sender/Config.h

// === LCD Display (NEW) ===
#define LCD_I2C_ADDR 0x27  // Or 0x3F, check your module
#define LCD_COLS 16
#define LCD_ROWS 2

// === Button Pins (NEW) ===
#define PIN_BTN_LEFT  2   // J_LEFT connector (see Schaltplan.pdf)
#define PIN_BTN_RIGHT 3   // J_RIGHT connector
#define PIN_BTN_OK    4   // J_OK connector

// === EEPROM Addresses (NEW) ===
#define EEPROM_CONFIG_ADDR 0  // TournamentConfig storage

// === Timing Constants (NEW) ===
#define SPLASH_DURATION_MS 3000
#define DEBOUNCE_DELAY_MS 50
#define ALARM_THRESHOLD_MS 3000
#define COMMAND_TIMEOUT_MS 500
```

**Validation**:
- ✅ Pins don't conflict with existing nRF24 pins (verify against constitution.md)
- ✅ Schematic updated and committed
- ✅ Config.h references schematic connectors in comments

---

### Phase 2: Data Structures & Commands (1 hour)

**Task 2.1: Create/Update Commands.h (Shared)**

Option A: Create `/Common/Commands.h` (recommended):
```cpp
// Common/Commands.h - SHARED between Sender and Empfänger
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

// Radio command codes
enum RadioCommand : uint8_t {
  CMD_STOP = 0x01,
  CMD_START_120 = 0x02,
  CMD_START_240 = 0x03,
  CMD_INIT = 0x04,
  CMD_ALARM = 0x05
};

// Packet structure
#pragma pack(push, 1)
struct RadioPacket {
  uint8_t command;
  uint8_t checksum;
};
#pragma pack(pop)

// Checksum functions
inline uint8_t calculateChecksum(uint8_t command) {
  return command ^ 0xFF;
}

inline bool validateChecksum(RadioPacket* packet) {
  return (packet->checksum == (packet->command ^ 0xFF));
}

// Compile-time size check
static_assert(sizeof(RadioPacket) == 2, "RadioPacket must be exactly 2 bytes");

#endif // COMMANDS_H
```

Option B: Ensure `Sender/Commands.h` and `Empfänger/Commands.h` are IDENTICAL

**Task 2.2: Add TournamentConfig Structure to Sender/Config.h**
```cpp
// Add to Sender/Config.h

// Tournament configuration (EEPROM-backed)
struct TournamentConfig {
  uint8_t shootingTime;   // 120 or 240
  uint8_t shooterCount;   // 2 (1-2 shooters) or 4 (3-4 shooters)
  uint8_t checksum;       // CRC8 validation
};

enum ShootingTime {
  TIME_120_SEC = 120,
  TIME_240_SEC = 240
};

enum ShooterCount {
  SHOOTERS_1_2 = 2,
  SHOOTERS_3_4 = 4
};
```

**Validation**:
- ✅ `sizeof(RadioPacket) == 2` assertion passes
- ✅ Commands.h identical between projects (or symlinked)

---

### Phase 3: Core Components (6-8 hours)

**Task 3.1: Extend ButtonManager (2 hours)**

File: `Sender/ButtonManager.h` and `Sender/ButtonManager.cpp`

Add:
```cpp
class ButtonManager {
private:
  uint8_t lastButtonState[3];        // LEFT, RIGHT, OK
  unsigned long lastDebounceTime[3];
  unsigned long okPressStartTime;
  bool okPressActive;
  bool alarmTriggered;

public:
  void init();
  bool isLeftPressed();
  bool isRightPressed();
  bool isOKPressed();
  bool isAlarmTriggered();  // Returns true once when alarm detected
  void update();             // Call in loop()
};
```

Implementation guide: See [contracts/state-machine.md](contracts/state-machine.md) § Button Debouncing

**Task 3.2: Extend StateMachine (2-3 hours)**

File: `Sender/StateMachine.h` and `Sender/StateMachine.cpp`

Add new states:
```cpp
enum TournamentState {
  STATE_SPLASH,
  STATE_CONFIG_MENU,
  STATE_PFEILE_HOLEN,
  STATE_SCHIESS_BETRIEB
};

class StateMachine {
private:
  TournamentState currentState;
  TournamentState previousState;
  unsigned long stateEnterTime;

public:
  void setState(TournamentState newState);
  TournamentState getState() const;
  TournamentState getPreviousState() const;
  bool isInTournamentMode() const;
  unsigned long getStateEnterTime() const;
  void update();  // Check auto-transitions (splash timeout)
};
```

Implementation guide: See [contracts/state-machine.md](contracts/state-machine.md) § State Machine Implementation Template

**Task 3.3: Create ConfigMenu Module (2-3 hours)**

Files: `Sender/ConfigMenu.h` and `Sender/ConfigMenu.cpp` (NEW)

```cpp
class ConfigMenu {
private:
  uint8_t cursorLine;        // 0, 1, or 2
  uint8_t selectedTime;      // 120 or 240
  uint8_t selectedCount;     // 2 or 4
  uint8_t selectedButton;    // 0 = Ändern, 1 = Start

public:
  void init(TournamentConfig config);
  void handleLeftButton();
  void handleRightButton();
  void handleOKButton(StateMachine& sm);
  void render(LiquidCrystal_I2C& lcd);
  TournamentConfig getConfig();
};
```

Implementation guide: See [contracts/state-machine.md](contracts/state-machine.md) § STATE_CONFIG_MENU

**Task 3.4: Create TournamentMode Module (2-3 hours)**

Files: `Sender/TournamentMode.h` and `Sender/TournamentMode.cpp` (NEW)

```cpp
class TournamentMode {
private:
  uint8_t selectedAction;     // Button index
  ShooterOrder shooterOrder;
  TournamentConfig config;

public:
  void init(TournamentConfig config);
  void handleButtons(bool left, bool right, bool ok,
                     StateMachine& sm, TournamentState currentState);
  void render(LiquidCrystal_I2C& lcd, TournamentState currentState);
};
```

Implementation guide: See [contracts/state-machine.md](contracts/state-machine.md) § STATE_PFEILE_HOLEN and STATE_SCHIESS_BETRIEB

**Validation**:
- ✅ All button presses trigger correct state transitions
- ✅ LCD displays render correctly
- ✅ Cursor navigation works as specified

---

### Phase 4: Radio Communication (2 hours)

**Task 4.1: Implement Radio Command Functions**

Add to `Sender/Sender.ino` or create `Sender/RadioManager.cpp`:

```cpp
enum TransmissionResult {
  TX_SUCCESS,
  TX_TIMEOUT,
  TX_ERROR
};

TransmissionResult sendCommand(RadioCommand cmd) {
  RadioPacket packet;
  packet.command = static_cast<uint8_t>(cmd);
  packet.checksum = calculateChecksum(packet.command);

  bool success = radio.write(&packet, sizeof(packet));

  if (success) {
    return TX_SUCCESS;
  } else if (!radio.isChipConnected()) {
    return TX_ERROR;
  } else {
    return TX_TIMEOUT;
  }
}

TransmissionResult sendAlarmWithRetry() {
  for (int attempt = 0; attempt < 3; attempt++) {
    TransmissionResult result = sendCommand(CMD_ALARM);
    if (result == TX_SUCCESS) return TX_SUCCESS;
    if (attempt < 2) delay(200);
  }
  return TX_TIMEOUT;
}
```

**Task 4.2: Update Receiver Command Handler**

File: `Empfaenger/Empfaenger.ino` (extend existing)

Add:
```cpp
void handleCommand(RadioCommand cmd) {
  switch(cmd) {
    case CMD_STOP:
      stopTimer();
      setLEDsRed();
      playStopTone();
      break;

    case CMD_START_120:
      startTimer(120);
      break;

    case CMD_START_240:
      startTimer(240);
      break;

    case CMD_INIT:
      resetReceiver();
      flashStatusLED(3);
      break;

    case CMD_ALARM:
      triggerAlarm();
      break;
  }
}
```

Implementation guide: See [contracts/radio-protocol.md](contracts/radio-protocol.md) § Command Semantics

**Validation**:
- ✅ Each command triggers correct receiver behavior
- ✅ Sender receives ACK within 500ms
- ✅ Timeout detected and displayed on sender

---

### Phase 5: EEPROM Persistence (1 hour)

**Task 5.1: Implement Config Load/Save**

Add to `Sender/Sender.ino` or create `Sender/ConfigManager.cpp`:

```cpp
#include <EEPROM.h>

uint8_t calculateConfigChecksum(TournamentConfig* config) {
  return config->shootingTime ^ config->shooterCount;
}

void saveConfigToEEPROM(TournamentConfig config) {
  config.checksum = calculateConfigChecksum(&config);
  EEPROM.put(EEPROM_CONFIG_ADDR, config);
}

TournamentConfig loadConfigFromEEPROM() {
  TournamentConfig config;
  EEPROM.get(EEPROM_CONFIG_ADDR, config);

  // Validate
  if (calculateConfigChecksum(&config) == config.checksum) {
    return config;
  } else {
    // Return defaults
    config.shootingTime = TIME_120_SEC;
    config.shooterCount = SHOOTERS_1_2;
    return config;
  }
}
```

**Call Points**:
- Load on `setup()` → initialize ConfigMenu
- Save on "Start" button press in CONFIG_MENU

**Validation**:
- ✅ Power cycle preserves configuration
- ✅ Corrupted EEPROM falls back to defaults
- ✅ "Neustart" retains current values

---

### Phase 6: Integration & Main Loop (2 hours)

**Task 6.1: Update Sender.ino Main Sketch**

```cpp
#include <RF24.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include "Config.h"
#include "Commands.h"
#include "ButtonManager.h"
#include "StateMachine.h"
#include "ConfigMenu.h"
#include "TournamentMode.h"
#include "SplashScreen.h"

// Global objects
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
ButtonManager buttons;
StateMachine stateMachine;
ConfigMenu configMenu;
TournamentMode tournamentMode;

void setup() {
  // Initialize hardware
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  buttons.init();
  radio.begin();
  setupRadio();

  // Load config from EEPROM
  TournamentConfig config = loadConfigFromEEPROM();

  // Initialize modules
  configMenu.init(config);
  tournamentMode.init(config);
  stateMachine.setState(STATE_SPLASH);
}

void loop() {
  // Update button manager (debouncing + alarm detection)
  buttons.update();

  // Global alarm check
  if (buttons.isAlarmTriggered()) {
    handleAlarm();
  }

  // Update state machine (auto-transitions)
  stateMachine.update();

  // State-specific updates
  TournamentState currentState = stateMachine.getState();

  switch (currentState) {
    case STATE_SPLASH:
      SplashScreen::render(lcd);
      // Auto-transition handled in stateMachine.update()
      break;

    case STATE_CONFIG_MENU:
      if (buttons.isLeftPressed()) configMenu.handleLeftButton();
      if (buttons.isRightPressed()) configMenu.handleRightButton();
      if (buttons.isOKPressed()) configMenu.handleOKButton(stateMachine);
      configMenu.render(lcd);
      break;

    case STATE_PFEILE_HOLEN:
    case STATE_SCHIESS_BETRIEB:
      tournamentMode.handleButtons(
        buttons.isLeftPressed(),
        buttons.isRightPressed(),
        buttons.isOKPressed(),
        stateMachine,
        currentState
      );
      tournamentMode.render(lcd, currentState);
      break;
  }

  delay(10);  // Loop frequency ~100 Hz
}

void handleAlarm() {
  TransmissionResult result = sendAlarmWithRetry();
  if (result == TX_SUCCESS) {
    showAlarmConfirmation();
  } else {
    showError("Alarm Fehler!");
  }
}
```

**Validation**:
- ✅ Main loop runs at > 20 Hz (< 50ms per iteration)
- ✅ All states reachable via button navigation
- ✅ No blocking delays in loop

---

## Testing Checklist

### Unit Tests (Software)

- [ ] Button debouncing (rapid presses → single trigger)
- [ ] Alarm detection (2.9s press = no alarm, 3.1s = alarm)
- [ ] EEPROM checksum validation (corrupt data → defaults)
- [ ] Radio packet checksum (corrupt packet → discarded)
- [ ] State transitions (all paths in state diagram)

### Hardware-in-the-Loop Tests

- [ ] LCD display rendering (all screens readable)
- [ ] Button navigation (Left/Right/OK in all states)
- [ ] Radio communication (send each command, verify receiver)
- [ ] EEPROM persistence (save, power cycle, verify load)
- [ ] Alarm trigger (hold OK 3s from each state)

### Integration Tests

- [ ] Full tournament workflow: Power on → Configure → Start → Shoot → Stop → Neustart
- [ ] Configuration persistence: Change settings → Start → Neustart → Verify retained
- [ ] Error recovery: Disconnect receiver → Press buttons → Display errors → Reconnect → Retry
- [ ] Radio range: Test at 10m, 25m, 50m (target: 99% success at 50m)

### Acceptance Criteria (from spec.md)

- [ ] **SC-001**: Configuration in < 30 seconds
- [ ] **SC-002**: State transitions < 1 second
- [ ] **SC-003**: Radio commands < 500ms latency
- [ ] **SC-004**: 95% users navigate config menu on first try
- [ ] **SC-005**: Alarm triggerable < 4 seconds
- [ ] **SC-006**: 100 consecutive state transitions without errors
- [ ] **SC-007**: Display updates < 200ms
- [ ] **SC-008**: Radio reliable at 50 meters

---

## Common Pitfalls & Tips

### 🚨 Pin Conflicts

**Problem**: I2C (A4/A5) conflicts with existing pins
**Solution**: Check `/Schaltung/Schaltplan.pdf` before assigning, verify against constitution.md

### 🚨 EEPROM Wear

**Problem**: Writing to EEPROM on every button press
**Solution**: Only write on "Start" button press (see research.md § 3)

### 🚨 Radio Packet Size

**Problem**: RadioPacket > 32 bytes (nRF24 max payload)
**Solution**: Use `static_assert(sizeof(RadioPacket) == 2)` to enforce

### 🚨 LCD Flicker

**Problem**: Calling `lcd.clear()` every loop iteration
**Solution**: Only clear on state change, use partial updates (see state-machine.md § Display Rendering)

### 🚨 Button Bouncing

**Problem**: Single press triggers multiple actions
**Solution**: Implement 50ms debounce delay (see state-machine.md § Button Debouncing)

### 🚨 Blocking Delays

**Problem**: `delay(3000)` in main loop freezes UI
**Solution**: Use `millis()` timestamps for splash screen timeout

### 💡 Debugging Tips

1. **Serial Logging**: Add `Serial.println()` at state transitions
2. **LED Indicators**: Use status LEDs to show button presses
3. **Radio Monitor**: Print transmitted commands to Serial
4. **EEPROM Dump**: Print loaded config on startup to verify persistence

---

## Library Installation

### Arduino IDE

1. Open Library Manager (Sketch → Include Library → Manage Libraries)
2. Install:
   - `RF24` by TMRh20
   - `LiquidCrystal I2C` by Frank de Brabander

### PlatformIO

Add to `platformio.ini`:
```ini
[env:nanoatmega328]
platform = atmelavr
board = nanoatmega328
framework = arduino
lib_deps =
    nrf24/RF24@^1.4.9
    marcoschwartz/LiquidCrystal_I2C@^1.1.4
```

---

## Git Workflow

1. Create feature branch: `git checkout 001-benutzer-fuehrung` (already exists)
2. Commit incrementally:
   - "Add LCD and button pin definitions"
   - "Implement ConfigMenu class"
   - "Extend StateMachine for tournament modes"
   - "Add EEPROM persistence"
3. Test before merging
4. PR to `main` with test results and photos/videos

---

## Next Steps After Implementation

1. Run `/speckit.tasks` to generate detailed task breakdown
2. Implement tasks in priority order (P1 → P2 → P3)
3. Update `/Schaltung/` schematic with LCD connector
4. Update constitution.md with LCD pin assignments
5. Write user manual (how to operate tournament system)

---

## Questions? Issues?

- **Hardware questions**: Check `/Schaltung/Schaltplan.pdf` and constitution.md
- **Protocol questions**: See [contracts/radio-protocol.md](contracts/radio-protocol.md)
- **State machine questions**: See [contracts/state-machine.md](contracts/state-machine.md)
- **Design rationale**: See [research.md](research.md)

**Ready to start? Begin with Phase 1 (Hardware Setup)!** 🚀
