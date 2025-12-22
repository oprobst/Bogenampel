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
// Timer1 für Interrupt-basierte 1-Sekunden-Ticks (wie Empfänger)
//=============================================================================

volatile bool senderSecondTick = false;     // Flag: Sekunde vergangen?

/**
 * @brief Timer1 Compare Match A Interrupt - wird jede Sekunde ausgelöst
 *
 * Diese ISR wird exakt jede Sekunde aufgerufen (identisch zum Empfänger).
 */
ISR(TIMER1_COMPA_vect) {
    senderSecondTick = true;
}

//=============================================================================
// Setup
//=============================================================================

void setup() {
    // Serial für Debugging
    #if DEBUG_ENABLED
    Serial.begin(System::SERIAL_BAUD);
    while (!Serial && millis() < 2000);
    #endif

    // SPI-Bus VOR allen SPI-Geräten initialisieren
    SPI.begin();
    delay(100);  // NRF24L01 benötigt Zeit zum Power-Up nach SPI-Init

    // Pins initialisieren
    initializePins();

    // Timer1 für Sekunden-Ticks initialisieren (MUSS VOR State Machine starten!)
    setupTimer1();

    // Button Manager initialisieren
    buttons.begin();
    buttons.initBuzzer();  // Buzzer für Tastentöne initialisieren

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
 * @brief Konfiguriert Timer1 für exakte 1-Sekunden-Interrupts (identisch zum Empfänger)
 *
 * Timer1 ist ein 16-Bit-Timer auf dem ATmega328P.
 * Mit Prescaler 256 und einem Compare-Wert von 62499:
 * - F_CPU = 16 MHz
 * - Prescaler = 256
 * - Timer-Takt = 16 MHz / 256 = 62.5 kHz
 * - Für 1 Hz: 62.5 kHz / 62500 = 1 Hz (exakt 1 Sekunde)
 */
void setupTimer1() {
    cli();  // Interrupts temporär deaktivieren

    // Timer1 zurücksetzen
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1 = 0;

    // Compare-Wert setzen für 1 Hz (1 Sekunde)
    // OCR1A = (F_CPU / (Prescaler * gewünschte Frequenz)) - 1
    // OCR1A = (16000000 / (256 * 1)) - 1 = 62499
    OCR1A = 62499;

    // CTC-Modus (Clear Timer on Compare Match)
    TCCR1B |= (1 << WGM12);

    // Prescaler 256
    TCCR1B |= (1 << CS12);

    // Timer1 Compare Match A Interrupt aktivieren
    TIMSK1 |= (1 << OCIE1A);

    sei();  // Interrupts wieder aktivieren

    DEBUG_PRINTLN(F("Timer1 konfiguriert (1s Ticks)"));
}

/**
 * @brief Setzt den Sender-Timer zurück (für synchronen Start)
 */
void resetSenderTimer() {
    cli();  // Interrupts kurz deaktivieren
    TCNT1 = 0;  // Timer1 Counter zurücksetzen
    senderSecondTick = false;
    sei();
}

/**
 * @brief Initialisiert das NRF24L01 Funkmodul
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

    // Radio konfigurieren
    radio.setPALevel(RF::POWER_LEVEL);
    radio.setDataRate(RF::DATA_RATE);
    radio.setChannel(RF::CHANNEL);
    radio.setPayloadSize(sizeof(RadioPacket));

    // Auto-ACK AKTIVIERT für Verbindungskontrolle
    radio.setAutoAck(RF::AUTO_ACK_ENABLED);
    radio.setRetries(RF::RETRY_DELAY, RF::RETRY_COUNT);

    // TX-Modus aktivieren
    radio.stopListening();

    // Pipe für Schreiben öffnen
    radio.openWritingPipe(pipeAddr);

    #if DEBUG_ENABLED
    DEBUG_PRINT(F("NRF Ch"));
    DEBUG_PRINT(RF::CHANNEL);
    DEBUG_PRINT(F(" ACK"));
    DEBUG_PRINTLN(radio.isPVariant() ? F("+") : F(""));
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

    // Senden mit Auto-Retry und ACK-Prüfung
    bool success = radio.write(&packet, sizeof(RadioPacket));

    #if DEBUG_ENABLED
    DEBUG_PRINT(F("TX:"));
    DEBUG_PRINTLN(success ? F("OK") : F("FAIL"));
    #endif

    // success == true bedeutet: ACK empfangen
    // success == false bedeutet: Kein ACK nach allen Retries
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
 * @brief Testet Verbindung zum Empfänger mit einem einzelnen PING
 * @return true wenn ACK empfangen wurde, false sonst
 */
bool testReceiverConnection() {
    TransmissionResult result = sendCommand(CMD_PING);
    return (result == TX_SUCCESS);
}

/**
 * @brief Testet Verbindungsqualität mit 10 Pings in 5 Sekunden
 * @return Erfolgsrate in Prozent (0-100)
 *
 * Sendet 10 PING-Kommandos im Abstand von 500ms und zählt erfolgreiche ACKs.
 * Die Rückgabe ist die Erfolgsrate in Prozent (0% = alle fehlgeschlagen, 100% = alle erfolgreich).
 */
uint8_t testConnectionQuality() {
    uint8_t successCount = 0;
    const uint8_t totalPings = RF::QUALITY_TEST_PINGS;

    for (uint8_t i = 0; i < totalPings; i++) {
        // Sende PING und prüfe ACK
        TransmissionResult result = sendCommand(CMD_PING);

        if (result == TX_SUCCESS) {
            successCount++;
        }

        // Warte 500ms bis zum nächsten Ping (außer beim letzten)
        if (i < totalPings - 1) {
            delay(RF::QUALITY_TEST_INTERVAL_MS);
        }
    }

    // Erfolgsrate berechnen (0-100%)
    uint8_t qualityPercent = (successCount * 100) / totalPings;

    return qualityPercent;
}

/**
 * @brief Misst die Batteriespannung
 * @return Spannung in Millivolt (z.B. 7200 für 7.2V)
 */
uint16_t readBatteryVoltage() {
    // Einfache ADC-Lesung ohne delays (delay() kann zu Problemen führen)
    uint16_t adcValue = analogRead(Pins::VOLTAGE_SENSE);

    // ADC-Wert in Spannung umrechnen (ohne Float!)
    // Vbat_mV = (ADC / 1023) * 5000mV * 2.0 = (ADC * 10000) / 1023
    uint16_t voltageMillivolts = ((uint32_t)adcValue * 10000UL) / Battery::ADC_MAX;

    return voltageMillivolts;
}

/**
 * @brief Prüft ob USB angeschlossen ist
 * @return true wenn USB angeschlossen, false wenn Batteriebetrieb
 *
 * USB wird nur angezeigt, wenn die Eingangsspannung < 6V ist.
 * Das bedeutet, die externe 9V Batterie ist definitiv nicht angeschlossen.
 */
bool isUsbPowered() {
    uint16_t voltage = readBatteryVoltage();
    return (voltage < 6000);  // < 6V = USB-Betrieb (externe 9V Batterie nicht angeschlossen)
}
