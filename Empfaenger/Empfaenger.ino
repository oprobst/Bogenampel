/**
 * @file Empfaenger.ino
 * @brief Hauptdatei für Bogenampel Empfänger (Anzeigeeinheit)
 *
 * Empfängt Kommandos vom Sender via NRF24L01 und steuert:
 * - 3x Status-LEDs (Grün, Gelb, Rot)
 * - (Später: LED Strip, Buzzer)
 */

#include "Config.h"
#include "Commands.h"

#include <SPI.h>
#include <RF24.h>

//=============================================================================
// Globale Instanzen
//=============================================================================

RF24 radio(Pins::NRF_CE, Pins::NRF_CSN);

// State-Variablen
bool systemInitialized = false;   // Wurde CMD_INIT empfangen?
uint32_t lastBlinkTime = 0;        // Für LED-Blink-Timer
uint8_t lastButtonReading = HIGH;  // Letzter gelesener Pin-Zustand
uint8_t buttonState = HIGH;        // Stabiler Button-Zustand nach Debouncing
uint32_t lastDebounceTime = 0;     // Zeitpunkt der letzten Button-Änderung

//=============================================================================
// Setup
//=============================================================================

void setup() {
    delay(1000);
    // Serial für Debugging
    #if DEBUG_ENABLED
    Serial.begin(System::SERIAL_BAUD);
    while (!Serial && millis() < 2000);
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("======================================"));
    DEBUG_PRINTLN(F("  Bogenampel Empfaenger V1.0"));
    DEBUG_PRINTLN(F("======================================"));
    DEBUG_PRINT(F("Build: "));
    DEBUG_PRINT(F(__DATE__));
    DEBUG_PRINT(F(" "));
    DEBUG_PRINTLN(F(__TIME__));
    DEBUG_PRINTLN(F(""));
    #endif

    // SPI-Bus initialisieren
    SPI.begin();
    delay(100);  // NRF24L01 benötigt Zeit zum Power-Up nach SPI-Init
    DEBUG_PRINTLN(F("SPI-Bus initialisiert"));

    // Pins initialisieren
    initializePins();

    // Radio initialisieren
    DEBUG_PRINTLN(F("Initialisiere NRF24L01..."));
    if (!initializeRadio()) {
        DEBUG_PRINTLN(F("FEHLER: NRF24L01 nicht gefunden!"));
        // Fehler-Anzeige: Rote LED blinkt schnell
        while (true) {
            digitalWrite(Pins::LED_RED, HIGH);
            delay(100);
            digitalWrite(Pins::LED_RED, LOW);
            delay(100);
        }
    } else {
        DEBUG_PRINTLN(F("NRF24L01 initialisiert"));
    }

    // Start-Sequenz: Alle LEDs kurz blinken
    startupSequence();

    DEBUG_PRINTLN(F("Setup abgeschlossen\n"));
    DEBUG_PRINTLN(F("Warte auf Kommandos vom Sender..."));
}

//=============================================================================
// Loop
//=============================================================================

void loop() {
    // Prüfe ob Daten verfügbar
    if (radio.available()) {
        // Empfange RadioPacket
        RadioPacket packet;
        radio.read(&packet, sizeof(RadioPacket));

        DEBUG_PRINT(F("RX:"));
        DEBUG_PRINTLN(packet.command, HEX);

        // Validiere Checksum
        if (validateChecksum(&packet)) {
            // Gelbe LED blinken lassen (Empfangsbestätigung)
            blinkYellowLED();

            // Kommando verarbeiten
            handleCommand(static_cast<RadioCommand>(packet.command));
        } else {
            DEBUG_PRINTLN(F("BAD CRC"));
        }
    }

    // Prüfe Debug-Button
    checkButton();

    // Kleine Pause um CPU zu entlasten
    delay(10);
}

//=============================================================================
// Hilfsfunktionen
//=============================================================================

/**
 * @brief Initialisiert alle GPIO-Pins
 */
void initializePins() {
    DEBUG_PRINTLN(F("Initialisiere Pins..."));

    // LEDs als Ausgänge (initial aus)
    pinMode(Pins::LED_GREEN, OUTPUT);
    pinMode(Pins::LED_YELLOW, OUTPUT);
    pinMode(Pins::LED_RED, OUTPUT);
    digitalWrite(Pins::LED_GREEN, LOW);
    digitalWrite(Pins::LED_YELLOW, LOW);
    digitalWrite(Pins::LED_RED, LOW);

    // Buzzer als Ausgang (initial aus)
    pinMode(Pins::BUZZER, OUTPUT);
    digitalWrite(Pins::BUZZER, LOW);

    // Debug-Taster als Eingang mit Pull-Up
    pinMode(Pins::BTN_DEBUG, INPUT_PULLUP);

    // NRF24 Control Pins werden von RF24.begin() initialisiert!
    // Keine manuelle Initialisierung nötig

    DEBUG_PRINTLN(F("Pins initialisiert"));
}

/**
 * @brief Initialisiert das NRF24L01 Funkmodul als Empfänger
 * @return true wenn erfolgreich, false bei Fehler
 */
bool initializeRadio() {
    // Radio starten mit mehreren Versuchen (SPI muss bereits initialisiert sein!)
    const uint8_t MAX_RETRIES = 3;
    bool radioOk = false;

    // CSN Pin auf HIGH setzen (deselect) VOR radio.begin()
    pinMode(Pins::NRF_CSN, OUTPUT);
    digitalWrite(Pins::NRF_CSN, HIGH);
    pinMode(Pins::NRF_CE, OUTPUT);
    digitalWrite(Pins::NRF_CE, LOW);
    delay(10);

    for (uint8_t attempt = 1; attempt <= MAX_RETRIES && !radioOk; attempt++) {
        delay(10);  // Kurze Pause vor jedem Versuch

        // Radio initialisieren
        if (!radio.begin()) {
            continue;
        }

        // Chip-Verbindung prüfen (robuster als nur radio.begin())
        delay(5);  // Kurze Pause nach begin()
        if (!radio.isChipConnected()) {
            continue;
        }

        radioOk = true;
    }

    if (!radioOk) {
        return false;  // Hardware nicht gefunden
    }

    // Pipe-Adresse aus PROGMEM laden
    uint8_t pipeAddr[5];
    memcpy_P(pipeAddr, RF::PIPE_ADDRESS, 5);

    // Pipe-Adresse ausgeben (Debug)
    #if DEBUG_ENABLED
    DEBUG_PRINT(F("Pipe: "));
    for (uint8_t i = 0; i < 5; i++) {
        DEBUG_PRINT((char)pipeAddr[i]);
    }
    DEBUG_PRINTLN();
    #endif

    // Radio konfigurieren
    radio.setPALevel(RF::POWER_LEVEL);
    radio.setDataRate(RF::DATA_RATE);
    radio.setChannel(RF::CHANNEL);
    radio.setPayloadSize(sizeof(RadioPacket));

    // Auto-ACK DEAKTIVIERT (Kommunikation funktioniert ohne ACK)
    radio.setAutoAck(false);

    // Pipe für Lesen öffnen (Pipe 1 statt 0 - Pipe 0 wird oft für ACK verwendet)
    radio.openReadingPipe(1, pipeAddr);

    // RX-Modus aktivieren (Empfangsmodus)
    radio.startListening();

    #if DEBUG_ENABLED
    DEBUG_PRINTLN(F("NRF24 Konfiguration:"));
    DEBUG_PRINT(F("  Kanal: "));
    DEBUG_PRINTLN(RF::CHANNEL);
    DEBUG_PRINT(F("  Power: "));
    DEBUG_PRINTLN(static_cast<int>(RF::POWER_LEVEL));
    DEBUG_PRINT(F("  Rate: "));
    DEBUG_PRINTLN(static_cast<int>(RF::DATA_RATE));
    DEBUG_PRINT(F("  Payload: "));
    DEBUG_PRINTLN(sizeof(RadioPacket));

    // Manuelle Radio-Diagnose
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Radio Status:"));
    DEBUG_PRINT(F("  isPVariant: "));
    DEBUG_PRINTLN(radio.isPVariant() ? F("Ja (Plus)") : F("Nein"));
    DEBUG_PRINT(F("  getChannel: "));
    DEBUG_PRINTLN(radio.getChannel());
    DEBUG_PRINT(F("  getPALevel: "));
    DEBUG_PRINTLN(radio.getPALevel());
    DEBUG_PRINT(F("  getDataRate: "));
    DEBUG_PRINTLN(radio.getDataRate());
    DEBUG_PRINT(F("  Listening: "));
    DEBUG_PRINTLN(radio.isChipConnected() ? F("Chip OK") : F("Chip FEHLER!"));
    DEBUG_PRINTLN(F(""));

    // Komplette Radio-Details ausgeben
    DEBUG_PRINTLN(F("=== Radio Details ==="));
    Serial.flush();
    delay(100);
    radio.printDetails();
    delay(100);
    Serial.flush();
    DEBUG_PRINTLN(F("====================="));
    #endif

    return true;
}

/**
 * @brief Start-Sequenz: Alle LEDs kurz aufblinken
 */
void startupSequence() {
    DEBUG_PRINTLN(F("Start-Sequenz..."));

    // Grün → Gelb → Rot
    digitalWrite(Pins::LED_GREEN, HIGH);
    delay(200);
    digitalWrite(Pins::LED_GREEN, LOW);

    digitalWrite(Pins::LED_YELLOW, HIGH);
    delay(200);
    digitalWrite(Pins::LED_YELLOW, LOW);

    digitalWrite(Pins::LED_RED, HIGH);
    delay(200);
    digitalWrite(Pins::LED_RED, LOW);

    delay(200);

    // Alle gleichzeitig
    digitalWrite(Pins::LED_GREEN, HIGH);
    digitalWrite(Pins::LED_YELLOW, HIGH);
    digitalWrite(Pins::LED_RED, HIGH);
    delay(300);
    digitalWrite(Pins::LED_GREEN, LOW);
    digitalWrite(Pins::LED_YELLOW, LOW);
    digitalWrite(Pins::LED_RED, LOW);
}

/**
 * @brief Lässt gelbe LED kurz aufblinken (Empfangsbestätigung)
 */
void blinkYellowLED() {
    digitalWrite(Pins::LED_YELLOW, HIGH);
    delay(Timing::LED_BLINK_DURATION_MS);
    digitalWrite(Pins::LED_YELLOW, LOW);
}

/**
 * @brief Lässt den Buzzer einen kurzen Piepton ausgeben
 */
void buzzerBeep() {
    tone(Pins::BUZZER, Timing::BUZZER_FREQUENCY_HZ);
    delay(Timing::BUZZER_BEEP_DURATION_MS);
    noTone(Pins::BUZZER);
}

/**
 * @brief Prüft Debug-Button mit Debouncing und löst Buzzer aus
 */
void checkButton() {
    // Aktuellen Button-Zustand lesen (LOW = gedrückt, wegen Pull-Up)
    uint8_t reading = digitalRead(Pins::BTN_DEBUG);

    // Wenn sich der gelesene Zustand geändert hat, Debounce-Timer zurücksetzen
    if (reading != lastButtonReading) {
        lastDebounceTime = millis();
    }

    // Letzten gelesenen Zustand speichern
    lastButtonReading = reading;

    // Wenn genug Zeit vergangen ist (Debounce-Zeit), Zustand akzeptieren
    if ((millis() - lastDebounceTime) > Timing::DEBOUNCE_MS) {
        // Wenn sich der stabile Zustand geändert hat
        if (reading != buttonState) {
            buttonState = reading;

            // Wenn Button gedrückt wurde (neuer stabiler Zustand = LOW)
            if (buttonState == LOW) {
                DEBUG_PRINTLN(F("Button gedrückt - Buzzer aktiv"));

                // Buzzer-Signal ausgeben
                buzzerBeep();
            }
        }
    }
}

/**
 * @brief Sendet PONG-Antwort zurück an Sender
 */
void sendPong() {
    // Pipe-Adresse aus PROGMEM laden
    uint8_t pipeAddr[5];
    memcpy_P(pipeAddr, RF::PIPE_ADDRESS, 5);

    // Wechsle in TX-Modus
    radio.stopListening();
    delay(5);  // Radio braucht Zeit

    // Writing Pipe öffnen
    radio.openWritingPipe(pipeAddr);

    // RadioPacket erstellen
    RadioPacket packet;
    packet.command = CMD_PONG;
    packet.checksum = calculateChecksum(packet.command);

    // PONG senden
    radio.write(&packet, sizeof(RadioPacket));

    // Zurück in RX-Modus
    delay(5);  // Radio braucht Zeit
    radio.startListening();
}

/**
 * @brief Verarbeitet empfangenes Kommando
 * @param cmd RadioCommand
 */
void handleCommand(RadioCommand cmd) {
    switch (cmd) {
        case CMD_PING:
            // Sender fragt: Bist du da?
            DEBUG_PRINTLN(F("PING->PONG"));
            sendPong();  // Antworte mit PONG
            break;

        case CMD_INIT:
            // System initialisieren
            DEBUG_PRINTLN(F("INIT"));
            systemInitialized = true;

            // Alle LEDs 3x blinken lassen (Bestätigung)
            for (int i = 0; i < 3; i++) {
                digitalWrite(Pins::LED_GREEN, HIGH);
                digitalWrite(Pins::LED_YELLOW, HIGH);
                digitalWrite(Pins::LED_RED, HIGH);
                delay(150);
                digitalWrite(Pins::LED_GREEN, LOW);
                digitalWrite(Pins::LED_YELLOW, LOW);
                digitalWrite(Pins::LED_RED, LOW);
                delay(150);
            }

            // Grüne LED bleibt an (Bereit)
            digitalWrite(Pins::LED_GREEN, HIGH);
            break;

        case CMD_START_120:
        case CMD_START_240:
            // Timer starten
            if (systemInitialized) {
                DEBUG_PRINT(F("→ Timer gestartet: "));
                DEBUG_PRINT(cmd == CMD_START_120 ? 120 : 240);
                DEBUG_PRINTLN(F("s"));

                // Grüne LED an (Aktiv)
                digitalWrite(Pins::LED_GREEN, HIGH);
                digitalWrite(Pins::LED_RED, LOW);

                // TODO: Timer-Logik implementieren
            } else {
                DEBUG_PRINTLN(F("Warnung: System nicht initialisiert!"));
            }
            break;

        case CMD_STOP:
            // Timer stoppen
            DEBUG_PRINTLN(F("→ Timer gestoppt"));

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_RED, HIGH);

            // TODO: Timer-Logik stoppen
            break;

        case CMD_ALARM:
            // Alarm auslösen
            DEBUG_PRINTLN(F("→ ALARM ausgelöst!"));

            // Alle LEDs blinken schnell
            for (int i = 0; i < 5; i++) {
                digitalWrite(Pins::LED_GREEN, HIGH);
                digitalWrite(Pins::LED_YELLOW, HIGH);
                digitalWrite(Pins::LED_RED, HIGH);
                delay(100);
                digitalWrite(Pins::LED_GREEN, LOW);
                digitalWrite(Pins::LED_YELLOW, LOW);
                digitalWrite(Pins::LED_RED, LOW);
                delay(100);
            }

            // Rote LED bleibt an
            digitalWrite(Pins::LED_RED, HIGH);
            break;

        default:
            DEBUG_PRINTLN(F("Warnung: Unbekanntes Kommando!"));
            break;
    }
}
