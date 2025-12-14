# Radio Protocol Contract: Sender ↔ Receiver Communication

**Feature**: 001-benutzer-fuehrung
**Date**: 2025-12-13
**Protocol Version**: 1.0

## Overview

This document defines the radio communication protocol between the Sender (tournament control) and Receiver (timer display) devices using nRF24L01+ modules.

**Key Principles**:
- **Unidirectional**: Sender → Receiver only (Receiver uses hardware ACK for confirmation)
- **Command-based**: Five distinct commands with specific semantics
- **Validated**: Simple checksum prevents corrupted command execution
- **Reliable**: Hardware retransmission with timeout detection

---

## Hardware Configuration

### nRF24L01+ Settings

**Both Sender and Receiver**:
```cpp
// Common settings (must match on both devices)
#define RF24_CHANNEL 76          // 2.476 GHz (avoid WiFi interference)
#define RF24_PA_LEVEL RF24_PA_HIGH  // Maximum power for range
#define RF24_DATA_RATE RF24_250KBPS  // Lowest rate = best range

// Pipe addresses (must match)
const uint64_t PIPE_ADDRESS = 0xE8E8F0F0E1LL;  // Unique identifier

// Auto-retransmit settings (Sender only)
#define RF24_RETRY_DELAY 5      // 1.25ms delay between retries
#define RF24_RETRY_COUNT 15     // 15 retries before giving up
```

**Setup Code** (Sender):
```cpp
void setupRadio() {
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(RF24_CHANNEL);
  radio.setRetries(RF24_RETRY_DELAY, RF24_RETRY_COUNT);
  radio.enableAckPayload();  // Use hardware ACK
  radio.openWritingPipe(PIPE_ADDRESS);
  radio.stopListening();  // Sender = TX mode
}
```

**Setup Code** (Receiver):
```cpp
void setupRadio() {
  radio.begin();
  radio.setPALevel(RF24_PA_HIGH);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(RF24_CHANNEL);
  radio.openReadingPipe(1, PIPE_ADDRESS);
  radio.startListening();  // Receiver = RX mode
}
```

---

## Packet Format

### RadioPacket Structure

```cpp
// Defined in Commands.h (MUST be identical on Sender and Receiver)
#pragma pack(push, 1)  // Ensure no padding
struct RadioPacket {
  uint8_t command;    // Command code (see RadioCommand enum)
  uint8_t checksum;   // XOR checksum for validation
};
#pragma pack(pop)

// Size: 2 bytes (fits in single nRF24 packet, max 32 bytes)
```

### Command Codes

```cpp
enum RadioCommand : uint8_t {
  CMD_STOP = 0x01,       // Stop timer, red light
  CMD_START_120 = 0x02,  // Start 120-second countdown
  CMD_START_240 = 0x03,  // Start 240-second countdown
  CMD_INIT = 0x04,       // Initialize receiver (tournament start)
  CMD_ALARM = 0x05       // Emergency alarm
};
```

### Checksum Calculation

**Simple XOR**:
```cpp
uint8_t calculateChecksum(uint8_t command) {
  return command ^ 0xFF;
}

bool validateChecksum(RadioPacket* packet) {
  return (packet->checksum == (packet->command ^ 0xFF));
}
```

**Rationale**: XOR is fast, sufficient for detecting single-bit errors and simple corruption. Not cryptographically secure, but unnecessary for this application.

---

## Command Semantics

### CMD_STOP (0x01)

**Sender Context**: "Passe beenden" button pressed in SCHIESS_BETRIEB state

**Receiver Action**:
1. Stop active countdown timer
2. Set WS2812B LEDs to RED (all segments)
3. Display "000" on 7-segment timer
4. Play "timer stopped" buzzer tone (2000 Hz, 200ms)
5. Enter IDLE state

**Expected Response**: Hardware ACK within 20ms

**Error Handling**: If no ACK, sender displays error and allows retry

---

### CMD_START_120 (0x02)

**Sender Context**: "Nächste Passe" button pressed in PFEILE_HOLEN state with 120s configuration

**Receiver Action**:
1. Start 10-second RED preparation phase (countdown 10→1)
2. Play "preparation start" buzzer tone (1500 Hz, 300ms)
3. After 10s: Transition to GREEN phase
4. Start 120-second countdown (120→1)
5. Play "shooting start" buzzer tone (2500 Hz, 300ms)
6. At 30s remaining: Transition to YELLOW (warning)
7. At 0s: Transition to RED, play "end" tone (2000 Hz, 500ms)

**Expected Response**: Hardware ACK within 20ms

**Error Handling**: If no ACK, sender displays error, stays in PFEILE_HOLEN

---

### CMD_START_240 (0x03)

**Sender Context**: "Nächste Passe" button pressed in PFEILE_HOLEN state with 240s configuration

**Receiver Action**:
Same as CMD_START_120 but with 240-second countdown instead of 120s

**Expected Response**: Hardware ACK within 20ms

---

### CMD_INIT (0x04)

**Sender Context**: "Start" button pressed in CONFIG_MENU (entering tournament mode)

**Receiver Action**:
1. Reset all state (stop any running timers)
2. Clear LED display (all OFF or RED)
3. Silence buzzer
4. Enter IDLE state, ready for CMD_START_120/240
5. Flash status LED 3 times to confirm initialization

**Expected Response**: Hardware ACK within 20ms

**Purpose**: Ensures receiver is in known state when tournament begins

---

### CMD_ALARM (0x05)

**Sender Context**: OK button held > 3 seconds in ANY state

**Receiver Action**:
1. Immediately stop all timers
2. Flash RED LEDs rapidly (10 Hz flashing)
3. Play continuous alarm buzzer tone (alternating 1000/2000 Hz)
4. Continue until CMD_STOP or CMD_INIT received
5. Display "ALARM" or flashing "---" on 7-segment

**Expected Response**: Hardware ACK within 20ms (critical command)

**Error Handling**: Sender retries 3 times with 200ms delay if no ACK

**Safety**: Highest priority command, must interrupt any ongoing operation

---

## Transmission Protocol

### Sender Transmission Function

```cpp
enum TransmissionResult {
  TX_SUCCESS,       // ACK received, command delivered
  TX_TIMEOUT,       // No ACK after retries
  TX_ERROR          // Radio hardware error
};

TransmissionResult sendCommand(RadioCommand cmd) {
  // Build packet
  RadioPacket packet;
  packet.command = static_cast<uint8_t>(cmd);
  packet.checksum = calculateChecksum(packet.command);

  // Attempt transmission
  bool success = radio.write(&packet, sizeof(packet));

  if (success) {
    return TX_SUCCESS;  // Hardware ACK received
  } else {
    // Check if radio is responsive
    if (!radio.isChipConnected()) {
      return TX_ERROR;  // Hardware failure
    }
    return TX_TIMEOUT;  // Receiver didn't ACK (out of range or off)
  }
}
```

### Receiver Reception Function

```cpp
bool receiveCommand(RadioCommand& cmd) {
  if (!radio.available()) {
    return false;  // No packet waiting
  }

  // Read packet
  RadioPacket packet;
  radio.read(&packet, sizeof(packet));

  // Validate checksum
  if (!validateChecksum(&packet)) {
    // Corrupted packet, ignore
    return false;
  }

  // Validate command code
  if (packet.command < CMD_STOP || packet.command > CMD_ALARM) {
    // Invalid command, ignore
    return false;
  }

  cmd = static_cast<RadioCommand>(packet.command);
  return true;
}
```

---

## Timing Requirements

### Latency Budget

| Phase | Time Budget | Rationale |
|-------|-------------|-----------|
| Sender button press → radio.write() | < 10ms | Minimize UI lag |
| radio.write() execution | < 20ms | 15 retries × 1.25ms + overhead |
| Radio propagation | < 1ms | Speed of light, negligible |
| Receiver processing | < 10ms | Command validation + action dispatch |
| **Total latency** | **< 50ms** | Within 500ms success criterion |

### Timeout Values

```cpp
#define COMMAND_TIMEOUT_MS 500      // Max time to wait for success
#define ALARM_RETRY_DELAY_MS 200    // Delay between alarm retry attempts
#define ALARM_MAX_RETRIES 3         // Extra retries for critical alarm command
```

### Sender Retry Logic (Alarm Command)

```cpp
TransmissionResult sendAlarmWithRetry() {
  for (int attempt = 0; attempt < ALARM_MAX_RETRIES; attempt++) {
    TransmissionResult result = sendCommand(CMD_ALARM);

    if (result == TX_SUCCESS) {
      return TX_SUCCESS;
    }

    if (attempt < ALARM_MAX_RETRIES - 1) {
      delay(ALARM_RETRY_DELAY_MS);  // Wait before retry
    }
  }

  return TX_TIMEOUT;  // All retries failed
}
```

---

## Error Handling

### Sender-Side Errors

| Error | Detection | User Feedback | Recovery |
|-------|-----------|---------------|----------|
| **TX_TIMEOUT** | `radio.write()` returns false | LCD: "Fehler: Empfänger?" | Allow manual retry via button |
| **TX_ERROR** | `!radio.isChipConnected()` | LCD: "Funk-Fehler!" | Requires power cycle or reconnect |
| **Invalid State** | Command sent in wrong state | LCD: "Interner Fehler" | Log error, reset state machine |

### Receiver-Side Errors

| Error | Detection | Action | Recovery |
|-------|-----------|--------|----------|
| **Checksum Fail** | `!validateChecksum()` | Discard packet, wait for retry | Sender auto-retries |
| **Invalid Command** | Command < 0x01 or > 0x05 | Discard packet, log to serial | Sender retries or user fixes |
| **Unexpected Command** | E.g., CMD_START while timer running | Ignore or handle gracefully | Depends on receiver state machine |

---

## Protocol Versioning

**Current Version**: 1.0

**Backward Compatibility**: Not required (both devices always updated together)

**Future Extensions** (reserved command codes):
- `0x06-0x0F`: Reserved for additional timer commands
- `0x10-0x1F`: Reserved for configuration commands
- `0x20-0xFF`: Reserved for future use

**Breaking Changes**: Increment major version, require synchronized firmware updates

---

## Testing Requirements

### Unit Tests

1. **Checksum validation**: Verify correct XOR calculation and validation
2. **Packet serialization**: Ensure RadioPacket is exactly 2 bytes (no padding)
3. **Command code validity**: Test enum range checks

### Hardware-in-the-Loop Tests

1. **Basic transmission**: Send each command, verify receiver executes correctly
2. **Range testing**: Test at 10m, 25m, 50m distances
3. **Interference**: Test with WiFi, Bluetooth active nearby
4. **Retry mechanism**: Simulate packet loss, verify retransmission works
5. **Timeout handling**: Power off receiver, verify sender detects timeout

### Acceptance Criteria

- ✅ Success rate > 99% at 50m line-of-sight
- ✅ Latency < 500ms for any command
- ✅ Alarm command succeeds within 1 second (3 retries)
- ✅ Invalid packets discarded without side effects
- ✅ No false command execution from corrupted data

---

## Security Considerations

**Threat Model**: Low security requirements (archery range environment)

**Addressed**:
- ✅ Checksum prevents accidental corruption execution
- ✅ Unique pipe address prevents interference from other nRF24 devices

**Not Addressed** (acceptable for this use case):
- ❌ No encryption (commands not sensitive)
- ❌ No authentication (physical access required, no remote threat)
- ❌ No replay protection (commands are stateless/idempotent)

**Future Enhancement** (if multi-range deployment): Add 16-bit random nonce to prevent cross-range interference

---

## Code Synchronization

**CRITICAL**: The following files MUST be IDENTICAL on Sender and Receiver:

1. **Commands.h**: Enum definitions, RadioPacket structure, checksum function
2. **RF24 Configuration**: Channel, PA level, data rate, pipe address

**Enforcement**:
- Use symbolic links or copy script to ensure synchronization
- Add compile-time size assertion: `static_assert(sizeof(RadioPacket) == 2)`
- Document in README: "WARNING: Commands.h must match between Sender and Empfänger"

**Recommended**: Create `Common/Commands.h` shared by both projects

---

## Appendix: Example Scenarios

### Scenario 1: Start 120s Tournament Round

1. User presses "Nächste Passe" on Sender (PFEILE_HOLEN state, 120s config)
2. Sender calls `sendCommand(CMD_START_120)`
3. RF24 transmits packet `{0x02, 0xFD}` (command + checksum)
4. Receiver validates checksum (0x02 XOR 0xFF = 0xFD ✓)
5. Receiver starts 10s RED prep, then 120s GREEN countdown
6. Sender receives hardware ACK, transitions to SCHIESS_BETRIEB state
7. Sender LCD updates: "Schießbetrieb" + shooter order display

**Total time**: ~30-50ms

### Scenario 2: Emergency Alarm

1. User holds OK button for 3 seconds on Sender (any state)
2. Sender calls `sendAlarmWithRetry()`
3. Attempt 1: RF24 transmits `{0x05, 0xFA}`
4. No ACK (receiver out of range)
5. Delay 200ms
6. Attempt 2: RF24 transmits `{0x05, 0xFA}`
7. Receiver ACK received
8. Receiver starts flashing RED + alarm buzzer
9. Sender returns to previous state
10. Sender LCD shows brief "Alarm gesendet"

**Total time**: ~200-400ms (depending on retry count)

---

## Change Log

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-12-13 | Initial protocol definition |
