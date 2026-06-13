/**
 * @file EmpfaengerV3.cpp
 * @brief Hauptdatei für Bogenampel V3 Empfänger (Anzeigeeinheit)
 *
 * PORT aus V2 (Empfaenger/Empfaenger.ino) auf Seeed XIAO ESP32C3:
 * - Funk: ESP-NOW statt NRF24 (RadioManager, 11 Kommandos unverändert)
 * - Zeitbasis: esp_timer (1 Hz) statt AVR-Timer1 — gleiches Muster
 *   (Callback setzt volatile-Flag, loop() arbeitet ab)
 * - Timer-Logik 1:1 inkl. AUTONOMEM Passenende und Gruppenwechsel (FR-004):
 *   Zeitablauf benötigt KEINEN Funkempfang!
 * - Status-LED D9 AKTIV LOW (Strapping-Fix Befund 3), ersetzt die 3 V2-LEDs
 * - 3 Potis live: Lautstärke (D0), Helligkeit (D1), Lüfter (D2) — US4
 * - Debug-Taster D7: lokaler Testlauf ohne Sender (FR-024)
 */

#include "Config.h"
#include "Commands.h"
#include "RadioManager.h"
#include "OTAManager.h"
#include "DisplayManager.h"
#include "BuzzerManager.h"
#include "FanManager.h"

#include <FastLED.h>
#include <esp_timer.h>

//=============================================================================
// Globale Instanzen
//=============================================================================

RadioManager radio;

// WS2812B LED Strip
CRGB leds[LEDStrip::TOTAL_LEDS];

// Display Manager
DisplayManager display(leds);

// Buzzer Manager (LEDC, Lautstärke vom Poti)
BuzzerManager buzzer(Pins::BUZZER, Timing::BUZZER_FREQUENCY_HZ);

// Lüfter (LEDC ≥ 25 kHz, Drehzahl vom Poti)
FanManager fan(Pins::FAN_PWM, Pins::FAN_POTI);

// Forward-Deklarationen
void showRainbowEffect();
void handleCommand(RadioCommand cmd);
void updateAlarm();
void updatePotis();
void updateStatusLed();
void checkButton();
void updatePreparation();
void updateTimer();

// Boot-Modus (FR-001): einmal beim Start entschieden, danach fix (FR-007).
//   true  = OTA-Wartungsmodus (Taster D7 beim Boot gehalten): WiFi + ArduinoOTA,
//           KEIN ESP-NOW, kein Timer (FR-002).
//   false = Normalbetrieb (ESP-NOW), KEIN WiFi/AP (FR-005).
bool bootOtaMode = false;
bool readOtaModeButton();
void setupNormal();
void setupOta();
void loopNormal();
void loopOta();

// State-Variablen
uint8_t lastButtonReading = HIGH;  // Letzter gelesener Pin-Zustand
uint8_t buttonState = HIGH;        // Stabiler Button-Zustand nach Debouncing
uint32_t lastDebounceTime = 0;     // Zeitpunkt der letzten Button-Änderung

// Timer-Variablen (esp_timer-basiert, Muster wie V2-Timer1-ISR)
volatile bool secondTickOccurred = false;  // Flag: Sekunden-Tick (vom Timer-Callback)
esp_timer_handle_t secondTimer = nullptr;
bool timerRunning = false;         // Läuft der Timer?
uint32_t timerRemainingSeconds = 0;  // Verbleibende Sekunden
uint32_t timerDurationMs = 0;      // Timer-Dauer in Millisekunden
Groups::Type currentGroup = Groups::Type::GROUP_AB;      // Aktuelle Gruppe (AB oder CD)
Groups::Position currentPosition = Groups::Position::POS_1;  // Aktuelle Position (1 oder 2)
bool groupsEnabled = true;         // Sind Gruppen aktiv? (false = 1-2 Schützen Modus)

// Vorbereitungsphase
bool inPreparationPhase = false;   // Läuft die Vorbereitungsphase?
uint32_t preparationRemainingSeconds = 0;  // Verbleibende Sekunden Vorbereitungsphase
uint32_t preparationDurationMs = 0;

// Alarm State-Variablen (nicht-blockierend)
bool alarmActive = false;          // Läuft gerade ein Alarm?
uint8_t alarmBlinkCount = 0;       // Aktueller Blink-Zähler (0-7)
bool alarmLedState = false;        // LED-Zustand
uint32_t alarmLastToggle = 0;      // Zeitpunkt der letzten Umschaltung

// Lokalregler (Potis, US4)
uint32_t lastPotiUpdate = 0;       // Zeitpunkt der letzten Poti-Messung
uint8_t currentBrightness = LEDStrip::BRIGHTNESS_MAX;  // Aktuelle Helligkeit
uint8_t lastVolumeDuty = 0;        // Letzter Lautstärke-Duty (für Feedback-Erkennung)

// Status-LED (D9, aktiv LOW): an = bereit, kurzes Aus-Blinken bei Frame-Empfang
uint32_t lastFrameCount = 0;       // Letzter Stand des Frame-Zählers
uint32_t ledBlinkUntil = 0;        // LED bleibt bis dahin aus (Empfangs-Blink)

//=============================================================================
// 1-Hz-Zeitbasis (esp_timer, R-7)
//=============================================================================

/**
 * @brief Periodischer 1-s-Callback — setzt nur das Flag (wie V2-Timer1-ISR)
 */
void onSecondTick(void* /*arg*/) {
    secondTickOccurred = true;
}

//=============================================================================
// Setup
//=============================================================================

void setup() {
    // ----- Gemeinsamer, hardware-sicherer Frühstart (BEIDE Modi) -----

    // FRÜH: Poti-Fußpunkt aktivieren (Strapping-Fix Befund 2) — D4 war beim
    // Reset hochohmig, damit GPIO2 HIGH bootet; ab jetzt arbeitet der
    // Lautstärke-Poti-Teiler normal
    pinMode(Pins::POTI_GND, OUTPUT);
    digitalWrite(Pins::POTI_GND, LOW);

    // FRÜH: Lüfter-PWM übernehmen (R5-Pull-up hielt die PWM-Leitung LOW →
    // Lüfter lief bis jetzt auf Minimaldrehzahl, Befund 6). Auch im
    // OTA-Wartungsmodus nötig (Hardware-Safety, Constitution III).
    fan.begin();

    // Status-LED (D9, aktiv LOW): erst mal aus
    pinMode(Pins::STATUS_LED, OUTPUT);
    digitalWrite(Pins::STATUS_LED, STATUS_LED_OFF);

    // Debug-Taster (J5, D7, INPUT_PULLUP, aktiv LOW) — wird gleich für die
    // Boot-Modus-Entscheidung gelesen
    pinMode(Pins::BTN_DEBUG, INPUT_PULLUP);

    // Serial für Debugging
    #if DEBUG_ENABLED
    Serial.begin(System::SERIAL_BAUD);
    while (!Serial && millis() < 2000);
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("======================================");
    DEBUG_PRINTLN("  Bogenampel Empfaenger V3.0");
    DEBUG_PRINTLN("======================================");
    DEBUG_PRINTF("Build: %s %s\n", __DATE__, __TIME__);
    #endif

    // ----- Boot-Modus-Entscheidung (FR-001): genau einmal, hier -----
    bootOtaMode = readOtaModeButton();

    if (bootOtaMode) {
        DEBUG_PRINTLN("Boot-Modus: OTA-WARTUNG (Taster gehalten)");
        setupOta();
    } else {
        DEBUG_PRINTLN("Boot-Modus: Normalbetrieb");
        setupNormal();
    }
}

/**
 * @brief Liest den Debug-Taster (J5/D7, aktiv LOW) entprellt beim Boot.
 *
 * Verlangt stabiles LOW über mehrere Abtastungen (~25 ms), damit ein einzelner
 * Störimpuls die Boot-Entscheidung nicht kippt (SC-001).
 * @return true, wenn der Taster stabil gedrückt ist → OTA-Wartungsmodus.
 */
bool readOtaModeButton() {
    constexpr uint8_t SAMPLES = 5;
    for (uint8_t i = 0; i < SAMPLES; i++) {
        if (digitalRead(Pins::BTN_DEBUG) != LOW) {
            return false;  // einmal nicht gedrückt → Normalbetrieb
        }
        delay(5);
    }
    return true;  // ~25 ms stabil LOW
}

/**
 * @brief Normalbetrieb-Setup: ESP-NOW + Timer/Anzeige (FR-005/FR-006).
 *        KEIN WiFi, KEIN SoftAP, kein ArduinoOTA.
 */
void setupNormal() {
    // Buzzer initialisieren (LEDC, stumm — R4-Basis-Pulldown hält den BC337
    // schon beim Boot gesperrt)
    buzzer.begin();

    // LED Strip initialisieren (WS2812-Protokoll, GRB)
    FastLED.addLeds<WS2812, Pins::LED_STRIP, GRB>(leds, LEDStrip::TOTAL_LEDS);

    // Initiale Helligkeit aus Poti lesen (25-100 %, FR-022)
    uint16_t potiValue = analogRead(Pins::BRIGHTNESS_POTI);
    currentBrightness = map(potiValue, 0, 4095, LEDStrip::BRIGHTNESS_MIN, LEDStrip::BRIGHTNESS_MAX);
    FastLED.setBrightness(currentBrightness);

    FastLED.clear();
    FastLED.show();
    delay(50);  // Kurze Pause nach Initialisierung
    DEBUG_PRINTF("LED Strip init, Helligkeit: %u\n", currentBrightness);

    // Initiale Buzzer-Lautstärke aus Poti
    updatePotis();

    // Regenbogen-Effekt beim Start zeigen
    showRainbowEffect();

    // ESP-NOW initialisieren (KEIN WiFi-Join, KEIN SoftAP — FR-005/FR-006)
    DEBUG_PRINTLN("Initialisiere ESP-NOW...");
    if (!radio.begin()) {
        DEBUG_PRINTLN("FEHLER: ESP-NOW init fehlgeschlagen!");
        // Fehler-Anzeige: Status-LED blinkt schnell (aktiv LOW!)
        while (true) {
            digitalWrite(Pins::STATUS_LED, STATUS_LED_ON);
            delay(100);
            digitalWrite(Pins::STATUS_LED, STATUS_LED_OFF);
            delay(100);
        }
    }

    // 1-Hz-Zeitbasis starten (esp_timer, R-7)
    const esp_timer_create_args_t timerArgs = {
        .callback = &onSecondTick,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "secondTick",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timerArgs, &secondTimer);
    esp_timer_start_periodic(secondTimer, 1000000ULL);  // 1 s

    // Bereit: Status-LED an
    digitalWrite(Pins::STATUS_LED, STATUS_LED_ON);

    DEBUG_PRINTLN("Setup abgeschlossen");
    DEBUG_PRINTLN("Warte auf Kommandos vom Sender...");
}

/**
 * @brief OTA-Wartungsmodus-Setup: reiner WiFi-Station-Betrieb + ArduinoOTA
 *        (FR-002/FR-003). KEIN ESP-NOW, kein Timer, kein Regenbogen/Buzzer.
 *        Die große LED-Anzeige bleibt dunkel; die Status-LED (D9) signalisiert
 *        den Zustand (OTAManager, FR-004a-c).
 */
void setupOta() {
    // Große Anzeige im Wartungsmodus dunkel halten (nur Status-LED + Netzwerk aktiv)
    FastLED.addLeds<WS2812, Pins::LED_STRIP, GRB>(leds, LEDStrip::TOTAL_LEDS);
    FastLED.clear(true);  // löschen + anzeigen (alle aus)

    // WiFi-Station + ArduinoOTA (kein SoftAP, kein ESP-NOW)
    OTAManager::begin();
}

//=============================================================================
// Loop
//=============================================================================

void loop() {
    // Boot-Modus-Fork (FR-001/FR-007): genau einer der beiden Pfade läuft
    if (bootOtaMode) {
        loopOta();
    } else {
        loopNormal();
    }
}

/**
 * @brief OTA-Wartungsmodus-Loop: ausschließlich OTA bedienen (FR-002).
 *
 * OTAManager::handle() erledigt WiFi-Reconnect-Retry, ArduinoOTA und das
 * Status-LED-Signal (FR-004a-c). KEIN delay() hier, damit ArduinoOTA.handle()
 * den laufenden Flash nicht aushungert (FR-004c läuft währenddessen).
 */
void loopOta() {
    OTAManager::handle();
}

/**
 * @brief Normalbetrieb-Loop: ESP-NOW + Timer/Anzeige (unverändert aus V2).
 *        Es wird KEIN OTAManager::handle() aufgerufen (FR-006).
 */
void loopNormal() {
    // Discovery-Antworten bedienen (FT_HELLO → FT_HELLO_ACK)
    radio.update();

    // Empfangene Kommandos abarbeiten (Queue wird vom WiFi-Task gefüllt)
    while (radio.commandAvailable()) {
        RadioCommand cmd = radio.nextCommand();
        DEBUG_PRINTF("RX: %s\n", commandToString(cmd));
        handleCommand(cmd);
    }

    // Prüfe ob eine Sekunde vergangen ist (Timer-Flag)
    if (secondTickOccurred) {
        secondTickOccurred = false;  // Flag zurücksetzen

        // Prüfe Vorbereitungsphase
        updatePreparation();

        // Prüfe Timer und aktualisiere LEDs
        updateTimer();
    }

    // Aktualisiere Buzzer-Zustand (nicht-blockierend, muss jede Iteration laufen)
    buzzer.update();

    // Aktualisiere Alarm-Zustand (nicht-blockierend)
    updateAlarm();

    // Prüfe Debug-Button (lokaler Testlauf ohne Sender, FR-024)
    checkButton();

    // Lokalregler: Lautstärke/Helligkeit/Lüfter aus Potis (≥ 10 Hz, US4)
    updatePotis();
    fan.update();

    // Status-LED: an = bereit, kurzes Aus-Blinken bei Frame-Empfang
    updateStatusLed();

    // Kleine Pause um CPU zu entlasten
    delay(5);
}

//=============================================================================
// Status-LED (D9, aktiv LOW — Strapping-Fix Befund 3)
//=============================================================================

/**
 * @brief Status-LED: dauerhaft an (bereit), blinkt kurz aus bei Frame-Empfang
 */
void updateStatusLed() {
    uint32_t frames = radio.framesReceived();
    if (frames != lastFrameCount) {
        lastFrameCount = frames;
        ledBlinkUntil = millis() + Timing::LED_BLINK_DURATION_MS;
        digitalWrite(Pins::STATUS_LED, STATUS_LED_OFF);
    } else if (ledBlinkUntil != 0 && millis() >= ledBlinkUntil) {
        ledBlinkUntil = 0;
        digitalWrite(Pins::STATUS_LED, STATUS_LED_ON);
    }
}

//=============================================================================
// Debug-Taster (lokaler Testlauf, FR-024)
//=============================================================================

/**
 * @brief Prüft Debug-Button mit Debouncing
 *
 * Lokaler Testlauf ohne Sender (HIL #17): Druck startet eine Passe wie
 * CMD_START_120 (10 s Vorbereitung + 120 s Countdown, autonomes Ende);
 * erneuter Druck während des Laufs stoppt wie CMD_STOP.
 */
void checkButton() {
    // Aktuellen Button-Zustand lesen (LOW = gedrückt, wegen Pull-Up)
    uint8_t reading = digitalRead(Pins::BTN_DEBUG);

    // Wenn sich der gelesene Zustand geändert hat, Debounce-Timer zurücksetzen
    if (reading != lastButtonReading) {
        lastDebounceTime = millis();
    }
    lastButtonReading = reading;

    // Wenn genug Zeit vergangen ist (Debounce-Zeit), Zustand akzeptieren
    if ((millis() - lastDebounceTime) > Timing::DEBOUNCE_MS) {
        if (reading != buttonState) {
            buttonState = reading;

            // Button gedrückt (neuer stabiler Zustand = LOW)
            if (buttonState == LOW) {
                if (timerRunning || inPreparationPhase) {
                    DEBUG_PRINTLN("Debug-Taster: lokaler Stopp");
                    handleCommand(CMD_STOP);
                } else {
                    DEBUG_PRINTLN("Debug-Taster: lokaler Testlauf (120s)");
                    handleCommand(CMD_START_120);
                }
            }
        }
    }
}

//=============================================================================
// Lokalregler: Potis (US4 — FR-021/FR-022)
//=============================================================================

/**
 * @brief Liest Lautstärke- und Helligkeits-Poti und wendet die Werte live an
 *
 * Lautstärke: Poti verpolt → in Software invertiert (ADC klein = laut, groß = aus).
 * Quadratische Kennlinie über den invertierten Wert; ab Volume::OFF_THRESHOLD stumm.
 * Helligkeit: linear 25-100 % wie V2.
 */
void updatePotis() {
    if (millis() - lastPotiUpdate < Timing::POTI_UPDATE_INTERVAL_MS && lastPotiUpdate != 0) return;
    lastPotiUpdate = millis();

    // --- Lautstärke (D0) — Poti verpolt: ADC klein = laut, ADC groß = aus.
    // Quadratische Kennlinie über den INVERTIERTEN Rohwert (gleiche Steilheit wie
    // bisher); ab Volume::OFF_THRESHOLD (nahe Maximum) komplett stumm (duty 0).
    uint32_t rawVol = analogRead(Pins::VOLUME_POTI);
    uint8_t duty;
    if (rawVol >= Volume::OFF_THRESHOLD) {
        duty = 0;  // Poti am Maximum → Buzzer aus
    } else {
        uint32_t inv = (uint32_t)Volume::OFF_THRESHOLD - rawVol;  // groß = laut, 0 = leise
        duty = BuzzerManager::DUTY_MIN +
            (uint8_t)((inv * inv * (uint32_t)(BuzzerManager::DUTY_MAX - BuzzerManager::DUTY_MIN))
                      / ((uint32_t)Volume::OFF_THRESHOLD * Volume::OFF_THRESHOLD));
    }
    buzzer.setVolume(duty);

    // Lautstärke-Feedback: 3 Pieptöne wenn Poti bewegt wurde (Hysterese 4 Stufen,
    // nur wenn kein anderes Signal läuft — nicht unterbrechen)
    uint8_t delta = (duty > lastVolumeDuty) ? duty - lastVolumeDuty : lastVolumeDuty - duty;
    if (delta >= 4 && !buzzer.isActive()) {
        buzzer.beep(3);
    }
    if (delta >= 4) {
        lastVolumeDuty = duty;
    }

    // --- Helligkeit (D1, linear 15-100 %, Poti verpolt → invertiert: ADC klein = hell) ---
    uint16_t rawBright = analogRead(Pins::BRIGHTNESS_POTI);
    uint8_t newBrightness = map(rawBright, 0, 4095, LEDStrip::BRIGHTNESS_MAX, LEDStrip::BRIGHTNESS_MIN);

    // Nur aktualisieren wenn sich Helligkeit signifikant geändert hat (Hysterese)
    if (abs((int)newBrightness - (int)currentBrightness) > 3) {
        currentBrightness = newBrightness;
        FastLED.setBrightness(currentBrightness);
        FastLED.show();
    }
}

//=============================================================================
// Alarm (nicht-blockierend, PORT aus V2)
//=============================================================================

/**
 * @brief Aktualisiert Alarm-Zustand (nicht-blockierend)
 *
 * Blinkt 8x mit 250ms Pausen (alle LEDs inkl. 7-Segment und Gruppen).
 * Diese Funktion muss regelmäßig in loop() aufgerufen werden!
 */
void updateAlarm() {
    if (!alarmActive) return;

    uint32_t now = millis();
    uint32_t elapsed = now - alarmLastToggle;

    // 250ms vergangen?
    if (elapsed >= 250) {
        alarmLastToggle = now;

        if (alarmLedState) {
            // LED Strip ausschalten (7-Segment + Gruppen)
            FastLED.clear();
            FastLED.show();
            digitalWrite(Pins::STATUS_LED, STATUS_LED_OFF);

            alarmLedState = false;

            // Blink-Zähler erhöhen
            alarmBlinkCount++;

            // Alle 8 Blinks fertig?
            if (alarmBlinkCount >= 8) {
                alarmActive = false;
                // Nach dem Alarm: gestoppt (rot "000"), Status-LED wieder an
                digitalWrite(Pins::STATUS_LED, STATUS_LED_ON);
                display.displayTimer(0, CRGB::Red, true);
            }
        } else {
            // LED Strip einschalten (alle ROT: 7-Segment + Gruppen)
            fill_solid(leds, LEDStrip::TOTAL_LEDS, CRGB::Red);
            FastLED.show();
            digitalWrite(Pins::STATUS_LED, STATUS_LED_ON);

            alarmLedState = true;
        }
    }
}

//=============================================================================
// Timer-Logik (PORT aus V2 — AUTONOM, FR-004!)
//=============================================================================

/**
 * @brief Aktualisiert Timer und Anzeige (1x pro Sekunde via esp_timer-Flag)
 *
 * AUTONOMIE-INVARIANTE (FR-004/FR-004a): Das reguläre Passenende und der
 * Gruppenwechsel bei 3-4 Schützen passieren hier OHNE jeden Funkempfang —
 * der V2-Bugfix (Empfänger beendet Passe selbst) bleibt erhalten (SC-012).
 */
void updateTimer() {
    if (!timerRunning) return;

    if (timerRemainingSeconds == 0) {
        // Timer abgelaufen
        timerRunning = false;

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

        DEBUG_PRINTLN("Timer END");

        // Autonome Entscheidung: Ende der Passe oder automatischer Gruppenwechsel
        if (!groupsEnabled || currentPosition == Groups::Position::POS_2) {
            // Ende der Passe: 1-2 Schützen, oder zweite Gruppe bei 3-4 Schützen
            DEBUG_PRINTLN("Pass END -> 3x beep");
            buzzer.beep(3);
        } else {
            // Erste Gruppe fertig (POS_1) → automatisch zur zweiten Gruppe wechseln
            if (currentGroup == Groups::Type::GROUP_AB) {
                currentGroup = Groups::Type::GROUP_CD;
                DEBUG_PRINTLN("Auto: AB->CD, prep start");
            } else {
                currentGroup = Groups::Type::GROUP_AB;
                DEBUG_PRINTLN("Auto: CD->AB, prep start");
            }
            currentPosition = Groups::Position::POS_2;

            // Vorbereitungsphase für zweite Gruppe starten
            inPreparationPhase = true;
            #if DEBUG_SHORT_TIMES
                preparationRemainingSeconds = 5;
            #else
                preparationRemainingSeconds = 10;
            #endif

            // Gruppe aktualisieren (die andere Gruppe ausschalten)
            if (currentGroup == Groups::Type::GROUP_AB) {
                display.setGroup(0, CRGB::Red);
                display.setGroup(1, CRGB::Black);
            } else {
                display.setGroup(0, CRGB::Black);
                display.setGroup(1, CRGB::Red);
            }
            display.displayTimer(preparationRemainingSeconds, CRGB::Red);

            // 2x Piepen (Vorbereitungsphase startet)
            buzzer.beep(2);
        }
    } else {
        // Begrenze auf 999 Sekunden (7-Segment-Display Maximum)
        uint32_t displaySec = (timerRemainingSeconds > 999) ? 999 : timerRemainingSeconds;

        // Farbe basierend auf verbleibender Zeit
        CRGB displayColor;
        static bool orangePhaseActive = false;

        #if DEBUG_SHORT_TIMES
            // DEBUG: Orange Ampel in den letzten 5 Sekunden
            uint32_t orangeThreshold = 5;
        #else
            // Normal: Orange Ampel in den letzten 30 Sekunden
            uint32_t orangeThreshold = 30;
        #endif

        if (timerRemainingSeconds <= orangeThreshold) {
            // Orange Phase
            displayColor = CRGB(255, 140, 0);  // Orange (statt reines Gelb)

            if (!orangePhaseActive) {
                orangePhaseActive = true;
                DEBUG_PRINTLN("Orange phase");
            }
        } else {
            // Grüne Phase
            displayColor = CRGB::Green;
            orangePhaseActive = false;
        }

        // Zeige verbleibende Zeit auf 7-Segment-Anzeige
        display.displayTimer(displaySec, displayColor);

        // Behalte aktuelle Gruppe sichtbar in gleicher Farbe wie die Ziffern
        if (!groupsEnabled) {
            // Keine Gruppe (1-2 Schützen Modus) - beide aus
            display.setGroup(0, CRGB::Black);
            display.setGroup(1, CRGB::Black);
        } else if (currentGroup == Groups::Type::GROUP_AB) {
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
 * @brief Aktualisiert Vorbereitungsphase und wechselt automatisch in die
 *        Schießphase (1x pro Sekunde via esp_timer-Flag)
 */
void updatePreparation() {
    if (!inPreparationPhase) return;

    if (preparationRemainingSeconds == 0) {
        // Beende Vorbereitungsphase
        inPreparationPhase = false;

        DEBUG_PRINTLN("Prep END");

        // Starte Timer
        timerRunning = true;
        timerRemainingSeconds = timerDurationMs / 1000;  // Konvertiere zu Sekunden

        // Zeige Start-Zeit in GRÜN
        uint32_t startTimeSec = (timerRemainingSeconds > 999) ? 999 : timerRemainingSeconds;
        display.displayTimer(startTimeSec, CRGB::Green);

        // Behalte aktuelle Gruppe sichtbar in GRÜN (nur bei aktivierten Gruppen)
        if (!groupsEnabled) {
            display.setGroup(0, CRGB::Black);
            display.setGroup(1, CRGB::Black);
        } else if (currentGroup == Groups::Type::GROUP_AB) {
            display.setGroup(0, CRGB::Green);
        } else {
            display.setGroup(1, CRGB::Green);
        }

        // Akustisches Signal: 1x Piepen (Ampel wird grün)
        buzzer.beep(1);
    } else {
        // Zeige verbleibende Vorbereitungszeit in ROT
        display.displayTimer(preparationRemainingSeconds, CRGB::Red);

        // Behalte aktuelle Gruppe sichtbar in ROT (nur bei aktivierten Gruppen)
        if (!groupsEnabled) {
            display.setGroup(0, CRGB::Black);
            display.setGroup(1, CRGB::Black);
        } else if (currentGroup == Groups::Type::GROUP_AB) {
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

//=============================================================================
// Start-Effekt (PORT aus V2)
//=============================================================================

/**
 * @brief Zeigt einen Regenbogen-Effekt beim Systemstart
 *
 * Aktiviert jedes Segment nacheinander in Regenbogenfarben:
 * 2 Gruppen + 21 7-Segment-Balken = 23 Segmente, Dauer ca. 5 Sekunden.
 */
void showRainbowEffect() {
    DEBUG_PRINTLN("Regenbogen-Effekt...");

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

//=============================================================================
// Kommando-Verarbeitung (PORT aus V2, Semantik unverändert — FR-002)
//=============================================================================

/**
 * @brief Verarbeitet empfangenes Kommando
 * @param cmd RadioCommand
 */
void handleCommand(RadioCommand cmd) {
    switch (cmd) {
        case CMD_PING:
            // Sender testet Verbindungsqualität — Link-Layer-ACK genügt
            DEBUG_PRINTLN("PING");
            break;

        case CMD_INIT:
            DEBUG_PRINTLN("INIT");

            // Alle Segmente 3x blau blinken lassen
            for (int i = 0; i < 3; i++) {
                fill_solid(leds, LEDStrip::TOTAL_LEDS, CRGB::Blue);
                FastLED.show();
                delay(200);

                FastLED.clear();
                FastLED.show();
                delay(200);
            }

            // Zeige "000" und Gruppe A/B
            display.displayTimer(0, CRGB::Red, true);
            display.setGroup(0, CRGB::Red);                // Gruppe A/B in rot
            currentGroup = Groups::Type::GROUP_AB;         // Setze aktuelle Gruppe auf A/B
            currentPosition = Groups::Position::POS_1;     // Position 1 (ganze Passe)
            break;

        case CMD_START_120:
        case CMD_START_240:
            DEBUG_PRINTLN("START");

            // Wenn Vorbereitungsphase bereits autonom gestartet wurde (z.B. nach
            // erstem Gruppe-Ende), CMD_START ignorieren um Sync-Versatz zu vermeiden.
            // (Regel 3 im Protokoll-Contract — CMD_START ist hier nur Sync-Signal)
            if (inPreparationPhase) {
                DEBUG_PRINTLN("START ignored: prep already running");
                break;
            }

            // Timer stoppen (falls noch von vorheriger Gruppe aktiv)
            timerRunning = false;

            // Starte Vorbereitungsphase (10s oder 5s im DEBUG)
            inPreparationPhase = true;
            #if DEBUG_SHORT_TIMES
                preparationDurationMs = 5000UL;
                preparationRemainingSeconds = 5;
            #else
                preparationDurationMs = 10000UL;
                preparationRemainingSeconds = 10;
            #endif

            // Timer-Dauer setzen (wird nach Vorbereitungsphase gestartet)
            #if DEBUG_SHORT_TIMES
                timerDurationMs = 15000UL;  // 15 Sekunden für beide Modi
            #else
                timerDurationMs = (cmd == CMD_START_120) ? 120000UL : 240000UL;
            #endif

            // Zeige initiale Vorbereitungszeit in ROT (z.B. "10")
            display.displayTimer(preparationRemainingSeconds, CRGB::Red);

            // Behalte aktuelle Gruppe sichtbar
            if (!groupsEnabled) {
                display.setGroup(0, CRGB::Black);
                display.setGroup(1, CRGB::Black);
            } else if (currentGroup == Groups::Type::GROUP_AB) {
                display.setGroup(0, CRGB::Red);
            } else {
                display.setGroup(1, CRGB::Red);
            }

            // Akustisches Signal: 2x Piepen (Vorbereitungsphase startet)
            buzzer.beep(2);
            break;

        case CMD_STOP:
            DEBUG_PRINTLN("STOP");

            // Timer sofort stoppen
            timerRunning = false;
            timerRemainingSeconds = 0;

            // Vorbereitungsphase auch stoppen (falls noch aktiv)
            inPreparationPhase = false;
            preparationRemainingSeconds = 0;
            preparationDurationMs = 0;

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Behalte aktuelle Gruppe sichtbar in ROT (falls vorhanden)
            if (!groupsEnabled) {
                display.setGroup(0, CRGB::Black);
                display.setGroup(1, CRGB::Black);
            } else if (currentGroup == Groups::Type::GROUP_AB) {
                display.setGroup(0, CRGB::Red);
            } else {
                display.setGroup(1, CRGB::Red);
            }

            // Akustisches Signal: 3x Piepen (Schießphase beendet)
            // (Alarm wird NICHT vorzeitig beendet - läuft bis zum Ende)
            buzzer.beep(3);
            break;

        case CMD_GROUP_AB:
            DEBUG_PRINTLN("GRP_AB");
            currentGroup = Groups::Type::GROUP_AB;      // Gruppe A/B
            currentPosition = Groups::Position::POS_1;  // Position 1 (ganze Passe)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe A/B LEDs auf ROT setzen (C/D wird automatisch ausgeschaltet)
            display.setGroup(0, CRGB::Red);
            break;

        case CMD_GROUP_CD:
            DEBUG_PRINTLN("GRP_CD");
            currentGroup = Groups::Type::GROUP_CD;      // Gruppe C/D
            currentPosition = Groups::Position::POS_1;  // Position 1 (ganze Passe)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe C/D LEDs auf ROT setzen (A/B wird automatisch ausgeschaltet)
            display.setGroup(1, CRGB::Red);
            break;

        case CMD_GROUP_NONE:
            DEBUG_PRINTLN("GRP_NONE");
            groupsEnabled = false;  // Keine Gruppen (1-2 Schützen Modus)

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // BEIDE Gruppen ausschalten (1-2 Schützen Modus)
            display.setGroup(0, CRGB::Black);
            display.setGroup(1, CRGB::Black);
            break;

        case CMD_GROUP_FINISH_AB:
            DEBUG_PRINTLN("GRP_FINISH_AB");
            currentGroup = Groups::Type::GROUP_AB;      // Gruppe A/B
            currentPosition = Groups::Position::POS_2;  // Position 2 (zweite Hälfte)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe A/B LEDs auf ROT setzen
            display.setGroup(0, CRGB::Red);
            break;

        case CMD_GROUP_FINISH_CD:
            DEBUG_PRINTLN("GRP_FINISH_CD");
            currentGroup = Groups::Type::GROUP_CD;      // Gruppe C/D
            currentPosition = Groups::Position::POS_2;  // Position 2 (zweite Hälfte)
            groupsEnabled = true;

            // Timer und Vorbereitung stoppen
            timerRunning = false;
            inPreparationPhase = false;

            // Zeige "000" in ROT
            display.displayTimer(0, CRGB::Red, true);

            // Gruppe C/D LEDs auf ROT setzen (A/B wird automatisch ausgeschaltet)
            display.setGroup(1, CRGB::Red);
            break;

        case CMD_ALARM:
            DEBUG_PRINTLN("ALARM");

            // Laufendes Alarm-Muster NICHT neu starten (App-Retries des
            // Senders haben neue seq — Idempotenz-Regel 6 im Contract)
            if (alarmActive) {
                DEBUG_PRINTLN("ALARM ignored: already active");
                break;
            }

            // Timer und Phasen sofort stoppen
            timerRunning = false;
            timerRemainingSeconds = 0;
            inPreparationPhase = false;
            preparationRemainingSeconds = 0;

            // Starte nicht-blockierenden Alarm (8x blinken mit 250ms)
            alarmActive = true;
            alarmBlinkCount = 0;
            alarmLastToggle = millis();

            // Sofort einschalten (alle ROT)
            fill_solid(leds, LEDStrip::TOTAL_LEDS, CRGB::Red);
            FastLED.show();
            alarmLedState = true;

            // Akustisches Signal: 8x Piepen (Alarm)
            buzzer.beep(8);
            break;

        default:
            DEBUG_PRINTLN("UNK");
            break;
    }
}
