/**
 * @file Sender.ino
 * @brief Hauptdatei für Bogenampel Sender (Bedieneinheit)
 */

#include "Config.h"
#include "Commands.h"

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <RF24.h>

#include "StateMachine.h"
#include "ButtonManager.h"

//=============================================================================
// Globale Instanzen
//=============================================================================

// Globale Instanzen müssen in der richtigen Reihenfolge erstellt werden
ButtonManager buttons;
Adafruit_ST7789 tft = Adafruit_ST7789(Pins::TFT_CS, Pins::TFT_DC, Pins::TFT_RST);
RF24 radio(Pins::NRF_CE, Pins::NRF_CSN);
StateMachine stateMachine(tft, buttons);

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
    DEBUG_PRINTLN(F("  Bogenampel Sender V1.0"));
    DEBUG_PRINTLN(F("======================================"));
    DEBUG_PRINT(F("Build: "));
    DEBUG_PRINT(F(__DATE__));
    DEBUG_PRINT(F(" "));
    DEBUG_PRINTLN(F(__TIME__));
    DEBUG_PRINTLN(F(""));
    #endif

    // SPI-Bus VOR allen SPI-Geräten initialisieren
    SPI.begin();
    DEBUG_PRINTLN(F("SPI-Bus initialisiert"));

    // Pins initialisieren
    initializePins();

    // Button Manager initialisieren
    buttons.begin();

    // Radio ZUERST initialisieren (VOR Display!)
    DEBUG_PRINTLN(F("Initialisiere NRF24L01..."));
    if (!initializeRadio()) {
        DEBUG_PRINTLN(F("FEHLER: NRF24L01 nicht gefunden!"));
        // TODO: Fehleranzeige auf Display
    } else {
        DEBUG_PRINTLN(F("NRF24L01 initialisiert"));
    }

    // Display initialisieren (nach Radio)
    DEBUG_PRINTLN(F("Initialisiere Display..."));
    tft.init(Display::WIDTH, Display::HEIGHT);  // ST7789 benötigt Auflösung
    tft.invertDisplay(false);
    tft.setRotation(Display::ROTATION);
    DEBUG_PRINTLN(F("Display initialisiert"));

    // State Machine starten (beginnt im SPLASH_SCREEN State und zeigt Splash Screen)
    stateMachine.begin();

    DEBUG_PRINTLN(F("Setup abgeschlossen\n"));
}

//=============================================================================
// Loop
//=============================================================================

void loop() {
    // Button Manager Update (immer zuerst!)
    buttons.update();

    // Alarm-Detection (globale Prüfung, hat Vorrang vor allem anderen)
    if (buttons.isAlarmTriggered()) {
        DEBUG_PRINTLN(F("ALARM ausgelöst! Sende CMD_ALARM..."));

        // Sende Alarm mit Retries
        TransmissionResult result = sendAlarmWithRetry();

        if (result == TX_SUCCESS) {
            DEBUG_PRINTLN(F("Alarm erfolgreich gesendet"));
            // Kurzes visuelles Feedback
            digitalWrite(Pins::LED_RED, HIGH);
            delay(50);
            digitalWrite(Pins::LED_RED, LOW);
        } else {
            DEBUG_PRINTLN(F("FEHLER: Alarm konnte nicht gesendet werden!"));
            // TODO: Fehleranzeige auf Display
        }

        // Kein State-Wechsel, bleibe im aktuellen State
    }

    // State Machine Update (verwaltet alle States inkl. Splash Screen)
    stateMachine.update();

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

    // Buttons werden vom ButtonManager initialisiert

    // Status-LED als Ausgang (initial aus)
    pinMode(Pins::LED_RED, OUTPUT);
    digitalWrite(Pins::LED_RED, LOW);

    // NRF24 Control Pins werden von RF24.begin() initialisiert!
    // Keine manuelle Initialisierung nötig

    DEBUG_PRINTLN(F("Pins initialisiert"));
}

/**
 * @brief Initialisiert das NRF24L01 Funkmodul
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
    radio.setRetries(RF::RETRY_DELAY, RF::RETRY_COUNT);
    radio.setPayloadSize(sizeof(RadioPacket));

    // Auto-ACK aktivieren
    radio.setAutoAck(true);

    // Pipe für Schreiben öffnen
    radio.openWritingPipe(pipeAddr);

    // Pipe 0 für ACK-Empfang öffnen (WICHTIG für Auto-ACK!)
    radio.openReadingPipe(0, pipeAddr);

    // TX-Modus aktivieren
    radio.stopListening();

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
 * @brief Sendet ein Radio-Kommando an den Empfänger
 * @param cmd RadioCommand (CMD_STOP, CMD_START_120, CMD_START_240, CMD_INIT, CMD_ALARM)
 * @return TransmissionResult (TX_SUCCESS, TX_TIMEOUT, TX_ERROR)
 */
TransmissionResult sendCommand(RadioCommand cmd) {
    // RadioPacket erstellen
    RadioPacket packet;
    packet.command = static_cast<uint8_t>(cmd);
    packet.checksum = calculateChecksum(packet.command);

    #if DEBUG_ENABLED
    DEBUG_PRINT(F("Sende Kommando: "));
    DEBUG_PRINT(commandToString(cmd));
    DEBUG_PRINT(F(" (0x"));
    DEBUG_PRINT(packet.command, HEX);
    DEBUG_PRINTLN(F(")"));
    #endif

    // Senden mit Auto-Retry (bereits in RF24 konfiguriert)
    bool success = radio.write(&packet, sizeof(RadioPacket));

    if (success) {
        DEBUG_PRINTLN(F("  → ACK empfangen"));
        return TX_SUCCESS;
    } else {
        DEBUG_PRINTLN(F("  → Timeout (kein ACK)"));
        return TX_TIMEOUT;
    }
}

/**
 * @brief Sendet Alarm-Kommando mit mehrfachen Retry-Versuchen
 * @return TransmissionResult
 */
TransmissionResult sendAlarmWithRetry() {
    TransmissionResult result;

    for (uint8_t retry = 0; retry < Timing::ALARM_MAX_RETRIES; retry++) {
        if (retry > 0) {
            DEBUG_PRINT(F("  Retry "));
            DEBUG_PRINT(retry);
            DEBUG_PRINT(F("/"));
            DEBUG_PRINTLN(Timing::ALARM_MAX_RETRIES - 1);
            delay(Timing::ALARM_RETRY_DELAY_MS);
        }

        result = sendCommand(CMD_ALARM);

        if (result == TX_SUCCESS) {
            return TX_SUCCESS;
        }
    }

    // Alle Retries fehlgeschlagen
    return result;
}

/**
 * @brief Testet Verbindung zum Empfänger
 * @return true wenn Empfänger antwortet, false sonst
 *
 * Hinweis: Sendet ein CMD_INIT und wartet auf ACK.
 * Ohne Empfänger wird dies immer false zurückgeben.
 */
bool testReceiverConnection() {
    DEBUG_PRINTLN(F("Teste Empfaenger-Verbindung..."));

    // Versuche CMD_INIT zu senden (kurzer Timeout)
    TransmissionResult result = sendCommand(CMD_INIT);

    if (result == TX_SUCCESS) {
        DEBUG_PRINTLN(F("Empfaenger gefunden!"));
        return true;
    } else {
        DEBUG_PRINTLN(F("Kein Empfaenger gefunden"));
        return false;
    }
}
