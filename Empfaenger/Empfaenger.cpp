/**
 * @file Empfaenger.cpp
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
#include <esp_task_wdt.h>

//=============================================================================
// Globale Instanzen
//=============================================================================

RadioManager radio;

// WS2811 LED Strip (12V, IC-gesteuert, RGB)
CRGB leds[LEDStrip::TOTAL_LEDS];

// Display Manager
DisplayManager display(leds);

// Buzzer Manager (LEDC, Lautstärke vom Poti)
BuzzerManager buzzer(Pins::BUZZER, Timing::BUZZER_FREQUENCY_HZ);

// Lüfter (LEDC ≥ 25 kHz, Drehzahl vom Poti)
FanManager fan(Pins::FAN_PWM, Pins::FAN_POTI);

// Forward-Deklarationen
void showRainbowEffect();
void showLedsSoftStart();
void handleCommand(RadioCommand cmd);
void updateAlarm();
void updatePotis();
void updateBrightnessPreview();
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
void updateLedDebugAnimation();

// State-Variablen
uint8_t lastButtonReading = HIGH;  // Letzter gelesener Pin-Zustand
uint8_t buttonState = HIGH;        // Stabiler Button-Zustand nach Debouncing
uint32_t lastDebounceTime = 0;     // Zeitpunkt der letzten Button-Änderung

// Sammel-Flag für direkte FastLED-Array-Manipulationen (Alarm, Helligkeits-
// Vorschau, …). Zusammen mit display.isDirty() entscheidet es, ob loopNormal()
// am Ende der Iteration genau EIN FastLED.show() ausführt. Mehrere show() in
// schneller Folge verschieben beim WS2811 sonst den ersten Pixel / mischen Farben.
bool ledsDirty = false;

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
uint8_t lastVolumeDuty = 0;        // Letzter Lautstärke-Duty (für Bewegungserkennung)
bool potiFeedbackEnabled = false;  // Vorschau erst nach erstem Messzyklus (kein Feedback beim Start)

// Helligkeits-Vorschau: beim Drehen am Helligkeits-Poti "888" anzeigen, damit die
// Reglereinstellung sofort sichtbar ist (analog zum Lautstärke-Vorhörton). Der
// vorherige Display-Inhalt wird gesichert und nach dem Nachlauf wiederhergestellt.
CRGB ledsBackup[LEDStrip::TOTAL_LEDS];
bool brightnessPreviewActive = false;
uint32_t brightnessPreviewUntil = 0;

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

    // LED Strip initialisieren (WS2811-Protokoll, RGB). Reihenfolge erst mit
    // sauberem Datensignal (330-Ω-Serienwiderstand am Daten-Pin) zuverlässig
    // bestimmt: feste Helligkeit + einheitliche Pixel zeigten gesendet Rot→Grün,
    // Grün→Blau ⇒ Standard-RGB. Frühere Messungen waren durch gekippte Bits verfälscht.
    // 800 kHz (Standard-WS2811): 400 kHz getestet → nur sporadisches Aufblitzen,
    // also ist dies ein 800-kHz-Typ. Das Restproblem (Gelb/Weiß kippen) ist der
    // Signalpegel — Abhilfe: Pegelwandler 3,3→5 V am Daten-Pin (Hardware).
    FastLED.addLeds<WS2811, Pins::LED_STRIP, RGB>(leds, LEDStrip::TOTAL_LEDS);
    // Temporales Dithering AUS: show() wird im Normalbetrieb nur bei Änderungen
    // aufgerufen (selten). Mit Dithering + brightness<255 bleiben die LEDs dann
    // auf einem Dither-Zwischenwert "hängen" → helligkeitsabhängige Farb-/
    // Helligkeitsartefakte. Ohne Dither ist die Ausgabe deterministisch.
    FastLED.setDither(DISABLE_DITHER);

    // Initiale Helligkeit aus Poti lesen (15-100 %, FR-022). Poti verpolt →
    // invertiert (ADC klein = hell), konsistent mit updatePotis() — sonst liefe
    // der Regenbogen-Effekt mit verkehrter (oft viel zu dunkler) Helligkeit.
    uint16_t potiValue = Adc::readAveraged(Pins::BRIGHTNESS_POTI);  // gemittelt (Rauschunterdrückung)
    currentBrightness = map(potiValue, 0, 4095, LEDStrip::BRIGHTNESS_MAX, LEDStrip::BRIGHTNESS_MIN);
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

    // Task-Watchdog (Schutz gegen Einfrieren bei Unterspannung): Wenn loopNormal()
    // länger als WDT_TIMEOUT_MS keinen Reset mehr abgibt (z.B. weil die CPU bei
    // einbrechender Versorgung hängt), erzwingt der WDT einen sauberen Neustart —
    // besser als eine mitten in der Passe eingefrorene Ampel. Ersetzt NICHT eine
    // ausreichende Stromversorgung; er fängt nur den Hang-Fall ab.
    // Registrierung erst hier — der blockierende Regenbogen-Start (~5 s) läuft davor.
    constexpr uint32_t WDT_TIMEOUT_MS = 5000;
    esp_task_wdt_config_t wdtConfig = {
        .timeout_ms = WDT_TIMEOUT_MS,
        .idle_core_mask = 0,        // Idle-Tasks nicht überwachen
        .trigger_panic = true       // bei Timeout: Panic → Reset
    };
    // Arduino hat den TWDT ggf. schon initialisiert → dann nur umkonfigurieren.
    if (esp_task_wdt_init(&wdtConfig) == ESP_ERR_INVALID_STATE) {
        esp_task_wdt_reconfigure(&wdtConfig);
    }
    esp_task_wdt_add(NULL);  // loopTask überwachen

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
    // Service-/Programmiermodus: OTA-bereit UND LED-Debug-Animation auf der
    // großen Anzeige (Verschaltungs-/Pixeltest). Start dunkel, die Animation in
    // loopOta() übernimmt sofort den ersten Frame.
    FastLED.addLeds<WS2811, Pins::LED_STRIP, RGB>(leds, LEDStrip::TOTAL_LEDS);
    FastLED.setDither(DISABLE_DITHER);  // siehe setupNormal: deterministische Ausgabe
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
 * @brief OTA-Wartungsmodus-Loop: OTA bedienen + LED-Debug-Animation (FR-002).
 *
 * OTAManager::handle() erledigt WiFi-Reconnect-Retry, ArduinoOTA und das
 * Status-LED-Signal (FR-004a-c). KEIN delay() hier, damit ArduinoOTA.handle()
 * den laufenden Flash nicht aushungert (FR-004c läuft währenddessen).
 * updateLedDebugAnimation() zeigt zusätzlich den Verschaltungstest auf der
 * großen Anzeige (Farbwechsel alle 5 s, nicht-blockierend).
 */
void loopOta() {
    OTAManager::handle();
    updateLedDebugAnimation();
}

/**
 * @brief LED-Debug-Animation für den Service-/Programmiermodus.
 *
 * Setzt ALLE Pixel (beide Gruppen + alle Segmente) gleichzeitig auf eine Farbe
 * und wechselt im 5-Sekunden-Takt Rot → Grün → Gelb (Endlosschleife). Da bei
 * korrekter Verschaltung alle Pixel identisch leuchten, fallen tote Pixel,
 * falsche Farbreihenfolge oder Verdrahtungsfehler sofort auf. Die Helligkeit
 * folgt live dem Helligkeits-Poti. Nicht-blockierend (millis-getaktet), damit
 * ArduinoOTA parallel bedient werden kann.
 */
void updateLedDebugAnimation() {
    constexpr uint32_t COLOR_INTERVAL_MS = 3000;  // Farbwechsel
    constexpr uint32_t REFRESH_MS = 100;          // Helligkeit/show() (10 Hz)
    // DIAGNOSE: reine Einzelkanäle mit dunklem Trenner. Nach dem dunklen Frame
    // folgt immer logisch ROT → GRÜN → BLAU. Helligkeit live vom Poti, damit
    // sichtbar wird, ob nach dem Dither-Fix nur noch die Helligkeit reagiert
    // (Farbe stabil) und ob der Poti-Wert noch mehrfach über den Bereich springt.
    static const CRGB colors[4] = {
        CRGB::Black,        // Sync-Marker (kurz dunkel)
        CRGB(255, 0, 0),    // logisch ROT
        CRGB(0, 255, 0),    // logisch GRÜN
        CRGB(0, 0, 255)     // logisch BLAU
    };
    static uint32_t lastColor = 0;
    static uint32_t lastRefresh = 0;
    static uint8_t colorIndex = 1;   // mit ROT starten
    static bool first = true;

    uint32_t now = millis();
    if (!first && (now - lastRefresh < REFRESH_MS)) {
        return;  // noch im aktuellen Refresh-Fenster
    }
    lastRefresh = now;

    if (first || (now - lastColor >= COLOR_INTERVAL_MS)) {
        if (!first) colorIndex = (colorIndex + 1) % 4;
        first = false;
        lastColor = now;
    }

    uint16_t potiValue = Adc::readAveraged(Pins::BRIGHTNESS_POTI);
    uint8_t brightness = map(potiValue, 0, 4095, LEDStrip::BRIGHTNESS_MAX, LEDStrip::BRIGHTNESS_MIN);
    FastLED.setBrightness(brightness);

    fill_solid(leds, LEDStrip::TOTAL_LEDS, colors[colorIndex]);
    FastLED.show();
}

/**
 * @brief Gibt den aktuellen leds[]-Frame aus — mit Soft-Start gegen Inrush.
 *
 * Der 12-V-Streifen hängt über einen Step-up an der USB-5-V-Schiene. Gehen viele
 * Pixel gleichzeitig von dunkel auf hell (Boot/erstes Bild, Passenstart "alles
 * rot", Alarm-Blitz, "888"-Vorschau), zieht der Step-up den Strom schlagartig aus
 * dem USB — die 5 V brechen kurz ein und der ESP kann per Brown-out resetten.
 *
 * Abhilfe: nur bei einem deutlichen LED-Lastanstieg wird der Helligkeitssprung
 * über wenige Zwischenstufen (~55 ms) gerampt, damit der Step-up den Strom
 * allmählich statt schlagartig anfordert. Kleine Änderungen und jedes Abschalten
 * werden weiterhin sofort ausgegeben (kein sichtbares Nachziehen im Countdown).
 */
void showLedsSoftStart() {
    // Grobe Lastschätzung: Summe (R+G+B) über alle Pixel, VOR globaler Helligkeit —
    // ein Maß dafür, wie viele Kanäle gleichzeitig aktiv sind (∝ Strombedarf).
    uint32_t load = 0;
    for (uint16_t i = 0; i < LEDStrip::TOTAL_LEDS; i++) {
        load += leds[i].r + leds[i].g + leds[i].b;
    }

    // Rampen nur bei nennenswertem Anstieg (~30 Pixel gehen voll an). Abschalten
    // oder kleine Deltas → sofort, ohne Verzögerung.
    static uint32_t lastLoad = 0;
    constexpr uint32_t RAMP_THRESHOLD = 30UL * 255;
    if (load > lastLoad + RAMP_THRESHOLD) {
        const uint8_t target = FastLED.getBrightness();
        constexpr uint8_t STEPS = 8;
        for (uint8_t s = 1; s <= STEPS; s++) {
            // FastLED.show(scale) gibt mit temporärer globaler Helligkeit aus,
            // ohne die per setBrightness gesetzte Helligkeit zu verändern.
            FastLED.show((uint8_t)((uint16_t)target * s / STEPS));
            delay(7);  // 8 × 7 ms ≈ 55 ms Gesamt-Rampe
        }
    } else {
        FastLED.show();
    }
    lastLoad = load;
}

/**
 * @brief Normalbetrieb-Loop: ESP-NOW + Timer/Anzeige (unverändert aus V2).
 *        Es wird KEIN OTAManager::handle() aufgerufen (FR-006).
 */
void loopNormal() {
    // Task-Watchdog bedienen: solange der Loop normal durchläuft, kein Reset.
    esp_task_wdt_reset();

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

    // Helligkeits-Vorschau ("888") nach Nachlauf wieder ausblenden
    updateBrightnessPreview();

    // Status-LED: an = bereit, kurzes Aus-Blinken bei Frame-Empfang
    updateStatusLed();

    // Zentrales, einziges FastLED.show() pro Iteration: erst jetzt — nachdem
    // alle Anzeige-Updates ins leds[]-Array geschrieben haben — wird der Frame
    // ausgegeben. Genau ein show() statt mehrerer in schneller Folge verhindert
    // beim WS2811 die Pixel-Verschiebung (Geisterpixel) und Farbmischung (z.B.
    // Rot → Pink), siehe DisplayManager::isDirty().
    if (ledsDirty || display.isDirty()) {
        showLedsSoftStart();  // Inrush-begrenzte Ausgabe (Brown-out-Schutz)
        ledsDirty = false;
        display.clearDirty();
    }

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

    const bool firstRead = !potiFeedbackEnabled;

    // --- Lautstärke (D0) — Poti verpolt: ADC klein = laut, ADC groß = aus.
    // Quadratische Kennlinie über den INVERTIERTEN Rohwert (gleiche Steilheit wie
    // bisher); ab Volume::OFF_THRESHOLD (nahe Maximum) komplett stumm (duty 0).
    uint32_t rawVol = Adc::readAveraged(Pins::VOLUME_POTI);  // gemittelt (Rauschunterdrückung)
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

    // Lautstärke-Feedback: kontinuierlicher Vorhörton beim Drehen am Poti. Der Ton
    // folgt live der eingestellten Lautstärke (setVolume oben) und verstummt max. 1 s
    // nach der letzten Bewegung (Hysterese 4 Stufen gegen ADC-Rauschen).
    // Beim allerersten Messzyklus (Initialisierung aus setupNormal()) KEIN Ton:
    // sonst liefe er während des blockierenden Regenbogen-Effekts (dort wird kein
    // buzzer.update() aufgerufen) durch → die Ampel würde beim Einschalten
    // durchgehend piepen.
    uint8_t volDelta = (duty > lastVolumeDuty) ? duty - lastVolumeDuty : lastVolumeDuty - duty;
    if (!firstRead && volDelta >= 4) {
        // Kein Vorhörton während eines laufenden Countdowns (Timer oder
        // Vorbereitungsphase) — würde die Schießphase akustisch stören.
        // Die Lautstärke wird über setVolume oben trotzdem live übernommen.
        if (!timerRunning && !inPreparationPhase) {
            buzzer.startPreview(Timing::PREVIEW_HOLD_MS);
        }
        lastVolumeDuty = duty;
    } else if (firstRead) {
        lastVolumeDuty = duty;         // Startwert übernehmen, ohne Ton
    }

    // --- Helligkeit (D1, linear 15-100 %, Poti verpolt → invertiert: ADC klein = hell) ---
    uint16_t rawBright = Adc::readAveraged(Pins::BRIGHTNESS_POTI);  // gemittelt (Rauschunterdrückung)
    uint8_t newBrightness = map(rawBright, 0, 4095, LEDStrip::BRIGHTNESS_MAX, LEDStrip::BRIGHTNESS_MIN);

    // Nur aktualisieren wenn sich Helligkeit signifikant geändert hat (Hysterese)
    if (abs((int)newBrightness - (int)currentBrightness) > 3) {
        currentBrightness = newBrightness;
        FastLED.setBrightness(currentBrightness);

        // Helligkeits-Vorschau: "888" anzeigen, solange gedreht wird (+ Nachlauf),
        // damit die Reglereinstellung sofort sichtbar ist — analog zum Lautstärke-
        // Vorhörton. Nicht beim Start (firstRead) und nicht während Countdown/Alarm.
        if (!firstRead && !timerRunning && !inPreparationPhase && !alarmActive) {
            if (!brightnessPreviewActive) {
                // Aktuellen Display-Inhalt sichern und Testbild zeichnen
                memcpy(ledsBackup, leds, sizeof(ledsBackup));
                brightnessPreviewActive = true;
                display.displayTimer(888, CRGB::Red, true);  // alle Segmente an (rot)
                display.setGroup(0, CRGB::Black);
                display.setGroup(1, CRGB::Black);
            }
            brightnessPreviewUntil = millis() + Timing::PREVIEW_HOLD_MS;
        }
        ledsDirty = true;  // setBrightness + ggf. "888"-Vorschau → show() in loop()
    }

    potiFeedbackEnabled = true;
}

//=============================================================================
// Helligkeits-Vorschau ("888" beim Drehen am Helligkeits-Poti)
//=============================================================================

/**
 * @brief Beendet die Helligkeits-Vorschau nach dem Nachlauf und stellt den
 *        vorherigen Display-Inhalt wieder her (nicht-blockierend)
 *
 * Übernimmt ein Countdown oder Alarm das Display, wird die Vorschau verworfen
 * (ohne Restore — der neue Zustand wurde bereits gezeichnet).
 */
void updateBrightnessPreview() {
    if (!brightnessPreviewActive) return;

    // Countdown/Alarm hat das Display übernommen → Vorschau ohne Restore beenden
    if (timerRunning || inPreparationPhase || alarmActive) {
        brightnessPreviewActive = false;
        return;
    }

    // Nachlauf abgelaufen → vorherigen Display-Inhalt wiederherstellen
    if (millis() >= brightnessPreviewUntil) {
        memcpy(leds, ledsBackup, sizeof(ledsBackup));
        ledsDirty = true;  // restaurierter Inhalt → show() in loop()
        brightnessPreviewActive = false;
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
            ledsDirty = true;  // show() in loop()
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
            ledsDirty = true;  // show() in loop()
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
    // Laufende Helligkeits-Vorschau ("888") verfällt, sobald ein Kommando den
    // Anzeigezustand neu setzt (kein Restore — das Kommando zeichnet neu). PING
    // ändert die Anzeige nicht und lässt die Vorschau weiterlaufen.
    if (cmd != CMD_PING) {
        brightnessPreviewActive = false;
    }

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
            ledsDirty = true;  // show() in loop()
            alarmLedState = true;

            // Akustisches Signal: 8x Piepen (Alarm)
            buzzer.beep(8);
            break;

        default:
            DEBUG_PRINTLN("UNK");
            break;
    }
}
