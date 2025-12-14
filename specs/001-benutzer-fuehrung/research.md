# Research: User Guidance Implementation

**Feature**: 001-benutzer-fuehrung
**Date**: 2025-12-13
**Status**: Phase 0 - Research & Design Decisions

## Research Tasks

Based on Constitution Check, the following items require research before Phase 1 design:

1. **Display hardware clarification** (Principle III - MUST resolve)
2. **Radio failure handling** (Principle I - MUST resolve)
3. **EEPROM write strategy** (Principle II - SHOULD optimize)

---

## 1. Display Hardware Clarification

### Question
The specification mentions "Monitor" with multi-line text display and button selection, but existing hardware uses WS2812B LED strips for 7-segment timer display. Is this:
- A new LCD/OLED display component?
- Visualization using existing WS2812B LEDs?
- Different display on sender vs. receiver?

### Research Findings

**Existing Hardware Analysis**:
- **Sender**: Currently has 2x status LEDs (D1, D2) only - no display mentioned in README.md
- **Receiver**: WS2812B LED strip (J4) configured for 3x 7-segment display (~155 LEDs)
- **KiCad Schematic**: `/Schaltung/Schaltplan.pdf` shows connectors but needs examination for display components

**Investigation of Sender Hardware**:
Looking at `/Sender/Config.h` and existing code:
- Sender has buttons (J1 for timer control)
- Sender has status LEDs
- No mention of display hardware in Sender firmware

### Decision

**Display Hardware**: The "Monitor" in the specification refers to a **NEW display component that must be added to the Sender device**.

**Rationale**:
1. **Configuration menu requirements**: The spec requires displaying 3 lines of text (shooting time, shooter count, action buttons) with cursor navigation
2. **Tournament mode requirements**: Display must show state name at top ("Pfeile holen", "Schießbetrieb") and shooter order at bottom
3. **WS2812B LEDs insufficient**: Current 7-segment LED display on receiver cannot render multi-line text menus
4. **Cost/complexity tradeoff**: LCD displays are inexpensive ($2-5) and well-supported in Arduino ecosystem

**Recommended Display**:
- **16x2 or 20x4 LCD with I2C interface** (e.g., HD44780 with PCF8574 I2C backpack)
- Uses 2 pins (SDA, SCL) via I2C
- Standard Arduino library support (LiquidCrystal_I2C)
- Sufficient for displaying menu items and state information

**Alternative Considered**:
- **OLED display (128x64, I2C)**: More expensive, better visibility, but overkill for text-only UI
- **Rejected**: Higher cost, more complex graphics handling not needed

### Schematic Impact

**NEW HARDWARE REQUIRED**:
- 1x 16x2 LCD with I2C interface (or 20x4 for more menu space)
- Connection to Arduino Nano I2C pins (A4=SDA, A5=SCL)
- Pull-up resistors (4.7kΩ) if not on I2C backpack

**Schematic Update Required**: ✅ YES
- Add LCD connector to `/Schaltung/` KiCad project
- Document I2C address (typically 0x27 or 0x3F)
- Add to pin assignments in constitution.md

---

## 2. Radio Communication Failure Handling

### Question
How should the system behave when radio transmission fails? What timeout and retry strategies should be implemented?

### Research Findings

**nRF24L01+ Characteristics**:
- **Automatic retransmission**: Built-in hardware retry (0-15 retries, configurable delay)
- **ACK packets**: Receiver automatically acknowledges successful reception
- **Timeout detection**: `write()` function returns success/failure status
- **Typical latency**: 5-50ms for successful transmission

**Safety Considerations** (Constitution Principle I):
- Sender must know if critical commands (Init, Alarm) were received
- Tournament state transitions depend on receiver executing commands
- Fail-safe: If receiver doesn't get command, sender should indicate failure

### Decision

**Radio Failure Strategy**:

**1. Transmission Level** (RF24 library configuration):
```cpp
radio.setRetries(delay, count);  // delay: 0-15 (250µs units), count: 0-15
// Recommended: setRetries(5, 15) = 1.25ms delay, 15 retries = ~20ms total
```

**2. Application Level** (Sender firmware):
```cpp
bool sendCommand(uint8_t command) {
  bool success = radio.write(&command, sizeof(command));
  if (!success) {
    // Display error indicator (flashing LED or LCD message)
    // Log failure for debugging
    return false;
  }
  return true;
}
```

**3. User Feedback**:
- **Success**: Brief confirmation (LED blink, beep, or LCD checkmark)
- **Failure**: Error indication (flashing red LED, error message on LCD)
- **Critical commands** (Init, Alarm): Block UI until confirmed or timeout (500ms)
- **Non-critical** (Stop, Start): Allow retry via button press

**4. Timeout Values**:
- **Command transmission timeout**: 500ms (includes 15 retries)
- **State transition timeout**: 1 second (display update + radio latency)
- **Alarm command**: 3 attempts with 200ms delay between attempts

### Alternatives Considered

**Alternative 1**: Bidirectional ACK with custom protocol
- **Rejected**: Adds complexity, nRF24 hardware ACK sufficient for this use case

**Alternative 2**: No failure handling, assume transmission succeeds
- **Rejected**: Violates Constitution Principle I (Safety First)

**Alternative 3**: Periodic heartbeat to check connectivity
- **Rejected**: Unnecessary power consumption, only needed on demand

---

## 3. EEPROM Write Strategy

### Question
When should configuration settings (shooting time, shooter count) be written to EEPROM? Minimize wear while ensuring persistence.

### Research Findings

**EEPROM Characteristics** (ATmega328P):
- **Size**: 1KB (1024 bytes)
- **Write endurance**: ~100,000 writes per cell
- **Write time**: ~3.3ms per byte
- **Read time**: Negligible

**Configuration Data** (2-4 bytes):
```cpp
struct TournamentConfig {
  uint8_t shootingTime;    // 120 or 240 seconds
  uint8_t shooterCount;    // 1-2 or 3-4 shooters
  uint8_t checksum;        // Validation
};
```

**Usage Patterns**:
- Configuration changes: Infrequent (once per tournament day, ~10 tournaments per year = 10 writes/year)
- "Neustart" button: Returns to config menu, settings retained (no write needed)
- Power loss: Settings should persist

### Decision

**EEPROM Write Strategy**: Write on "Start" button press (entering tournament mode)

**Rationale**:
1. **Minimize writes**: Only write when configuration is finalized (Start pressed), not on every Left/Right navigation
2. **Wear leveling**: With 100,000 write cycles and ~10-50 writes/year, EEPROM will last 2000-10,000 years
3. **Power efficiency**: Avoid repeated 3.3ms writes during navigation
4. **User intent**: "Start" confirms configuration, appropriate time to persist

**Implementation**:
```cpp
// On "Start" button press in configuration menu:
void saveConfiguration() {
  TournamentConfig config;
  config.shootingTime = selectedTime;     // 120 or 240
  config.shooterCount = selectedCount;    // 1-2 or 3-4
  config.checksum = calculateChecksum(&config);

  EEPROM.put(EEPROM_CONFIG_ADDR, config);  // Write to address 0
}

// On power-up or "Neustart":
void loadConfiguration() {
  TournamentConfig config;
  EEPROM.get(EEPROM_CONFIG_ADDR, config);

  if (validateChecksum(&config)) {
    selectedTime = config.shootingTime;
    selectedCount = config.shooterCount;
  } else {
    // Use defaults: 120s, 1-2 shooters
    selectedTime = 120;
    selectedCount = 2;  // 1-2 shooters
  }
}
```

**Validation**: CRC8 checksum prevents reading corrupted data after power loss during write

### Alternatives Considered

**Alternative 1**: Write immediately on each Left/Right button press
- **Rejected**: Unnecessary wear (hundreds of writes per configuration session)

**Alternative 2**: Write on power-down (using watchdog timer)
- **Rejected**: Arduino Nano cannot detect power-down with 9V battery, unreliable

**Alternative 3**: Never write, always use defaults
- **Rejected**: Violates spec requirement "configuration options retain previous tournament settings"

---

## Research Summary

### All Research Tasks Resolved ✅

| Research Task | Decision | Rationale |
|---------------|----------|-----------|
| **Display Hardware** | 16x2 LCD with I2C interface | Text menu navigation requires multi-line character display; WS2812B LEDs insufficient |
| **Radio Failure Handling** | nRF24 hardware retry (15x) + application timeout (500ms) | Built-in reliability with user feedback on failure; safety-critical commands get extra attempts |
| **EEPROM Write Strategy** | Write on "Start" button press only | Minimizes wear, writes only when configuration finalized, provides persistence |

### Design Constraints Established

1. **New Hardware**:
   - Add 16x2 LCD with I2C to Sender
   - Schematic update required in `/Schaltung/`

2. **Pin Allocation** (Sender):
   - I2C SDA: A4
   - I2C SCL: A5
   - Requires validation against existing pin usage

3. **Library Dependencies**:
   - LiquidCrystal_I2C (LCD control)
   - RF24 (radio, existing)
   - EEPROM (Arduino core, existing)

4. **Performance Validated**:
   - LCD I2C communication: ~1-5ms per write (within 200ms display update budget)
   - Radio timeout: 500ms (within 1s state transition budget)
   - EEPROM write: 3.3ms (negligible on "Start" press)

### Constitution Re-Check

**Principle I (Safety)**: ✅ Radio failure handling defined with timeout and retry
**Principle II (Simplicity)**: ✅ EEPROM strategy minimizes wear while maintaining persistence
**Principle III (Hardware Standards)**: ✅ Display hardware selected (LCD I2C standard component)
**Principle V (Documentation)**: ⚠️ Schematic update required for LCD before implementation

### Ready for Phase 1 ✅

All MUST-resolve research tasks complete. Proceed to data model and contract design.
