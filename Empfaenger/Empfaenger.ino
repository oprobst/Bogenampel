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
#include "DisplayManager.h"
#include "BuzzerManager.h"

#include <SPI.h>
#include <RF24.h>
#include <FastLED.h>

//=============================================================================
// Globale Instanzen
//=============================================================================

RF24 radio(Pins::NRF_CE, Pins::NRF_CSN);

// WS2812B LED Strip
CRGB leds[LEDStrip::TOTAL_LEDS];
bool debugMode = false;  // Debug-Modus aktiv (5% Helligkeit)

// Display Manager
DisplayManager display(leds);

// Buzzer Manager
BuzzerManager buzzer(Pins::BUZZER, Timing::BUZZER_FREQUENCY_HZ);

// Forward-Deklarationen
void showRainbowEffect();
void setTrafficLightColor(CRGB color);

// State-Variablen
uint32_t lastBlinkTime = 0;        // Für LED-Blink-Timer
uint8_t lastButtonReading = HIGH;  // Letzter gelesener Pin-Zustand
uint8_t buttonState = HIGH;        // Stabiler Button-Zustand nach Debouncing
uint32_t lastDebounceTime = 0;     // Zeitpunkt der letzten Button-Änderung

// Timer-Variablen (Interrupt-basiert)
volatile bool secondTickOccurred = false;  // Flag: Sekunden-Tick passiert (wird von ISR gesetzt)
bool timerRunning = false;         // Läuft der Timer?
uint32_t timerRemainingSeconds = 0;  // Verbleibende Sekunden (wird jede Sekunde dekrementiert)
uint32_t timerDurationMs = 0;      // Timer-Dauer in Millisekunden (für Kompatibilität)
Groups::Type currentGroup = Groups::Type::GROUP_AB;      // Aktuelle Gruppe (AB oder CD)
Groups::Position currentPosition = Groups::Position::POS_1;  // Aktuelle Position (1 oder 2)
bool groupsEnabled = true;         // Sind Gruppen aktiv? (false = 1-2 Schützen Modus)

// Vorbereitungsphase
bool inPreparationPhase = false;   // Läuft die Vorbereitungsphase?
uint32_t preparationRemainingSeconds = 0;  // Verbleibende Sekunden Vorbereitungsphase
uint32_t preparationDurationMs = 0; // Dauer der Vorbereitungsphase (für Kompatibilität)


//=============================================================================
// Timer1 Interrupt Service Routine (ISR)
//=============================================================================

/**
 * @brief Timer1 Compare Match A Interrupt - wird jede Sekunde ausgelöst
 *
 * Diese ISR wird exakt jede Sekunde aufgerufen und setzt ein Flag,
 * das in der loop() verarbeitet wird. Dadurch ist die Zeitbasis
 * unabhängig von blocking delays (z.B. Buzzer).
 */
ISR(TIMER1_COMPA_vect) {
    secondTickOccurred = true;
}

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

    // Timer1 für exakte Sekunden-Ticks konfigurieren
    setupTimer1();

    // Debug-Jumper lesen
    debugMode = (digitalRead(Pins::DEBUG_JUMPER) == LOW);
    DEBUG_PRINT(F("Debug-Modus: "));
    DEBUG_PRINTLN(debugMode ? F("AN (25%)") : F("AUS (100%)"));

    // LED Strip initialisieren (WS2812E - neuere Variante)
    // WS2812E verwendet oft GRB statt RGB
    FastLED.addLeds<WS2812, Pins::LED_STRIP, GRB>(leds, LEDStrip::TOTAL_LEDS);
    FastLED.setBrightness(debugMode ? LEDStrip::BRIGHTNESS_DEBUG : LEDStrip::BRIGHTNESS_NORMAL);
    FastLED.clear();
    FastLED.show();
    delay(50);  // Kurze Pause nach Initialisierung
    DEBUG_PRINTLN(F("LED Strip initialisiert"));

    // Regenbogen-Effekt beim Start zeigen
    showRainbowEffect();

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

    // Prüfe ob eine Sekunde vergangen ist (Interrupt-Flag)
    if (secondTickOccurred) {
        secondTickOccurred = false;  // Flag zurücksetzen

        // Prüfe Vorbereitungsphase
        updatePreparation();

        // Prüfe Timer und aktualisiere LEDs
        updateTimer();
    }

    // Aktualisiere Buzzer-Zustand (nicht-blockierend, muss jede Iteration laufen)
    buzzer.update();

    // Prüfe Debug-Button (nicht zeitkritisch)
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
    buzzer.begin();
    digitalWrite(Pins::BUZZER, LOW);

    // Debug-Taster als Eingang mit Pull-Up
    pinMode(Pins::BTN_DEBUG, INPUT_PULLUP);

    // Debug-Jumper als Eingang mit Pull-Up
    pinMode(Pins::DEBUG_JUMPER, INPUT_PULLUP);

    // NRF24 Control Pins werden von RF24.begin() initialisiert!
    // Keine manuelle Initialisierung nötig

    DEBUG_PRINTLN(F("Pins initialisiert"));
}

/**
 * @brief Konfiguriert Timer1 für exakte 1-Sekunden-Interrupts
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
 * @brief Lässt den Buzzer einen kurzen Piepton ausgeben (nicht-blockierend)
 */
void buzzerBeep() {
    buzzer.beep(1);
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
 * @brief Aktualisiert Timer und LED-Status mit 7-Segment-Anzeige (Interrupt-basiert)
 *
 * Diese Funktion wird genau einmal pro Sekunde aufgerufen (durch Timer1-Interrupt).
 * Dadurch ist die Zeitbasis unabhängig von blocking delays (z.B. Buzzer).
 */
void updateTimer() {
    if (!timerRunning) return;

    if (timerRemainingSeconds == 0) {
        // Timer abgelaufen
        timerRunning = false;

        // Rote LED an (Stop)
        digitalWrite(Pins::LED_GREEN, LOW);
        digitalWrite(Pins::LED_YELLOW, LOW);
        digitalWrite(Pins::LED_RED, HIGH);

        // Zeige "000" in ROT auf 7-Segment-Anzeige
        display.displayTimer(0, CRGB::Red, true);

        // Behalte aktuelle Gruppe sichtbar in ROT (falls vorhanden)
        if (!groupsEnabled) {
            // Keine Gruppe (1-2 Schützen Modus) - beide aus
            display.setGroup(0, CRGB::Black);
            display.setGroup(1, CRGB::Black);
        } else if (currentGroup == Groups::Type::GROUP_AB) {
            display.setGroup(0, CRGB::Red);
        } else {
            display.setGroup(1, CRGB::Red);
        }

        DEBUG_PRINTLN(F("Timer END"));

        // KEINE 3 Pieptöne hier! Diese werden nur bei CMD_STOP gesendet
        // (wichtig für 3-4 Schützen: nur am Ende BEIDER Gruppen piepen)
    } else {
        // Begrenze auf 999 Sekunden (7-Segment-Display Maximum)
        uint32_t displaySec = (timerRemainingSeconds > 999) ? 999 : timerRemainingSeconds;

        // Farbe basierend auf verbleibender Zeit
        CRGB displayColor;
        static bool yellowPhaseActive = false;

        #if DEBUG_SHORT_TIMES
            // DEBUG: Gelbe Ampel in den letzten 5 Sekunden
            uint32_t yellowThreshold = 5;
        #else
            // Normal: Gelbe Ampel in den letzten 30 Sekunden
            uint32_t yellowThreshold = 30;
        #endif

        if (timerRemainingSeconds <= yellowThreshold) {
            // Orange-Gelbe Phase
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, HIGH);
            digitalWrite(Pins::LED_RED, LOW);
            displayColor = CRGB(255, 140, 0);  // Orange (statt reines Gelb)

            if (!yellowPhaseActive) {
                yellowPhaseActive = true;
                DEBUG_PRINTLN(F("Orange phase"));
            }
        } else {
            // Grüne Phase
            digitalWrite(Pins::LED_GREEN, HIGH);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, LOW);
            displayColor = CRGB::Green;
            yellowPhaseActive = false;
        }

        // Zeige verbleibende Zeit auf 7-Segment-Anzeige
        display.displayTimer(displaySec, displayColor);

        // Behalte aktuelle Gruppe sichtbar in gleicher Farbe wie die Ziffern
        if (currentGroup == Groups::Type::GROUP_AB) {
            display.setGroup(0, displayColor);
        } else {
            display.setGroup(1, displayColor);
        }
       
    }
     
    // Dekrementiere verbleibende Zeit
    if (timerRemainingSeconds > 0) {
        timerRemainingSeconds--;
    }
}

/**
 * @brief Aktualisiert Vorbereitungsphase mit Countdown und wechselt automatisch in Schießphase (Interrupt-basiert)
 *
 * Diese Funktion wird genau einmal pro Sekunde aufgerufen (durch Timer1-Interrupt).
 */
void updatePreparation() {
    if (!inPreparationPhase) return;

    if (preparationRemainingSeconds == 0) {
        // Beende Vorbereitungsphase
        inPreparationPhase = false;

        DEBUG_PRINTLN(F("Prep END"));

        // 1 Signalton: Schießphase beginnt
        buzzer.beep(1);

        // Starte Timer
        timerRunning = true;
        timerRemainingSeconds = timerDurationMs / 1000;  // Konvertiere zu Sekunden

        // Grüne LED an, Rest aus
        digitalWrite(Pins::LED_GREEN, HIGH);
        digitalWrite(Pins::LED_YELLOW, LOW);
        digitalWrite(Pins::LED_RED, LOW);

        // Zeige Start-Zeit in GRÜN
        uint32_t startTimeSec = (timerRemainingSeconds > 999) ? 999 : timerRemainingSeconds;
        display.displayTimer(startTimeSec, CRGB::Green);

        // Behalte aktuelle Gruppe sichtbar in GRÜN (gleiche Farbe wie Ziffern)
        if (currentGroup == Groups::Type::GROUP_AB) {
            display.setGroup(0, CRGB::Green);
        } else {
            display.setGroup(1, CRGB::Green);
        }

        // Akustisches Signal: 1x Piepen (Ampel wird grün)
        buzzer.beep(1);
    } else {
        // Zeige verbleibende Vorbereitungszeit in ROT
        display.displayTimer(preparationRemainingSeconds, CRGB::Red);

        // Behalte aktuelle Gruppe sichtbar in ROT (gleiche Farbe wie Ziffern)
        if (currentGroup == Groups::Type::GROUP_AB) {
            display.setGroup(0, CRGB::Red);
        } else {
            display.setGroup(1, CRGB::Red);

        }
        
    }

    // Dekrementiere verbleibende Zeit
    if (preparationRemainingSeconds > 0) {
        preparationRemainingSeconds--;
    }
}

/**
 * @brief Setzt alle LEDs auf die Ampel-Farbe
 * @param color Farbe (CRGB::Red, CRGB::Yellow, CRGB::Green)
 */
void setTrafficLightColor(CRGB color) {
    fill_solid(leds, LEDStrip::TOTAL_LEDS, color);
    FastLED.show();
}

/**
 * @brief Zeigt einen Regenbogen-Effekt beim Systemstart
 *
 * Aktiviert jedes Segment nacheinander in Regenbogenfarben:
 * 1. Gruppe A/B (16 LEDs)
 * 2. Gruppe C/D (16 LEDs)
 * 3. Alle 21 Segmente der 7-Segment-Displays (3 Ziffern × 7 Segmente)
 *
 * Gesamt: 23 Segmente in Regenbogenfarben
 * Dauer: ca. 5 Sekunden
 */
void showRainbowEffect() {
    DEBUG_PRINTLN(F("Regenbogen-Effekt..."));

    // Alle LEDs ausschalten
    FastLED.clear();
    FastLED.show();

    // Anzahl der "Segmente": 2 Gruppen + 21 7-Segment-Balken = 23
    const uint8_t totalSegments = 2 + (LEDStrip::NUM_DIGITS * LEDStrip::SEGMENTS_PER_DIGIT);
    uint8_t segmentIndex = 0;

    // 1. Gruppe A/B (Segment 0)
    uint8_t hue = (segmentIndex * 256) / totalSegments;
    fill_solid(leds + LEDStrip::GROUP_AB_START, LEDStrip::GROUP_AB_LEDS, CHSV(hue, 255, 255));
    FastLED.show();
    delay(200);
    segmentIndex++;

    // 2. Gruppe C/D (Segment 1)
    hue = (segmentIndex * 256) / totalSegments;
    fill_solid(leds + LEDStrip::GROUP_CD_START, LEDStrip::GROUP_CD_LEDS, CHSV(hue, 255, 255));
    FastLED.show();
    delay(200);
    segmentIndex++;

    // 3. Alle 7-Segment-Display-Segmente (3 Ziffern × 7 Segmente = 21 Segmente)
    // Reihenfolge: 1er-Stelle (B,A,F,G,C,D,E), 10er-Stelle (B,A,F,G,C,D,E), 100er-Stelle (B,A,F,G,C,D,E)
    for (uint8_t digit = 0; digit < LEDStrip::NUM_DIGITS; digit++) {
        // Start-Index für diese Ziffer
        uint8_t digitStart = LEDStrip::DIGIT_START + (digit * LEDStrip::LEDS_PER_DIGIT);

        // Gehe durch alle 7 Segmente dieser Ziffer
        for (uint8_t seg = 0; seg < LEDStrip::SEGMENTS_PER_DIGIT; seg++) {
            // Berechne Regenbogenfarbe für dieses Segment
            hue = (segmentIndex * 256) / totalSegments;

            // Setze alle 6 LEDs dieses Segments auf die Regenbogenfarbe
            uint8_t segmentStart = digitStart + (seg * LEDStrip::LEDS_PER_SEGMENT);
            fill_solid(leds + segmentStart, LEDStrip::LEDS_PER_SEGMENT, CHSV(hue, 255, 255));

            // Zeige das Update an
            FastLED.show();
            delay(200);

            segmentIndex++;
        }
    }

    // Kurze Pause am Ende mit allen Segmenten leuchtend
    delay(800);

    // Alle LEDs ausschalten
    FastLED.clear();
    FastLED.show();
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

            // Alle Segmente 3x blau blinken lassen
            for (int i = 0; i < 3; i++) {
                // Alle LEDs rot
                fill_solid(leds, LEDStrip::TOTAL_LEDS, CRGB::Blue);
                FastLED.show();
                delay(200);

                // Alle LEDs aus
                FastLED.clear();
                FastLED.show();
                delay(200);
            }

            // Zeige "000" und Gruppe A/B
            display.displayTimer(0, CRGB::Red, true);
            display.setGroup(0, CRGB::Red);                // Gruppe A/B in rot
            currentGroup = Groups::Type::GROUP_AB;         // Setze aktuelle Gruppe auf A/B
            currentPosition = Groups::Position::POS_1;     // Position 1 (ganze Passe)

            // Status-LEDs: Rote LED an (Stop/Pfeile Holen)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);
            break;

        case CMD_START_120:
        case CMD_START_240:
            DEBUG_PRINTLN(F("START"));

            // Timer stoppen (falls noch von vorheriger Gruppe aktiv)
            timerRunning = false;

            // Starte Vorbereitungsphase (10s oder 5s im DEBUG)
            inPreparationPhase = true;
            #if DEBUG_SHORT_TIMES
                preparationDurationMs = 5000UL;  // 5 Sekunden (DEBUG)
                preparationRemainingSeconds = 5;  // 5 Sekunden
            #else
                preparationDurationMs = 10000UL; // 10 Sekunden
                preparationRemainingSeconds = 10;  // 10 Sekunden
            #endif

            // Timer-Dauer setzen (wird nach Vorbereitungsphase gestartet)
            #if DEBUG_SHORT_TIMES
                timerDurationMs = 15000UL;  // 15 Sekunden für beide Modi
            #else
                timerDurationMs = (cmd == CMD_START_120) ? 120000UL : 240000UL;
            #endif

            // Rote LED bleibt an (Vorbereitungsphase)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);

            // Zeige initiale Vorbereitungszeit in ROT (z.B. "10" oder "5")
            display.displayTimer(preparationRemainingSeconds, CRGB::Red);

            // Behalte aktuelle Gruppe sichtbar
            if (currentGroup == Groups::Type::GROUP_AB) {
                display.setGroup(0, CRGB::Red);
            } else {
                display.setGroup(1, CRGB::Red);
            }

            // Akustisches Signal: 2x Piepen (Vorbereitungsphase startet)
            buzzer.beep(2);
            break;

        case CMD_STOP:
            DEBUG_PRINTLN(F("STOP"));

            preparationRemainingSeconds = 0;
            preparationDurationMs = 0;

            // Akustisches Signal: 3x Piepen (Schießphase beendet)
            buzzer.beep(3);
            break;

        case CMD_GROUP_AB:
            DEBUG_PRINTLN(F("GRP_AB"));
            currentGroup = Groups::Type::GROUP_AB;      // Gruppe A/B
            currentPosition = Groups::Position::POS_1;  // Position 1 (ganze Passe)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe A/B LEDs auf ROT setzen (C/D wird automatisch ausgeschaltet)
            display.setGroup(0, CRGB::Red);
            break;

        case CMD_GROUP_CD:
            DEBUG_PRINTLN(F("GRP_CD"));
            currentGroup = Groups::Type::GROUP_CD;      // Gruppe C/D
            currentPosition = Groups::Position::POS_1;  // Position 1 (ganze Passe)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe C/D LEDs auf ROT setzen (A/B wird automatisch ausgeschaltet)
            display.setGroup(1, CRGB::Red);
            break;

        case CMD_GROUP_NONE:
            DEBUG_PRINTLN(F("GRP_NONE"));
            groupsEnabled = false;  // Keine Gruppen (1-2 Schützen Modus)

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // BEIDE Gruppen ausschalten (1-2 Schützen Modus)
            display.setGroup(0, CRGB::Black);
            display.setGroup(1, CRGB::Black);
            break;

        case CMD_GROUP_FINISH_AB:
            DEBUG_PRINTLN(F("GRP_FINISH_AB"));
            currentGroup = Groups::Type::GROUP_AB;      // Gruppe A/B
            currentPosition = Groups::Position::POS_2;  // Position 2 (zweite Hälfte der Passe)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe A/B LEDs auf ROT setzen
            display.setGroup(0, CRGB::Red);
            break;

        case CMD_GROUP_FINISH_CD:
            DEBUG_PRINTLN(F("GRP_FINISH_CD"));
            currentGroup = Groups::Type::GROUP_CD;      // Gruppe C/D
            currentPosition = Groups::Position::POS_2;  // Position 2 (zweite Hälfte der Passe)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Rote LED an (Stop)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, HIGH);

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe C/D LEDs auf ROT setzen (A/B wird automatisch ausgeschaltet)
            display.setGroup(1, CRGB::Red);
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
            buzzer.beep(8);
            break;

        default:
            DEBUG_PRINTLN(F("UNK"));
            break;
    }
}
