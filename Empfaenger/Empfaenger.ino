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

//=============================================================================
// Setup
//=============================================================================

void setup() {
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

        DEBUG_PRINT(F("Paket empfangen: 0x"));
        DEBUG_PRINT(packet.command, HEX);
        DEBUG_PRINT(F(" (Checksum: 0x"));
        DEBUG_PRINT(packet.checksum, HEX);
        DEBUG_PRINTLN(F(")"));

        // Validiere Checksum
        if (validateChecksum(&packet)) {
            DEBUG_PRINTLN(F("Checksum OK"));

            // Gelbe LED blinken lassen (Empfangsbestätigung)
            blinkYellowLED();

            // Kommando verarbeiten
            handleCommand(static_cast<RadioCommand>(packet.command));
        } else {
            DEBUG_PRINTLN(F("FEHLER: Ungültige Checksum!"));
        }
    }

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
    // Radio starten (SPI muss bereits initialisiert sein!)
    if (!radio.begin()) {
        return false;  // Hardware nicht gefunden
    }

    // Pipe-Adresse aus PROGMEM laden
    uint8_t pipeAddr[5];
    memcpy_P(pipeAddr, RF::PIPE_ADDRESS, 5);

    // Radio konfigurieren
    radio.setPALevel(RF::POWER_LEVEL);
    radio.setDataRate(RF::DATA_RATE);
    radio.setChannel(RF::CHANNEL);
    radio.setPayloadSize(sizeof(RadioPacket));

    // Auto-ACK aktivieren
    radio.setAutoAck(RF::AUTO_ACK_ENABLED);

    // Pipe für Lesen öffnen (Pipe 0)
    radio.openReadingPipe(0, pipeAddr);

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
 * @brief Verarbeitet empfangenes Kommando
 * @param cmd RadioCommand
 */
void handleCommand(RadioCommand cmd) {
    DEBUG_PRINT(F("Verarbeite Kommando: "));
    DEBUG_PRINTLN(commandToString(cmd));

    switch (cmd) {
        case CMD_INIT:
            // System initialisieren
            DEBUG_PRINTLN(F("→ System initialisiert"));
            systemInitialized = true;

            // Grüne LED an (Bereit)
            digitalWrite(Pins::LED_GREEN, HIGH);
            digitalWrite(Pins::LED_RED, LOW);
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
