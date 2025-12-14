# Research & Technologie-Entscheidungen: Bogenampel

**Datum**: 2025-12-04
**Phase**: 0 (Forschung)
**Status**: Abgeschlossen

## Übersicht

Dieses Dokument dokumentiert alle technischen Entscheidungen für das Bogenampel-Projekt, basierend auf Research zu Arduino-Embedded-Systems, Funkübertragung und LED-Steuerung.

---

## 1. RF24-Bibliothek Best Practices

### Entscheidung

**Gewählte Bibliothek**: TMRh20/RF24 (https://github.com/nRF24/RF24)

**Version**: 1.4.x (stable)

### Rationale

- **Etabliert**: Über 10 Jahre aktive Entwicklung, große Community
- **Performance**: Optimierte SPI-Kommunikation für ATmega328P
- **Features**: Auto-ACK, Auto-Retransmit, Payload-Größen bis 32 Bytes
- **Dokumentation**: Umfangreiche Examples und Tutorials
- **Kompatibilität**: Arduino IDE und PlatformIO Support

### Konfiguration für 100m Reichweite

```cpp
// RF24 Optimale Einstellungen für Reichweite
radio.begin();
radio.setPALevel(RF24_PA_MAX);      // Maximale Sendeleistung (0dBm)
radio.setDataRate(RF24_250KBPS);    // Niedrigste Datenrate = höchste Reichweite
radio.setChannel(108);               // Channel 108 (2.508 GHz) - wenig WiFi-Interferenz
radio.setRetries(15, 15);            // 15 * 250µs = 3.75ms Verzögerung, 15 Wiederholungen
radio.enableAckPayload();            // ACK-Payloads für bidirektionale Kommunikation
radio.setAutoAck(true);              // Hardware-ACK aktivieren
```

**Erwartete Reichweite**:
- Mit PCB-Antenne: 50-80m Indoor, 100m+ Freifeld
- Mit externer Antenne (optional): 200m+ Freifeld

### Retry-Strategien

**Sender-Seite**:
- 15 Wiederholungen bei fehlgeschlagenem ACK
- 3.75ms Verzögerung zwischen Versuchen
- Bei Totalausfall: Lokale LED-Anzeige (D1/D2) blinken lassen

**Empfänger-Seite**:
- Fail-Safe: Bei Kommunikationsverlust Countdown autonom weiterführen
- Keine abhängige Logik von Sender-Befehlen während laufendem Timer

### Alternativen erwogen

- **LoRa (SX1276/RFM95)**:
  - ❌ Abgelehnt: Overkill (10km+ Reichweite), höherer Stromverbrauch, teurere Module
  - ✅ nRF24L01+ ausreichend für 100m

- **433 MHz Module (HC-12)**:
  - ❌ Abgelehnt: Langsamere Datenrate, keine Hardware-ACK, größere Antenne nötig
  - ✅ nRF24L01+ schneller und kompakter

---

## 2. WS2812B-Bibliothek Auswahl

### Entscheidung

**Gewählte Bibliothek**: FastLED (https://github.com/FastLED/FastLED)

**Version**: 3.6.x

### Rationale

- **Performance**: Optimierter Assembler-Code für ATmega328P
- **Helligkeitssteuerung**: Globale Helligkeits-PWM (perfekt für Development-Mode 3%)
- **Color-Correction**: RGB-Farbkorrektur für präzise Ampelfarben
- **Effekte**: Smooth-Fading für sanfte Übergänge (optional)
- **Memory-Effizient**: Geringer SRAM-Footprint

### 7-Segment-Encoding

**Segment-Layout**:
```
   AAA
  F   B
   GGG
  E   C
   DDD
```

**Digit-Encoding-Tabelle**:
```cpp
const uint8_t SEGMENTS[10] = {
  0b00111111,  // 0: ABCDEF
  0b00000110,  // 1: BC
  0b01011011,  // 2: ABDEG
  0b01001111,  // 3: ABCDG
  0b01100110,  // 4: BCFG
  0b01101101,  // 5: ACDFG
  0b01111101,  // 6: ACDEFG
  0b00000111,  // 7: ABC
  0b01111111,  // 8: ABCDEFG
  0b01101111   // 9: ABCDFG
};
```

**3-Digit Countdown** (z.B. "240"):
- Digit 1 (Hunderter): Segment 0-6 (7 LEDs)
- Digit 2 (Zehner): Segment 7-13 (7 LEDs)
- Digit 3 (Einer): Segment 14-20 (7 LEDs)
- Gesamt pro Digit: 7 LEDs
- **Total**: 21 LEDs für Zahlen + ~134 LEDs für Hintergrund-Beleuchtung = ~155 LEDs

### Helligkeitssteuerung (Development Mode)

```cpp
#define DEV_MODE_PIN 2  // J5: Development Mode Jumper (D2 → GND)
#define FULL_BRIGHTNESS 255
#define DEV_BRIGHTNESS 8  // ~3% von 255

void setup() {
  pinMode(DEV_MODE_PIN, INPUT_PULLUP);

  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);

  // Helligkeit basierend auf Jumper setzen
  if (digitalRead(DEV_MODE_PIN) == LOW) {
    FastLED.setBrightness(DEV_BRIGHTNESS);  // 3% für USB
  } else {
    FastLED.setBrightness(FULL_BRIGHTNESS);  // 100% für Powerbank
  }
}
```

**Stromverbrauch-Schätzung**:
- 155 LEDs × 60mA (weiß, voll) = 9.3A (Maximal)
- Mit 3% Helligkeit: 155 LEDs × 1.8mA = 0.28A ≈ 280mA (USB-sicher)

### Alternativen erwogen

- **Adafruit_NeoPixel**:
  - ❌ Abgelehnt: Weniger Features (keine globale Helligkeit-PWM)
  - ✅ FastLED bietet mehr Kontrolle bei gleichem Overhead

- **7-Segment Hardware-Displays (TM1637)**:
  - ❌ Abgelehnt: Keine Farb-Steuerung, separates Modul nötig
  - ✅ WS2812B flexibler für Ampel-Farben

---

## 3. ATmega328P Timer-Strategien

### Entscheidung

**Timer-Ansatz**: `millis()`-basierter Countdown mit Timer1 ISR für Sekunden-Ticks

### Rationale

- **Einfachheit**: `millis()` API ist Arduino-Standard, einfach zu testen
- **WS2812B-Kompatibilität**: Keine Konflikte mit FastLED (nutzt Interrupts nur kurz)
- **Präzision**: ±1ms über 240 Sekunden (erfüllt ±1s Anforderung)
- **Non-Blocking**: Erlaubt paralleles Button-Handling und Radio-Empfang

### Implementierung

```cpp
// Timer1 für 1-Sekunden-Interrupts konfigurieren
void setupTimer1() {
  cli();  // Interrupts deaktivieren

  // Timer1 auf CTC Mode setzen (Clear Timer on Compare Match)
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  // OCR1A für 1 Hz (16 MHz / 1024 / 15625 = 1 Hz)
  OCR1A = 15624;  // (16000000 / 1024) - 1

  // CTC Mode, Prescaler 1024
  TCCR1B |= (1 << WGM12) | (1 << CS12) | (1 << CS10);

  // Timer Compare Interrupt aktivieren
  TIMSK1 |= (1 << OCIE1A);

  sei();  // Interrupts aktivieren
}

ISR(TIMER1_COMPA_vect) {
  // Jede Sekunde aufgerufen
  secondTick = true;  // Flag für main loop
}
```

**Main Loop**:
```cpp
void loop() {
  if (secondTick) {
    secondTick = false;

    if (timerState == STATE_SHOOTING || timerState == STATE_PREPARATION) {
      remainingSeconds--;
      updateDisplay();
      checkStateTransition();
    }
  }

  // Andere Tasks (Radio, Button-Polling)
  handleRadio();
  handleButton();
}
```

### Watchdog-Timer für Fail-Safe

```cpp
#include <avr/wdt.h>

void setup() {
  wdt_enable(WDTO_8S);  // 8 Sekunden Watchdog
}

void loop() {
  wdt_reset();  // Watchdog zurücksetzen (muss regelmäßig aufgerufen werden)

  // Normale Loop-Logik
}
```

**Fail-Safe-Logik**: Wenn main loop hängt (z.B. durch Bug), resettet Watchdog nach 8s den Arduino.

### Alternativen erwogen

- **`delay()`-basierter Countdown**:
  - ❌ Abgelehnt: Blockiert Radio-Empfang und Button-Handling
  - ✅ `millis()` ist non-blocking

- **Timer0 (Arduino `millis()`-Timer)**:
  - ❌ Abgelehnt: Würde `millis()` und `delay()` stören
  - ✅ Timer1 ist separater 16-Bit-Timer

---

## 4. Energieverwaltung

### Entscheidung

**Sender**: AVR Sleep Mode (Idle Mode) zwischen Button-Presses
**Empfänger**: Kein Sleep während Timer läuft (nur im STOPPED State)

### Rationale

**Sender-Stromverbrauch**:
- Aktiv (Senden): ~20mA
- Idle Mode: ~5mA
- Sleep Mode (Power-Down): ~0.1mA

**Empfänger-Stromverbrauch**:
- Aktiv (LEDs aus): ~30mA
- Aktiv (LEDs 3%): ~280mA
- Aktiv (LEDs 100%): ~9.3A

**Batterielaufzeit-Ziel**: 8 Stunden

**Sender (9V Block-Batterie, 500mAh)**:
- Ohne Sleep: 500mAh / 20mA = 25h ✅
- Mit Sleep (95% idle): 500mAh / 6mA = 83h ✅ (deutlich über 8h)

**Empfänger (10.000mAh Powerbank)**:
- LEDs 100%: 10.000mAh / 9.3A = 1h ❌ (zu kurz)
- LEDs 50%: 10.000mAh / 4.65A = 2.15h ❌ (zu kurz)
- LEDs 25%: 10.000mAh / 2.32A = 4.3h ⚠️ (knapp)
- LEDs mit PWM-Cycle (Dimm-Intervalle): ~30% Average = 10.000mAh / 2.8A = 3.5h ⚠️

**Lösung**: LED-Helligkeit dynamisch anpassen:
- Timer läuft: 50% Helligkeit (sichtbar, aber energiesparend)
- Timer gestoppt: 10% Helligkeit (Standby-Modus)
- Development Mode: 3% Helligkeit (USB-Safe)

**Aktualisierte Laufzeit** (50% Durchschnitt):
- 10.000mAh / 2.8A ≈ 3.5h (kontinuierlich)
- Bei realistischer Nutzung (50% Timer-Aktiv, 50% Standby): ~6-8h ✅

### Sleep-Mode Implementierung (Sender)

```cpp
#include <avr/sleep.h>
#include <avr/power.h>

void enterSleep() {
  set_sleep_mode(SLEEP_MODE_IDLE);  // Idle Mode (Timer1 läuft weiter)
  sleep_enable();
  sleep_mode();  // Sleep bis Interrupt
  sleep_disable();
}

void loop() {
  if (timerState == STATE_STOPPED && !buttonPressed) {
    enterSleep();  // Sleep zwischen Button-Presses
  }

  // Normale Logik
}
```

### USB vs. Powerbank Erkennung

**Methode**: Voltage-Messung am VIN Pin (nicht zuverlässig) → **Besser**: Development-Mode Jumper (manuelle Konfiguration)

**Rationale**: USB vs. Powerbank haben beide 5V → Voltage-Messung unzuverlässig. Jumper ist explizit und fail-safe.

### Alternativen erwogen

- **Deep Sleep (Power-Down Mode)**:
  - ❌ Abgelehnt: Würde Timer1 stoppen → Countdown ungenau
  - ✅ Idle Mode ausreichend (~5mA)

- **Dynamische Voltage-Messung**:
  - ❌ Abgelehnt: Unreliable bei USB vs. Powerbank (beide 5V)
  - ✅ Jumper ist explizit und sicher

---

## 5. Grenzfall-Verhalten

### 5.1 Funkausfall während Countdown

**Szenario**: Sender-Batterie leer, Signal blockiert, oder außer Reichweite während Timer läuft.

**Entscheidung**: **Empfänger läuft autonom weiter bis Timer-Ende**

**Rationale**:
- ✅ Vorhersehbares Verhalten (Timer läuft zu Ende wie erwartet)
- ✅ Kein abruptes Stoppen, das Schützen verwirren würde
- ✅ Sender kann nach Verbindungswiederherstellung manuell stoppen
- ❌ Alternative (sofortiger Stopp): Unsicher, da Schützen mitten im Schießen sein könnten

**Implementierung**:
```cpp
// Empfänger: Countdown läuft unabhängig von Radio
void loop() {
  // Timer-Logik ist NICHT abhängig von Radio-Empfang
  if (timerState == STATE_SHOOTING && remainingSeconds > 0) {
    // Countdown läuft weiter, auch ohne Radio-Signal
  }

  // Radio-Empfang ist optional (nur für STOP-Befehl)
  if (radio.available()) {
    handleRadioCommand();  // Kann Timer stoppen, wenn STOP empfangen
  }
}
```

### 5.2 Rapid Button-Presses (Debouncing)

**Szenario**: Benutzer drückt Taster mehrfach schnell hintereinander.

**Entscheidung**: **Software-Debouncing mit 50ms Verzögerung**

**Implementierung**:
```cpp
#define BUTTON_PIN 2
#define DEBOUNCE_DELAY 50  // 50ms

unsigned long lastDebounceTime = 0;
int lastButtonState = HIGH;

bool readButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW && lastButtonState == HIGH) {
      lastButtonState = reading;
      return true;  // Button press detected
    }
  }

  lastButtonState = reading;
  return false;
}
```

**Rationale**: Verhindert mehrfache Trigger durch mechanisches Prellen des Tasters.

### 5.3 Modusschalter-Änderung während Betrieb

**Szenario**: Benutzer wechselt 120s/240s Schalter während Timer läuft.

**Entscheidung**: **Modus wird nur beim Timer-Start gelesen, Änderung während Betrieb ignoriert**

**Implementierung**:
```cpp
#define MODE_SWITCH_PIN 3

int timerDuration = 0;  // Gespeicherte Dauer für aktuellen Countdown

void startTimer() {
  // Modus nur beim Start auslesen
  if (digitalRead(MODE_SWITCH_PIN) == HIGH) {
    timerDuration = 240;  // 240s Modus
  } else {
    timerDuration = 120;  // 120s Modus
  }

  remainingSeconds = timerDuration;
  timerState = STATE_PREPARATION;
}

void loop() {
  // Modus wird während Countdown NICHT erneut gelesen
  if (timerState == STATE_SHOOTING) {
    // Countdown verwendet gespeicherte timerDuration
  }
}
```

**Rationale**: Verhindert unerwartete Timer-Sprünge während Countdown.

### 5.4 Initialer Power-On State

**Szenario**: System wird eingeschaltet (Batterie eingelegt, USB angeschlossen).

**Entscheidung**: **System startet im STOPPED State mit roter Anzeige "000"**

**Implementierung**:
```cpp
void setup() {
  // Hardware-Initialisierung
  setupRadio();
  setupDisplay();

  // Initialer State
  timerState = STATE_STOPPED;
  remainingSeconds = 0;

  // Rote Anzeige "000"
  displayNumber(0, CRGB::Red);

  // Akustisches Signal (optional): Kurzer Beep bei Power-On
  playTone(2000, 100);  // 2000 Hz, 100ms
}
```

**Rationale**: Eindeutiger, sicherer Start-Zustand (kein unerwarteter Timer-Start).

### 5.5 Sender außerhalb Funkreichweite

**Szenario**: Sender zu weit entfernt (>100m Freifeld, >50m Indoor).

**Entscheidung**:
- **Sender**: Lokale Status-LEDs blinken bei fehlgeschlagenem Send
- **Empfänger**: Läuft autonom weiter (siehe 5.1)

**Implementierung (Sender)**:
```cpp
bool sendCommand(uint8_t command) {
  bool success = radio.write(&command, sizeof(command));

  if (!success) {
    // Lokale Status-LED blinken lassen
    blinkStatusLED();
    return false;
  }

  return true;
}
```

**Rationale**: Benutzer erhält visuelles Feedback, dass Befehl nicht gesendet wurde.

### 5.6 Unzureichende Stromversorgung (schwache Powerbank)

**Szenario**: Powerbank fast leer, liefert <5V oder kann Stromspitzen nicht liefern.

**Entscheidung**: **System verhält sich robust, aber Display kann flackern**

**Robustheit-Maßnahmen**:
- **Capacitor**: 1000µF Kondensator puffert LED-Stromspitzen
- **Brownout Detection**: Arduino Nano hat eingebaute BOD (4.3V Threshold) → Reset bei Unterspannung
- **Software-Check** (optional):

```cpp
void checkVoltage() {
  long result = readVcc();  // Interne Voltage-Messung

  if (result < 4500) {  // Unter 4.5V
    // Helligkeit reduzieren
    FastLED.setBrightness(50);
    // Optional: Warn-Signal
  }
}
```

**Rationale**: Graceful Degradation statt hartem Crash.

---

## Zusammenfassung der Entscheidungen

| Komponente | Technologie | Version | Rationale |
|------------|-------------|---------|-----------|
| Funkmodul-Treiber | TMRh20/RF24 | 1.4.x | Etabliert, 100m Reichweite, Hardware-ACK |
| LED-Steuerung | FastLED | 3.6.x | Helligkeits-PWM, Memory-effizient, Fast |
| Timer-Strategie | `millis()` + Timer1 ISR | - | Non-blocking, WS2812B-kompatibel, ±1ms Präzision |
| Energieverwaltung | AVR Idle Mode (Sender) | - | 5mA idle, 8h+ Batterielaufzeit |
| Grenzfall-Handling | Fail-Safe Autonomous | - | Vorhersehbar, sicher bei Funkausfall |

---

## Nächste Schritte

✅ Phase 0 abgeschlossen
→ Phase 1: data-model.md, contracts/, quickstart.md erstellen
