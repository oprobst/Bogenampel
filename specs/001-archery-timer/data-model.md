# Datenmodell & State-Machine: Bogenampel

**Datum**: 2025-12-04
**Phase**: 1 (Design)
**Status**: Abgeschlossen

## Übersicht

Dieses Dokument definiert die State-Machine, Datenstrukturen und Zustandsübergänge für die Bogenampel-Firmware.

---

## Timer State-Machine

### Zustände

Das Empfänger-System hat 5 Hauptzustände:

```
┌─────────────┐
│   STOPPED   │◄─────┐
│  (Rot, 000) │      │
└──────┬──────┘      │
       │ START       │ STOP / Timer abgelaufen
       │             │
┌──────▼──────────┐  │
│  PREPARATION    │  │
│ (Gelb, 10s countdown)│
└──────┬──────────┘  │
       │ 10s elapsed │
       │             │
┌──────▼──────────┐  │
│   SHOOTING      │  │
│ (Grün, 120/240s)│──┘
└──────┬──────────┘  │
       │ ≤30s remaining
       │             │
┌──────▼──────────┐  │
│    WARNING      │  │
│ (Gelb, ≤30s)    │──┘
└──────┬──────────┘
       │ Timer = 0
       │
┌──────▼──────────┐
│    EXPIRED      │
│  (Rot, 000)     │
└─────────────────┘
       │
       │ (Auto-transition nach 2s)
       │
┌──────▼──────────┐
│   STOPPED       │
└─────────────────┘
```

### Zustandsdefinitionen

| Zustand | Enum | Beschreibung | Farbe | Display | Audio |
|---------|------|-------------|-------|---------|-------|
| STOPPED | 0 | Timer gestoppt, wartet auf START | Rot | 000 | - |
| PREPARATION | 1 | 10s Countdown zur Vorbereitung | Gelb | 10-01 | 1500 Hz bei Start |
| SHOOTING | 2 | Hauptschießphase (120s oder 240s) | Grün | 240-31 | 2500 Hz bei Start |
| WARNING | 3 | Letzte 30 Sekunden Warnung | Gelb | 30-01 | - |
| EXPIRED | 4 | Timer abgelaufen | Rot | 000 | 2000 Hz |

### Zustandsübergänge

#### 1. STOPPED → PREPARATION

**Trigger**: START-Befehl vom Sender empfangen

**Bedingungen**:
- Timer ist im STOPPED State
- Gültiger START-Befehl

**Aktionen**:
```cpp
timerState = STATE_PREPARATION;
remainingSeconds = 10;
playTone(1500, 200);  // Tiefer Ton, 200ms
updateDisplay(remainingSeconds, CRGB::Yellow);
```

#### 2. PREPARATION → SHOOTING

**Trigger**: Countdown erreicht 0

**Bedingungen**:
- `remainingSeconds == 0`
- State ist PREPARATION

**Aktionen**:
```cpp
timerState = STATE_SHOOTING;
remainingSeconds = timerDuration;  // 120 oder 240
playTone(2500, 200);  // Hoher Ton, 200ms
updateDisplay(remainingSeconds, CRGB::Green);
```

#### 3. SHOOTING → WARNING

**Trigger**: Countdown erreicht 30 Sekunden

**Bedingungen**:
- `remainingSeconds == 30`
- State ist SHOOTING

**Aktionen**:
```cpp
timerState = STATE_WARNING;
// remainingSeconds bleibt bei 30, läuft weiter
updateDisplay(remainingSeconds, CRGB::Yellow);
// Kein Audio-Signal (kontinuierlicher Countdown)
```

#### 4. WARNING → EXPIRED (oder SHOOTING → EXPIRED)

**Trigger**: Countdown erreicht 0

**Bedingungen**:
- `remainingSeconds == 0`
- State ist WARNING oder SHOOTING

**Aktionen**:
```cpp
timerState = STATE_EXPIRED;
remainingSeconds = 0;
playTone(2000, 500);  // Mittlerer Ton, 500ms
updateDisplay(0, CRGB::Red);
expiredTimestamp = millis();  // Für Auto-Transition
```

#### 5. EXPIRED → STOPPED

**Trigger**: 2 Sekunden nach EXPIRED

**Bedingungen**:
- State ist EXPIRED
- `(millis() - expiredTimestamp) >= 2000`

**Aktionen**:
```cpp
timerState = STATE_STOPPED;
// remainingSeconds bleibt 0
// Display bleibt "000" Rot
```

#### 6. (ANY) → STOPPED

**Trigger**: STOP-Befehl vom Sender empfangen

**Bedingungen**:
- Gültiger STOP-Befehl
- State ist nicht bereits STOPPED

**Aktionen**:
```cpp
timerState = STATE_STOPPED;
remainingSeconds = 0;
updateDisplay(0, CRGB::Red);
// Kein Audio-Signal (manueller Stopp)
```

---

## Datenstrukturen

### 1. TimerState (Empfänger)

```cpp
enum TimerState {
  STATE_STOPPED = 0,
  STATE_PREPARATION = 1,
  STATE_SHOOTING = 2,
  STATE_WARNING = 3,
  STATE_EXPIRED = 4
};

struct TimerConfig {
  uint8_t state;             // Aktueller Zustand (TimerState enum)
  uint16_t remainingSeconds; // Verbleibende Zeit in Sekunden
  uint16_t timerDuration;    // Gewählte Dauer (120 oder 240)
  unsigned long lastTick;    // Letzter millis() Wert für Sekunden-Tick
  unsigned long expiredTimestamp;  // Timestamp für EXPIRED → STOPPED Transition
};

// Globale Instanz
TimerConfig timer = {
  .state = STATE_STOPPED,
  .remainingSeconds = 0,
  .timerDuration = 120,  // Default: 120s
  .lastTick = 0,
  .expiredTimestamp = 0
};
```

### 2. RadioCommand (Sender & Empfänger)

```cpp
enum CommandType {
  CMD_START = 0x01,
  CMD_STOP = 0x02
};

struct RadioCommand {
  uint8_t command;      // CommandType enum
  uint8_t checksum;     // Simple checksum für Validierung
};

// Checksum-Berechnung
uint8_t calculateChecksum(uint8_t command) {
  return (command ^ 0xAA);  // XOR mit Konstante
}

// Beispiel: START-Befehl senden
RadioCommand cmd = {
  .command = CMD_START,
  .checksum = calculateChecksum(CMD_START)
};
```

### 3. DisplayConfig (Empfänger)

```cpp
struct DisplayConfig {
  uint8_t brightness;      // 0-255 (FastLED Helligkeit)
  bool developmentMode;    // true = 3% Helligkeit
  CRGB currentColor;       // Aktuelle Ampelfarbe
};

// Globale Instanz
DisplayConfig display = {
  .brightness = 255,
  .developmentMode = false,
  .currentColor = CRGB::Red
};
```

### 4. ButtonState (Sender)

```cpp
struct ButtonState {
  uint8_t pin;                 // Button Pin (z.B. 2)
  int lastState;               // Letzter Button-State (HIGH/LOW)
  unsigned long lastDebounceTime;  // Letzter Debounce-Timestamp
  uint16_t debounceDelay;      // Debounce-Verzögerung in ms (z.B. 50)
};

// Globale Instanz
ButtonState button = {
  .pin = 2,  // J1: Taster
  .lastState = HIGH,
  .lastDebounceTime = 0,
  .debounceDelay = 50
};
```

### 5. AudioConfig (Empfänger)

```cpp
struct AudioConfig {
  uint8_t buzzerPin;       // Buzzer Pin (z.B. 8)
  uint8_t volumePin;       // Potentiometer Pin (A0)
  uint8_t volumeLevel;     // 0-255 (gemappter ADC-Wert)
};

// Globale Instanz
AudioConfig audio = {
  .buzzerPin = 8,  // J8: KY-006 Buzzer
  .volumePin = A0,  // RV1: Potentiometer
  .volumeLevel = 128  // Default: 50%
};
```

---

## Daten-Persistenz

### EEPROM-Layout (optional, für zukünftige Features)

```cpp
// EEPROM Address Map
#define EEPROM_MAGIC 0x00        // 2 Bytes: Magic Number (0xBAAD)
#define EEPROM_TIMER_DURATION 0x02  // 1 Byte: Letzte Timer-Dauer (120/240)
#define EEPROM_BRIGHTNESS 0x03      // 1 Byte: Letzte Helligkeit
#define EEPROM_VOLUME 0x04          // 1 Byte: Letzte Lautstärke

// Beispiel: Timer-Dauer speichern
void saveTimerDuration(uint8_t duration) {
  EEPROM.write(EEPROM_TIMER_DURATION, duration);
}

// Beispiel: Timer-Dauer laden
uint8_t loadTimerDuration() {
  uint8_t duration = EEPROM.read(EEPROM_TIMER_DURATION);
  if (duration != 120 && duration != 240) {
    return 120;  // Default
  }
  return duration;
}
```

**Hinweis**: EEPROM-Nutzung ist optional für MVP. Kann später hinzugefügt werden.

---

## Speicher-Footprint-Analyse

### Flash-Speicher (ATmega328P: 32KB)

| Komponente | Geschätzte Größe | Notizen |
|------------|-----------------|---------|
| Arduino Core | ~4KB | Bootloader + Core-Funktionen |
| RF24 Library | ~3KB | nRF24L01+ Treiber |
| FastLED Library | ~8KB | WS2812B Steuerung |
| Application Code | ~6KB | State-Machine, Display, Audio |
| **Total** | **~21KB** | ~65% genutzt, 11KB frei |

✅ **Ausreichend Platz** für zukünftige Features

### SRAM (ATmega328P: 2KB)

| Komponente | Größe | Notizen |
|------------|-------|---------|
| LED Array (155 LEDs × 3 Bytes) | 465 Bytes | WS2812B RGB-Buffer |
| RF24 Buffer | 32 Bytes | TX/RX Payload |
| Global Variables | ~100 Bytes | TimerConfig, DisplayConfig, etc. |
| Stack (estimated) | ~300 Bytes | Function calls, local vars |
| **Total** | **~897 Bytes** | ~44% genutzt, 1151 Bytes frei |

✅ **Ausreichend SRAM** - Keine Speicherprobleme erwartet

---

## State-Machine Implementierung

### Empfänger Main Loop (Pseudo-Code)

```cpp
void loop() {
  // 1. Handle Timer-Tick (Sekunden-Update)
  if (timer.state != STATE_STOPPED && timer.state != STATE_EXPIRED) {
    if (millis() - timer.lastTick >= 1000) {
      timer.lastTick = millis();
      timer.remainingSeconds--;

      // State-Transitions prüfen
      checkStateTransitions();

      // Display aktualisieren
      updateDisplay(timer.remainingSeconds, display.currentColor);
    }
  }

  // 2. Handle EXPIRED → STOPPED Transition
  if (timer.state == STATE_EXPIRED) {
    if (millis() - timer.expiredTimestamp >= 2000) {
      timer.state = STATE_STOPPED;
    }
  }

  // 3. Handle Radio-Empfang (non-blocking)
  if (radio.available()) {
    RadioCommand cmd;
    radio.read(&cmd, sizeof(cmd));

    // Validiere Checksum
    if (cmd.checksum == calculateChecksum(cmd.command)) {
      handleRadioCommand(cmd.command);
    }
  }

  // 4. Update Audio-Lautstärke (Potentiometer)
  audio.volumeLevel = map(analogRead(audio.volumePin), 0, 1023, 0, 255);

  // 5. Watchdog zurücksetzen
  wdt_reset();
}

void checkStateTransitions() {
  switch (timer.state) {
    case STATE_PREPARATION:
      if (timer.remainingSeconds == 0) {
        transitionToShooting();
      }
      break;

    case STATE_SHOOTING:
      if (timer.remainingSeconds == 30) {
        transitionToWarning();
      } else if (timer.remainingSeconds == 0) {
        transitionToExpired();
      }
      break;

    case STATE_WARNING:
      if (timer.remainingSeconds == 0) {
        transitionToExpired();
      }
      break;
  }
}
```

### Sender Main Loop (Pseudo-Code)

```cpp
void loop() {
  // 1. Handle Button-Press (mit Debouncing)
  if (readButton()) {
    if (timer.state == STATE_STOPPED) {
      sendCommand(CMD_START);
      timer.state = STATE_PREPARATION;  // Lokales State-Tracking
    } else {
      sendCommand(CMD_STOP);
      timer.state = STATE_STOPPED;
    }
  }

  // 2. Update Status-LEDs
  updateStatusLEDs(timer.state);

  // 3. Enter Sleep wenn idle
  if (timer.state == STATE_STOPPED && !buttonPressed) {
    enterSleep();
  }

  // 4. Watchdog zurücksetzen
  wdt_reset();
}

bool sendCommand(uint8_t cmd) {
  RadioCommand packet = {
    .command = cmd,
    .checksum = calculateChecksum(cmd)
  };

  bool success = radio.write(&packet, sizeof(packet));

  if (!success) {
    blinkStatusLED();  // Visuelles Feedback bei Fehler
  }

  return success;
}
```

---

## Validierung & Testbarkeit

### Unit-Tests (Zustandsübergänge)

```cpp
// Test: STOPPED → PREPARATION
void test_stopped_to_preparation() {
  timer.state = STATE_STOPPED;
  handleRadioCommand(CMD_START);
  assert(timer.state == STATE_PREPARATION);
  assert(timer.remainingSeconds == 10);
}

// Test: PREPARATION → SHOOTING
void test_preparation_to_shooting() {
  timer.state = STATE_PREPARATION;
  timer.remainingSeconds = 1;
  checkStateTransitions();  // Tick down to 0
  timer.remainingSeconds = 0;
  checkStateTransitions();
  assert(timer.state == STATE_SHOOTING);
  assert(timer.remainingSeconds == timer.timerDuration);
}

// Test: SHOOTING → WARNING bei 30s
void test_shooting_to_warning() {
  timer.state = STATE_SHOOTING;
  timer.remainingSeconds = 31;
  checkStateTransitions();  // Noch im SHOOTING
  assert(timer.state == STATE_SHOOTING);

  timer.remainingSeconds = 30;
  checkStateTransitions();  // Jetzt WARNING
  assert(timer.state == STATE_WARNING);
}
```

### Hardware-in-the-Loop Tests

1. **Reichweitentest**: Sender 100m entfernt, START/STOP Befehle senden
2. **Countdown-Genauigkeit**: Timer für 240s laufen lassen, mit Stoppuhr vergleichen
3. **Farbwechsel**: Visuell prüfen: Rot (Stop) → Gelb (Prep) → Grün (Shoot) → Gelb (Warn) → Rot (Expired)
4. **Audio-Frequenzen**: Mit Frequenzzähler messen: 1500 Hz, 2500 Hz, 2000 Hz
5. **Stromverbrauch**: Multimeter-Messung bei verschiedenen Helligkeiten

---

## Nächste Schritte

✅ Phase 1 Datenmodell abgeschlossen
→ Fortsetzung: contracts/radio-protocol.md erstellen
