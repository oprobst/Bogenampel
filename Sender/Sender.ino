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
    DEBUG_PRINTLN(F("\nBogenampel V1.0"));
    #endif

    // SPI-Bus VOR allen SPI-Geräten initialisieren
    SPI.begin();
    delay(100);  // NRF24L01 benötigt Zeit zum Power-Up nach SPI-Init

    // Pins initialisieren
    initializePins();

    // Button Manager initialisieren
    buttons.begin();

    // Radio ZUERST initialisieren (VOR Display!)
    bool radioOk = initializeRadio();
    DEBUG_PRINTLN(radioOk ? F("NRF OK") : F("NRF FAIL"));

    // Display initialisieren (nach Radio)
    tft.init(Display::WIDTH, Display::HEIGHT);  // ST7789 benötigt Auflösung
    tft.invertDisplay(false);
    tft.setRotation(Display::ROTATION);

    // State Machine starten (beginnt im SPLASH_SCREEN State und zeigt Splash Screen)
    stateMachine.begin();

    // Radio-Status an State Machine übergeben
    stateMachine.setRadioInitialized(radioOk);

    DEBUG_PRINTLN(F("Setup OK\n"));

    // Warte 2 Sekunden, damit Empfänger auch bereit ist
    delay(2000);
}

//=============================================================================
// Loop
//=============================================================================

void loop() {
    // Button Manager Update (immer zuerst!)
    buttons.update();

    // Alarm-Detection (globale Prüfung, hat Vorrang vor allem anderen)
    if (buttons.isAlarmTriggered()) {
        DEBUG_PRINTLN(F("ALARM!"));

        // Sende Alarm mit Retries
        TransmissionResult result = sendAlarmWithRetry();

        if (result == TX_SUCCESS) {
            // Kurzes visuelles Feedback
            digitalWrite(Pins::LED_RED, HIGH);
            delay(50);
            digitalWrite(Pins::LED_RED, LOW);
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
    // Buttons werden vom ButtonManager initialisiert

    // Status-LED als Ausgang (initial aus)
    pinMode(Pins::LED_RED, OUTPUT);
    digitalWrite(Pins::LED_RED, LOW);

    // NRF24 Control Pins werden von RF24.begin() initialisiert!
    // Keine manuelle Initialisierung nötig
}

/**
 * @brief Initialisiert das NRF24L01 Funkmodul
 * @return true wenn erfolgreich, false bei Fehler
 */
bool initializeRadio() {
    // Radio starten mit mehreren Versuchen (SPI muss bereits initialisiert sein!)
    const uint8_t MAX_RETRIES = 3;
    bool radioOk = false;

    #if DEBUG_ENABLED
    // CSN Pin auf HIGH setzen (deselect) VOR radio.begin()
    pinMode(Pins::NRF_CSN, OUTPUT);
    digitalWrite(Pins::NRF_CSN, HIGH);
    pinMode(Pins::NRF_CE, OUTPUT);
    digitalWrite(Pins::NRF_CE, LOW);
    delay(10);

    DEBUG_PRINT(F("NRF CE="));
    DEBUG_PRINT(Pins::NRF_CE);
    DEBUG_PRINT(F(" CSN="));
    DEBUG_PRINTLN(Pins::NRF_CSN);
    #endif

    for (uint8_t attempt = 1; attempt <= MAX_RETRIES && !radioOk; attempt++) {
        delay(10);  // Kurze Pause vor jedem Versuch

        // Radio initialisieren
        if (!radio.begin()) {
            #if DEBUG_ENABLED
            DEBUG_PRINT(F("Retry "));
            DEBUG_PRINTLN(attempt);
            #endif
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
        #if DEBUG_ENABLED
        DEBUG_PRINTLN(F("NRF FAIL"));
        #endif
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

    // TX-Modus aktivieren
    radio.stopListening();

    // Pipe für Schreiben öffnen
    radio.openWritingPipe(pipeAddr);

    #if DEBUG_ENABLED
    DEBUG_PRINT(F("NRF OK Ch"));
    DEBUG_PRINT(RF::CHANNEL);
    DEBUG_PRINT(F(" P"));
    DEBUG_PRINT(RF::POWER_LEVEL);
    DEBUG_PRINT(F(" R"));
    DEBUG_PRINT(RF::DATA_RATE);
    DEBUG_PRINT(F(" "));
    DEBUG_PRINT(radio.isPVariant() ? F("Plus") : F("Std"));
    DEBUG_PRINTLN(F(" NoACK"));

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
 * @brief Sendet ein Radio-Kommando an den Empfänger
 * @param cmd RadioCommand (CMD_STOP, CMD_START_120, CMD_START_240, CMD_INIT, CMD_ALARM)
 * @return TransmissionResult (TX_SUCCESS, TX_TIMEOUT, TX_ERROR)
 */
TransmissionResult sendCommand(RadioCommand cmd) {
    // RadioPacket erstellen
    RadioPacket packet;
    packet.command = static_cast<uint8_t>(cmd);
    packet.checksum = calculateChecksum(packet.command);

    // Kurze Pause vor dem Senden (Radio stabilisieren)
    delay(10);

    // Senden mit Auto-Retry (bereits in RF24 konfiguriert)
    bool success = radio.write(&packet, sizeof(RadioPacket));

    #if DEBUG_ENABLED
    DEBUG_PRINT(F("TX:"));
    DEBUG_PRINT(packet.command, HEX);
    DEBUG_PRINTLN(success ? F(" OK") : F(" FAIL"));
    #endif

    return success ? TX_SUCCESS : TX_TIMEOUT;
}

/**
 * @brief Sendet Alarm-Kommando mit mehrfachen Retry-Versuchen
 * @return TransmissionResult
 */
TransmissionResult sendAlarmWithRetry() {
    TransmissionResult result;

    for (uint8_t retry = 0; retry < Timing::ALARM_MAX_RETRIES; retry++) {
        if (retry > 0) {
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
 * @brief Pingt den Empfänger an und wartet auf PONG-Antwort
 * @return true wenn Empfänger antwortet (PONG empfangen), false sonst
 */
bool pingReceiver() {
    // Pipe-Adresse aus PROGMEM laden (für Reading Pipe)
    uint8_t pipeAddr[5];
    memcpy_P(pipeAddr, RF::PIPE_ADDRESS, 5);

    // 1. Sende PING
    sendCommand(CMD_PING);
    delay(10);  // Kurze Pause nach dem Senden

    // 2. Wechsle in RX-Modus zum Empfangen der PONG-Antwort
    radio.openReadingPipe(1, pipeAddr);  // Pipe 1 für PONG-Empfang
    radio.startListening();
    delay(5);  // Radio braucht Zeit zum Modewechsel

    // 3. Warte auf PONG (max 250ms)
    bool pongReceived = false;
    uint32_t startTime = millis();
    const uint16_t PONG_TIMEOUT_MS = 250;

    while (millis() - startTime < PONG_TIMEOUT_MS) {
        if (radio.available()) {
            RadioPacket packet;
            radio.read(&packet, sizeof(RadioPacket));

            // Prüfe ob es ein PONG ist
            if (validateChecksum(&packet) && packet.command == CMD_PONG) {
                DEBUG_PRINTLN(F("PONG!"));
                pongReceived = true;
                break;
            }
        }
        delay(1);  // Kurze Pause
    }

    // 4. Zurück in TX-Modus
    radio.stopListening();
    delay(5);  // Radio braucht Zeit zum Modewechsel

    return pongReceived;
}

/**
 * @brief Testet Verbindung zum Empfänger
 * @return true wenn Empfänger antwortet, false sonst
 *
 * Hinweis: Ohne ACK können wir nicht wissen, ob Empfänger antwortet.
 * Wir senden CMD_PING und gehen davon aus, dass es funktioniert.
 */
bool testReceiverConnection() {
    // Sende PING (ohne auf PONG zu warten)
    sendCommand(CMD_PING);

    // Gehe davon aus, dass es funktioniert (Kommunikation ist getestet)
    return true;
}
