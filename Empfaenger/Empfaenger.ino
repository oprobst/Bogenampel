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

// Timer-Variablen
bool timerRunning = false;         // Läuft der Timer?
uint32_t timerStartTime = 0;       // Startzeitpunkt (millis)
uint32_t timerDurationMs = 0;      // Timer-Dauer in Millisekunden
uint8_t currentGroup = 0;          // Aktuelle Gruppe (0=AB, 1=CD)

// Vorbereitungsphase
bool inPreparationPhase = false;   // Läuft die Vorbereitungsphase?
uint32_t preparationStartTime = 0; // Start der Vorbereitungsphase
uint32_t preparationDurationMs = 0; // Dauer der Vorbereitungsphase

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

    // Prüfe Vorbereitungsphase
    updatePreparation();

    // Prüfe Timer und aktualisiere LEDs
    updateTimer();

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

    // Radio konfigurieren
    radio.setPALevel(RF::POWER_LEVEL);
    radio.setDataRate(RF::DATA_RATE);
    radio.setChannel(RF::CHANNEL);
    radio.setPayloadSize(sizeof(RadioPacket));

    // Auto-ACK AKTIVIERT (Empfänger sendet automatisch ACK an Sender)
    radio.setAutoAck(RF::AUTO_ACK_ENABLED);
    radio.setRetries(RF::RETRY_DELAY, RF::RETRY_COUNT);

    // Pipe für Lesen öffnen (Pipe 1)
    radio.openReadingPipe(1, pipeAddr);

    // RX-Modus aktivieren (Empfangsmodus)
    radio.startListening();

    #if DEBUG_ENABLED
    DEBUG_PRINT(F("NRF Ch"));
    DEBUG_PRINT(RF::CHANNEL);
    DEBUG_PRINTLN(F(" RX"));
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
 * @brief Lässt den Buzzer einen kurzen Piepton ausgeben (1 Sekunde)
 */
void buzzerBeep() {
    tone(Pins::BUZZER, Timing::BUZZER_FREQUENCY_HZ);
    delay(1000);  // 1 Sekunde
    noTone(Pins::BUZZER);
}

/**
 * @brief Lässt den Buzzer mehrfach piepen
 * @param count Anzahl der Pieptöne
 *
 * Jeder Pieperton: 500ms Ton, 500ms Pause
 */
void buzzerBeepMultiple(uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        tone(Pins::BUZZER, Timing::BUZZER_FREQUENCY_HZ);
        delay(500);  // 500ms Ton
        noTone(Pins::BUZZER);

        // Pause zwischen Tönen (außer beim letzten)
        if (i < count - 1) {
            delay(500);  // 500ms Pause
        }
    }
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
 * @brief Aktualisiert Timer und LED-Status
 */
void updateTimer() {
    if (!timerRunning) return;

    // Verbleibende Zeit berechnen
    uint32_t elapsed = millis() - timerStartTime;

    if (elapsed >= timerDurationMs) {
        // Timer abgelaufen
        timerRunning = false;

        // Rote LED an (Stop)
        digitalWrite(Pins::LED_GREEN, LOW);
        digitalWrite(Pins::LED_YELLOW, LOW);
        digitalWrite(Pins::LED_RED, HIGH);

        DEBUG_PRINTLN(F("Timer END"));
        DEBUG_PRINTLN(F("Beep 3x"));

        // Akustisches Signal: 3x Piepen (Zeit abgelaufen)
        buzzerBeepMultiple(3);
    } else {
        // Verbleibende Zeit in Sekunden
        uint32_t remainingMs = timerDurationMs - elapsed;
        uint32_t remainingSec = remainingMs / 1000;

        // Gelbe LED bei letzten 30 Sekunden
        static uint32_t lastDebugSec = 0;
        if (remainingSec <= 30) {
            digitalWrite(Pins::LED_YELLOW, HIGH);

            // Debug-Ausgabe nur einmal pro Sekunde
            if (remainingSec != lastDebugSec) {
                DEBUG_PRINT(F("T:"));
                DEBUG_PRINTLN(remainingSec);
                lastDebugSec = remainingSec;
            }
        } else {
            digitalWrite(Pins::LED_YELLOW, LOW);
            lastDebugSec = 0;
        }
    }
}

/**
 * @brief Aktualisiert Vorbereitungsphase und wechselt automatisch in Schießphase
 */
void updatePreparation() {
    if (!inPreparationPhase) return;

    // Prüfe ob Vorbereitungsphase vorbei
    uint32_t elapsed = millis() - preparationStartTime;
    if (elapsed >= preparationDurationMs) {
        // Beende Vorbereitungsphase
        inPreparationPhase = false;

        DEBUG_PRINTLN(F("Prep END"));

        // Starte Timer
        timerRunning = true;
        timerStartTime = millis();

        // Grüne LED an, Rest aus
        digitalWrite(Pins::LED_GREEN, HIGH);
        digitalWrite(Pins::LED_YELLOW, LOW);
        digitalWrite(Pins::LED_RED, LOW);

        // Akustisches Signal: 1x Piepen (Ampel wird grün)
        buzzerBeepMultiple(1);
    }
}

/**
 * @brief Verarbeitet empfangenes Kommando
 * @param cmd RadioCommand
 */
void handleCommand(RadioCommand cmd) {
    switch (cmd) {
        case CMD_PING:
            // Sender testet Verbindungsqualität
            // ACK wird automatisch vom NRF24L01 gesendet
            DEBUG_PRINTLN(F("PING"));
            break;

        case CMD_INIT:
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

            // Rote LED bleibt an (Stop/Pfeile Holen)
            digitalWrite(Pins::LED_RED, HIGH);
            break;

        case CMD_START_120:
        case CMD_START_240:
            if (systemInitialized) {
                DEBUG_PRINTLN(F("START"));

                // Starte Vorbereitungsphase (10s oder 5s im DEBUG)
                inPreparationPhase = true;
                preparationStartTime = millis();
                #if DEBUG_SHORT_TIMES
                    preparationDurationMs = 5000UL;  // 5 Sekunden (DEBUG)
                #else
                    preparationDurationMs = 10000UL; // 10 Sekunden
                #endif

                // Timer-Dauer setzen (wird nach Vorbereitungsphase gestartet)
                #if DEBUG_SHORT_TIMES
                    timerDurationMs = (cmd == CMD_START_120) ? 6000UL : 12000UL;
                #else
                    timerDurationMs = (cmd == CMD_START_120) ? 120000UL : 240000UL;
                #endif

                // Ampel bleibt rot (keine LED-Änderung)
                // Akustisches Signal: 2x Piepen (Vorbereitungsphase startet)
                buzzerBeepMultiple(2);
            }
            break;

        case CMD_STOP:
            DEBUG_PRINTLN(F("STOP"));

            // Wenn Timer noch läuft: 3x Piepen (vorzeitiges Ende)
            if (timerRunning) {
                buzzerBeepMultiple(3);
            }

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an, Rest aus
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);
            break;

        case CMD_GROUP_AB:
            DEBUG_PRINTLN(F("GRP_AB"));
            currentGroup = 0;  // Gruppe A/B

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);
            break;

        case CMD_GROUP_CD:
            DEBUG_PRINTLN(F("GRP_CD"));
            currentGroup = 1;  // Gruppe C/D

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);
            break;

        case CMD_ALARM:
            DEBUG_PRINTLN(F("ALARM"));

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

            // Akustisches Signal: 8x Piepen (Alarm)
            buzzerBeepMultiple(8);
            break;

        default:
            DEBUG_PRINTLN(F("UNK"));
            break;
    }
}
