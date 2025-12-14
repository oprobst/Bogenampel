# Funkprotokoll-Spezifikation: Bogenampel

**Datum**: 2025-12-04
**Phase**: 1 (Design)
**Status**: Abgeschlossen
**Protokoll-Version**: 1.0

## Übersicht

Dieses Dokument definiert das Funkprotokoll zwischen Sender (Fernbedienung) und Empfänger (Anzeigeeinheit) über nRF24L01+ Module (2.4 GHz).

---

## Protokoll-Parameter

### RF24-Konfiguration

| Parameter | Wert | Rationale |
|-----------|------|-----------|
| Frequenz-Kanal | 108 | 2.508 GHz (wenig WiFi-Interferenz) |
| Datenrate | 250 kbps | Maximale Reichweite |
| TX-Power | PA_MAX | 0 dBm (maximale Sendeleistung) |
| Payload-Größe | 2 Bytes | Minimal (CMD + Checksum) |
| Auto-ACK | Aktiviert | Hardware-Bestätigung |
| Auto-Retry | 15 Versuche | 3.75ms Verzögerung zwischen Versuchen |
| CRC | 2 Bytes | Hardware-CRC-Prüfung |

### Adressen

**Sender → Empfänger**:
- **TX-Adresse (Sender)**: `0xF0F0F0F0E1`
- **RX-Adresse (Empfänger)**: `0xF0F0F0F0E1`

**Rationale**: Eindeutige 40-Bit-Adresse, verhindert Kollisionen mit anderen nRF24-Geräten.

**Konfiguration (Arduino)**:
```cpp
const uint64_t pipe = 0xF0F0F0F0E1LL;

// Sender Setup
radio.openWritingPipe(pipe);
radio.stopListening();  // TX-Modus

// Empfänger Setup
radio.openReadingPipe(1, pipe);
radio.startListening();  // RX-Modus
```

---

## Befehlsformat

### Payload-Struktur

```
┌─────────────────────────────────────┐
│  Byte 0   │  Byte 1                │
│  Command  │  Checksum              │
│  (uint8)  │  (uint8)               │
└─────────────────────────────────────┘
  2 Bytes Total
```

### Befehls-Codes

| Befehl | Code (Hex) | Beschreibung | Sender-Aktion | Empfänger-Reaktion |
|--------|-----------|-------------|---------------|-------------------|
| START  | 0x01 | Timer starten | Button-Press im STOPPED State | Transition zu PREPARATION |
| STOP   | 0x02 | Timer stoppen | Button-Press während Timer läuft | Transition zu STOPPED |

### Checksum-Berechnung

**Algorithmus**: XOR mit Konstante

```cpp
#define CHECKSUM_KEY 0xAA

uint8_t calculateChecksum(uint8_t command) {
  return command ^ CHECKSUM_KEY;
}
```

**Beispiele**:
- START (0x01): Checksum = 0x01 XOR 0xAA = 0xAB
- STOP (0x02): Checksum = 0x02 XOR 0xAA = 0xA8

**Rationale**: Einfach, schnell, erkennt Single-Bit-Fehler. CRC erfolgt zusätzlich durch nRF24-Hardware.

---

## Nachrichtenfluss

### 1. START-Befehl

```
┌─────────┐                                  ┌───────────┐
│ Sender  │                                  │ Empfänger │
└────┬────┘                                  └─────┬─────┘
     │                                             │
     │ Button-Press (J1)                          │
     │                                             │
     │─────────── START (0x01 0xAB) ─────────────>│
     │                                             │
     │                                             │ Validate Checksum
     │                                             │
     │<──────────── ACK ──────────────────────────│
     │                                             │
     │                                             │ Transition: STOPPED → PREPARATION
     │                                             │ Display: "010" Gelb
     │                                             │ Audio: 1500 Hz
```

**Sender-Code**:
```cpp
if (button_pressed && timer.state == STATE_STOPPED) {
  RadioCommand cmd = {
    .command = CMD_START,
    .checksum = calculateChecksum(CMD_START)
  };

  bool success = radio.write(&cmd, sizeof(cmd));

  if (success) {
    // ACK empfangen, Befehl gesendet
    updateStatusLED(STATUS_OK);
  } else {
    // Kein ACK, Befehl fehlgeschlagen
    updateStatusLED(STATUS_ERROR);
    blinkStatusLED();
  }
}
```

**Empfänger-Code**:
```cpp
if (radio.available()) {
  RadioCommand cmd;
  radio.read(&cmd, sizeof(cmd));

  // Validiere Checksum
  if (cmd.checksum == calculateChecksum(cmd.command)) {
    handleCommand(cmd.command);
  } else {
    // Ungültige Checksum, ignorieren
    errorCount++;
  }
}

void handleCommand(uint8_t command) {
  switch (command) {
    case CMD_START:
      if (timer.state == STATE_STOPPED) {
        startTimer();  // Transition zu PREPARATION
      }
      break;

    case CMD_STOP:
      if (timer.state != STATE_STOPPED) {
        stopTimer();  // Transition zu STOPPED
      }
      break;
  }
}
```

### 2. STOP-Befehl

```
┌─────────┐                                  ┌───────────┐
│ Sender  │                                  │ Empfänger │
└────┬────┘                                  └─────┬─────┘
     │                                             │
     │ Button-Press (J1)                          │ Timer läuft: "187" Grün
     │                                             │
     │──────────── STOP (0x02 0xA8) ─────────────>│
     │                                             │
     │                                             │ Validate Checksum
     │                                             │
     │<──────────── ACK ──────────────────────────│
     │                                             │
     │                                             │ Transition: ANY → STOPPED
     │                                             │ Display: "000" Rot
```

---

## Fehlerbehandlung

### 1. Checksum-Fehler

**Szenario**: Empfänger empfängt Payload mit ungültiger Checksum.

**Verhalten**:
```cpp
if (cmd.checksum != calculateChecksum(cmd.command)) {
  // Ungültiger Befehl, ignorieren
  errorCount++;
  return;  // Kein State-Change
}
```

**Rationale**: Verhindert Ausführung korrupter Befehle.

### 2. Übertragungsfehler (kein ACK)

**Szenario**: Sender erhält kein ACK vom Empfänger (z.B. außer Reichweite).

**Verhalten (Sender)**:
```cpp
bool success = radio.write(&cmd, sizeof(cmd));

if (!success) {
  // Kein ACK nach 15 Retries
  // Lokale Status-LED blinken lassen
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(100);
    digitalWrite(STATUS_LED, LOW);
    delay(100);
  }

  // Empfänger läuft autonom weiter (Fail-Safe)
}
```

**Rationale**: Benutzer erhält visuelles Feedback, aber Empfänger bleibt operational (autonom).

### 3. Funkausfall während Countdown

**Szenario**: Empfänger verliert Verbindung zum Sender während Timer läuft.

**Verhalten (Empfänger)**:
```cpp
// Empfänger läuft UNABHÄNGIG von Radio-Empfang
void loop() {
  // Timer-Logik (autonom)
  if (timer.state != STATE_STOPPED) {
    updateCountdown();  // Läuft weiter ohne Radio
  }

  // Radio-Empfang (optional, nur für STOP)
  if (radio.available()) {
    handleRadioCommand();
  }
}
```

**Rationale**: Fail-Safe-Prinzip - Timer läuft predictable weiter bis zum Ende. Sender kann nach Wiederverbindung manuell stoppen.

### 4. Doppelte Befehle (Button-Bouncing)

**Szenario**: Sender schickt mehrere START-Befehle durch mechanisches Prellen.

**Verhalten (Sender - Debouncing)**:
```cpp
bool readButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading == LOW) {
      lastButtonState = reading;
      return true;  // Gültiger Button-Press
    }
  }

  lastButtonState = reading;
  return false;
}
```

**Verhalten (Empfänger - Idempotenz)**:
```cpp
void handleCommand(uint8_t command) {
  switch (command) {
    case CMD_START:
      // Nur starten, wenn bereits STOPPED
      if (timer.state == STATE_STOPPED) {
        startTimer();
      }
      // Sonst ignorieren (idempotent)
      break;

    case CMD_STOP:
      // Stoppen erlaubt in jedem State außer STOPPED
      if (timer.state != STATE_STOPPED) {
        stopTimer();
      }
      break;
  }
}
```

**Rationale**: Verhindert unerwartete State-Transitions durch doppelte Befehle.

---

## Performance-Metriken

### Latenz

| Metrik | Ziel | Gemessener Wert (erwartet) |
|--------|------|---------------------------|
| Button → Funk-TX | <10ms | ~5ms |
| Funk-Übertragung | <50ms | ~10-30ms (abhängig von Reichweite) |
| Funk-RX → State-Change | <10ms | ~5ms |
| **Gesamt-Latenz** | **<100ms** | **~20-40ms** ✅ |

### Zuverlässigkeit

| Metrik | Ziel | Rationale |
|--------|------|-----------|
| Befehlsausfall-Rate | <1% | Auto-Retry mit 15 Versuchen, Hardware-CRC |
| Funkreichweite (Indoor) | 50m | PA_MAX + 250kbps Datenrate |
| Funkreichweite (Outdoor) | 100m | Freifeld ohne Hindernisse |

### Stromverbrauch

| Komponente | Aktiv | Idle | Sleep |
|------------|-------|------|-------|
| Sender (nRF24 TX) | 12mA | - | - |
| Sender (Arduino + nRF24 RX) | 20mA | 5mA (Idle Mode) | 0.1mA (Power-Down) |
| Empfänger (nRF24 RX) | 14mA | - | - |
| Empfänger (Arduino + nRF24 RX) | 30mA | - | - |

---

## Test-Szenarien

### 1. Reichweitentest

**Ziel**: Validiere 50m Indoor / 100m Outdoor Reichweite

**Prozedur**:
1. Platziere Empfänger an Startpunkt
2. Bewege Sender in 10m-Schritten weg
3. Sende START/STOP Befehle bei jeder Distanz
4. Notiere Erfolgsrate (ACK empfangen?)

**Erwartetes Ergebnis**:
- 0-50m Indoor: 100% Erfolgsrate
- 50-100m Outdoor: >95% Erfolgsrate
- >100m: Degraded (<50% Erfolgsrate)

### 2. Interferenz-Test

**Ziel**: Validiere Robustheit bei WiFi-Interferenz

**Prozedur**:
1. Platziere WiFi-Router neben Empfänger (Channel 1, 6, 11)
2. Sende START/STOP Befehle
3. Messe Erfolgsrate

**Erwartetes Ergebnis**:
- Channel 108 (2.508 GHz) sollte wenig WiFi-Interferenz haben
- >95% Erfolgsrate selbst bei starkem WiFi-Traffic

### 3. Latenz-Test

**Ziel**: Messe Button-Press → State-Change Latenz

**Prozedur**:
1. Drücke Sender-Taster
2. Messe Zeit bis Empfänger-Display ändert Farbe (mit Oszilloskop oder Kamera)

**Erwartetes Ergebnis**:
- <100ms Gesamtlatenz (Button → Display-Update)

### 4. Dauerlauf-Test

**Ziel**: Validiere Stabilität über 8 Stunden

**Prozedur**:
1. Starte Timer, lasse vollständig ablaufen
2. Wiederhole 10× (simuliert 10 Schießrunden)
3. Prüfe auf Memory-Leaks, Freeze, etc.

**Erwartetes Ergebnis**:
- Kein Crash, kein Memory-Leak
- Alle Timer-Zyklen laufen korrekt durch

---

## Erweiterbarkeit (zukünftige Versionen)

### Bidirektionale Kommunikation (Empfänger → Sender)

**Use Case**: Empfänger sendet Timer-Status zurück an Sender für lokales Display.

**Protokoll-Erweiterung**:
```
┌─────────────────────────────────────────────────────┐
│  Byte 0   │  Byte 1   │  Byte 2-3  │  Byte 4      │
│  Command  │  State    │  Remaining │  Checksum    │
│  (uint8)  │  (uint8)  │  (uint16)  │  (uint8)     │
└─────────────────────────────────────────────────────┘
  5 Bytes Total
```

**Rationale**: Sender könnte Timer-Status anzeigen (optional für v2.0).

### Mehrere Empfänger (Broadcast)

**Use Case**: Ein Sender steuert mehrere Anzeigen auf großem Platz.

**Protokoll-Erweiterung**: Empfänger auf gleicher Pipe, alle empfangen gleiche Befehle.

**Rationale**: Einfache Broadcast-Architektur, keine Änderung am Protokoll nötig.

---

## Compliance & Standards

| Standard | Compliance | Notizen |
|----------|-----------|---------|
| ISM Band 2.4 GHz | ✅ | nRF24L01+ ist zertifiziert für 2.4-2.5 GHz ISM Band |
| FCC Part 15 | ✅ | Unlicensed operation unter 0dBm (1mW) |
| CE (EU) | ✅ | nRF24L01+ Module sind CE-zertifiziert |

**Hinweis**: PCB-Design sollte CE/FCC Konformität berücksichtigen (Antenne-Placement, Shielding).

---

## Zusammenfassung

| Parameter | Wert |
|-----------|------|
| Protokoll-Version | 1.0 |
| Payload-Größe | 2 Bytes (CMD + Checksum) |
| Befehle | START (0x01), STOP (0x02) |
| Latenz | <100ms (typisch ~20-40ms) |
| Reichweite | 50m Indoor, 100m Outdoor |
| Zuverlässigkeit | >99% (mit Auto-Retry) |
| Stromverbrauch Sender | 20mA aktiv, 5mA idle |
| Stromverbrauch Empfänger | 30mA (ohne LEDs) |

---

## Nächste Schritte

✅ Phase 1 Funkprotokoll abgeschlossen
→ Fortsetzung: quickstart.md erstellen
