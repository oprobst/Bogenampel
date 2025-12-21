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
#include <FastLED.h>

//=============================================================================
// Globale Instanzen
//=============================================================================

RF24 radio(Pins::NRF_CE, Pins::NRF_CSN);

// WS2812B LED Strip
CRGB leds[LEDStrip::TOTAL_LEDS];
bool debugMode = false;  // Debug-Modus aktiv (5% Helligkeit)

// Forward-Deklarationen
void showRainbowEffect();
void setTrafficLightColor(CRGB color);
void setGroupAB(CRGB color);
void setGroupCD(CRGB color);
void displayNumber(uint16_t number, CRGB color, bool showLeadingZeros = false);
void displayDigit(uint8_t digitStartIndex, uint8_t digit, CRGB color);

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

    // Debug-Jumper als Eingang mit Pull-Up
    pinMode(Pins::DEBUG_JUMPER, INPUT_PULLUP);

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
    digitalWrite(Pins::BUZZER, LOW);  // Explizit Pin auf LOW setzen
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
        digitalWrite(Pins::BUZZER, LOW);  // Explizit Pin auf LOW setzen

        // Pause zwischen Tönen (außer beim letzten)
        if (i < count - 1) {
            delay(500);  // 500ms Pause
        }
    }

    // Sicherstellen, dass Buzzer am Ende definitiv aus ist
    noTone(Pins::BUZZER);
    digitalWrite(Pins::BUZZER, LOW);
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
 * @brief Aktualisiert Timer und LED-Status mit 7-Segment-Anzeige
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

        // Zeige "000" in ROT auf 7-Segment-Anzeige
        displayNumber(0, CRGB::Red, true);

        // Behalte aktuelle Gruppe sichtbar in ROT (gleiche Farbe wie "000")
        if (currentGroup == 0) {
            setGroupAB(CRGB::Red);
        } else {
            setGroupCD(CRGB::Red);
        }

        DEBUG_PRINTLN(F("Timer END"));

        // Kurze Pause damit Display sichtbar auf ROT umschaltet
        delay(100);

        DEBUG_PRINTLN(F("Beep 3x"));

        // Akustisches Signal: 3x Piepen (Zeit abgelaufen)
        buzzerBeepMultiple(3);
    } else {
        // Verbleibende Zeit in Sekunden
        uint32_t remainingMs = timerDurationMs - elapsed;
        uint32_t remainingSec = (remainingMs / 1000) + 1;  // +1 für aufrunden (zeige immer noch verbleibende volle Sekunde)

        // Begrenze auf 999 Sekunden (7-Segment-Display Maximum)
        if (remainingSec > 999) {
            remainingSec = 999;
        }

        // Farbe basierend auf verbleibender Zeit
        CRGB displayColor;
        static bool yellowPhaseActive = false;
        static uint32_t lastDisplayedSec = 0;

        if (remainingSec <= 30) {
            // Orange-Gelbe Phase (letzte 30 Sekunden)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, HIGH);
            digitalWrite(Pins::LED_RED, LOW);
            displayColor = CRGB(255, 140, 0);  // Orange (statt reines Gelb)

            if (!yellowPhaseActive) {
                yellowPhaseActive = true;
                DEBUG_PRINTLN(F("Yellow phase"));
            }
        } else {
            // Grüne Phase
            digitalWrite(Pins::LED_GREEN, HIGH);
            digitalWrite(Pins::LED_YELLOW, LOW);
            digitalWrite(Pins::LED_RED, LOW);
            displayColor = CRGB::Green;
            yellowPhaseActive = false;
        }

        // Aktualisiere Display nur wenn sich die Sekunde geändert hat
        if (remainingSec != lastDisplayedSec) {
            // Zeige verbleibende Zeit auf 7-Segment-Anzeige
            displayNumber(remainingSec, displayColor);

            // Behalte aktuelle Gruppe sichtbar in gleicher Farbe wie die Ziffern
            if (currentGroup == 0) {
                setGroupAB(displayColor);
            } else {
                setGroupCD(displayColor);
            }

            DEBUG_PRINT(F("T:"));
            DEBUG_PRINTLN(remainingSec);
            lastDisplayedSec = remainingSec;
        }
    }
}

/**
 * @brief Aktualisiert Vorbereitungsphase mit Countdown und wechselt automatisch in Schießphase
 */
void updatePreparation() {
    if (!inPreparationPhase) return;

    // Verbleibende Zeit berechnen
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

        // Zeige Start-Zeit in GRÜN (wird von updateTimer() übernommen)
        uint32_t startTimeSec = timerDurationMs / 1000;
        if (startTimeSec > 999) startTimeSec = 999;
        displayNumber(startTimeSec, CRGB::Green);

        // Behalte aktuelle Gruppe sichtbar in GRÜN (gleiche Farbe wie Ziffern)
        if (currentGroup == 0) {
            setGroupAB(CRGB::Green);
        } else {
            setGroupCD(CRGB::Green);
        }

        // Akustisches Signal: 1x Piepen (Ampel wird grün)
        buzzerBeepMultiple(1);
    } else {
        // Zeige verbleibende Vorbereitungszeit
        uint32_t remainingMs = preparationDurationMs - elapsed;
        uint32_t remainingSec = (remainingMs / 1000) + 1;  // +1 für aufrunden

        static uint32_t lastDisplayedPrepSec = 0;

        // Aktualisiere Display nur wenn sich die Sekunde geändert hat
        if (remainingSec != lastDisplayedPrepSec) {
            // Zeige verbleibende Vorbereitungszeit in ROT
            displayNumber(remainingSec, CRGB::Red);

            // Behalte aktuelle Gruppe sichtbar in ROT (gleiche Farbe wie Ziffern)
            if (currentGroup == 0) {
                setGroupAB(CRGB::Red);
            } else {
                setGroupCD(CRGB::Red);
            }

            DEBUG_PRINT(F("Prep:"));
            DEBUG_PRINTLN(remainingSec);
            lastDisplayedPrepSec = remainingSec;
        }
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
 * @brief Setzt die LEDs für Gruppe A/B auf die angegebene Farbe
 * @param color Farbe (z.B. CRGB::Red, CRGB::Green, CRGB::Blue)
 *
 * Steuert LED 1-16 (Array-Index 0-15) an.
 */
void setGroupAB(CRGB color) {
    fill_solid(leds + LEDStrip::GROUP_AB_START, LEDStrip::GROUP_AB_LEDS, color);
    FastLED.show();
}

/**
 * @brief Setzt die LEDs für Gruppe C/D auf die angegebene Farbe
 * @param color Farbe (z.B. CRGB::Red, CRGB::Green, CRGB::Blue)
 *
 * Steuert LED 17-32 (Array-Index 16-31) an.
 */
void setGroupCD(CRGB color) {
    fill_solid(leds + LEDStrip::GROUP_CD_START, LEDStrip::GROUP_CD_LEDS, color);
    FastLED.show();
}

/**
 * @brief Zeigt eine einzelne Ziffer auf einem 7-Segment-Display an
 * @param digitStartIndex Start-Index der Ziffer im LED-Array
 * @param digit Ziffer (0-9)
 * @param color Farbe der Ziffer
 *
 * Hardware-Anordnung der Segmente (in Reihenfolge im LED-Strip):
 * Index 0-5:   Segment B (rechts oben, senkrecht)
 * Index 6-11:  Segment A (oben, horizontal)
 * Index 12-17: Segment F (links oben, senkrecht)
 * Index 18-23: Segment G (mitte, horizontal)
 * Index 24-29: Segment C (rechts unten, senkrecht)
 * Index 30-35: Segment D (unten, horizontal)
 * Index 36-41: Segment E (links unten, senkrecht)
 *
 *    AAAAAA
 *   F      B
 *   F      B
 *    GGGGGG
 *   E      C
 *   E      C
 *    DDDDDD
 */
void displayDigit(uint8_t digitStartIndex, uint8_t digit, CRGB color) {
    // 7-Segment-Mapping für Ziffern 0-9
    // Jedes Bit repräsentiert ein Segment in der Reihenfolge: B, A, F, G, C, D, E
    const uint8_t segmentMap[10] = {
        0b1110111,  // 0: B, A, F, C, D, E (kein G)
        0b1000100,  // 1: B, C
        0b1101011,  // 2: B, A, G, D, E
        0b1101110,  // 3: B, A, G, C, D
        0b1011100,  // 4: B, F, G, C
        0b0111110,  // 5: A, F, G, C, D
        0b0111111,  // 6: A, F, G, C, D, E
        0b1100100,  // 7: B, A, C
        0b1111111,  // 8: B, A, F, G, C, D, E (alle)
        0b1111110   // 9: B, A, F, G, C, D
    };

    if (digit > 9) {
        digit = 0;  // Fallback auf 0 bei ungültiger Ziffer
    }

    uint8_t pattern = segmentMap[digit];

    // Segment-Reihenfolge: B, A, F, G, C, D, E
    for (uint8_t seg = 0; seg < LEDStrip::SEGMENTS_PER_DIGIT; seg++) {
        bool segmentOn = (pattern >> (LEDStrip::SEGMENTS_PER_DIGIT - 1 - seg)) & 0x01;
        CRGB segmentColor = segmentOn ? color : CRGB::Black;

        // Setze alle 6 LEDs dieses Segments
        uint8_t segmentStart = digitStartIndex + (seg * LEDStrip::LEDS_PER_SEGMENT);
        fill_solid(leds + segmentStart, LEDStrip::LEDS_PER_SEGMENT, segmentColor);
    }
}

/**
 * @brief Zeigt eine Zahl von 0-999 auf den 7-Segment-Displays an
 * @param number Zahl (0-999, wird bei >999 auf 999 begrenzt)
 * @param color Farbe der Anzeige
 * @param showLeadingZeros true = führende Nullen anzeigen, false = ausblenden (Standard)
 *
 * Zeigt die Zahl auf den drei 7-Segment-Displays an:
 * - 100er-Stelle (LED 117-158, Index 116-157)
 * - 10er-Stelle  (LED 75-116, Index 74-115)
 * - 1er-Stelle   (LED 33-74, Index 32-73)
 */
void displayNumber(uint16_t number, CRGB color, bool showLeadingZeros = false) {
    // Begrenze auf 0-999
    if (number > 999) {
        number = 999;
    }

    // Extrahiere einzelne Ziffern
    uint8_t digit100 = number / 100;
    uint8_t digit10 = (number / 10) % 10;
    uint8_t digit1 = number % 10;

    // Zeige Ziffern an
    // 100er-Stelle
    if (showLeadingZeros || number >= 100) {
        displayDigit(LEDStrip::DIGIT_100_START, digit100, color);
    } else {
        displayDigit(LEDStrip::DIGIT_100_START, 0, CRGB::Black);  // Ausschalten
    }

    // 10er-Stelle
    if (showLeadingZeros || number >= 10) {
        displayDigit(LEDStrip::DIGIT_10_START, digit10, color);
    } else {
        displayDigit(LEDStrip::DIGIT_10_START, 0, CRGB::Black);  // Ausschalten
    }

    // 1er-Stelle: Immer anzeigen
    displayDigit(LEDStrip::DIGIT_1_START, digit1, color);

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
            systemInitialized = true;

            // Alle Segmente 3x rot blinken lassen
            for (int i = 0; i < 3; i++) {
                // Alle LEDs rot
                fill_solid(leds, LEDStrip::TOTAL_LEDS, CRGB::Red);
                FastLED.show();
                delay(200);

                // Alle LEDs aus
                FastLED.clear();
                FastLED.show();
                delay(200);
            }

            // Zeige "000" und Gruppe A/B
            displayNumber(0, CRGB::Red, true);  // "000" in grün mit führenden Nullen
            setGroupAB(CRGB::Red);                // Gruppe A/B in rot
            currentGroup = 0;                     // Setze aktuelle Gruppe auf A/B

            // Status-LEDs: Rote LED an (Stop/Pfeile Holen)
            digitalWrite(Pins::LED_GREEN, LOW);
            digitalWrite(Pins::LED_YELLOW, LOW);
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

                // Rote LED bleibt an (Vorbereitungsphase)
                digitalWrite(Pins::LED_GREEN, LOW);
                digitalWrite(Pins::LED_YELLOW, LOW);
                digitalWrite(Pins::LED_RED, HIGH);

                // Zeige initiale Vorbereitungszeit in ROT (z.B. "10" oder "5")
                uint32_t prepTimeSec = preparationDurationMs / 1000;
                displayNumber(prepTimeSec, CRGB::Red);

                // Behalte aktuelle Gruppe sichtbar
                if (currentGroup == 0) {
                    setGroupAB(CRGB::Red);
                } else {
                    setGroupCD(CRGB::Red);
                }

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

            // Zeige "000" in ROT
            displayNumber(0, CRGB::Red, true);

            // Behalte aktuelle Gruppe sichtbar
            if (currentGroup == 0) {
                setGroupAB(CRGB::Red);
            } else {
                setGroupCD(CRGB::Red);
            }
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

            // Zeige "000" in ROT
            displayNumber(0, CRGB::Red, true);

            // Gruppe A/B LEDs auf ROT setzen, C/D ausschalten
            setGroupAB(CRGB::Red);
            setGroupCD(CRGB::Black);
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

            // Zeige "000" in ROT
            displayNumber(0, CRGB::Red, true);

            // Gruppe C/D LEDs auf ROT setzen, A/B ausschalten
            setGroupCD(CRGB::Red);
            setGroupAB(CRGB::Black);
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
