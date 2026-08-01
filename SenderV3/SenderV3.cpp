/**
 * @file SenderV3.cpp
 * @brief Hauptdatei für Bogenampel V3 Sender (Bedieneinheit)
 *
 * Hardware: ESP32-S3-WROOM-1U-N16R8, 1.54" e-Paper (SSD1681), ESP-NOW,
 * LiPo + MCP73837-Lader, One-Button-Soft-Power (TPS62742-Latch).
 *
 * Setup-Reihenfolge (kritisch!):
 * 1. LATCH HIGH — allererste Aktion, sonst geht das Gerät beim Loslassen
 *    des Einschalt-Tasters (SW2/GPIO15, Rolle CONFIG) wieder aus (FR-014, R-13)
 * 2. ConfigStore (NVS) → 3. Buttons (Boot-Lockout) → 4. LOAD-Rail + Display
 * 5. Funk (ESP-NOW + Discovery) → 6. StateMachine (Splash)
 *
 * Boot-Modus: Wird beim Einschalten zusätzlich zu CONFIG auch OK gehalten,
 * startet das Gerät im OTA-Wartungsmodus — WiFi-Station + ArduinoOTA, KEIN
 * ESP-NOW, keine StateMachine. Im Normalbetrieb läuft umgekehrt kein WiFi:
 * der ESP32 hat nur ein Radio und damit einen Kanal, den ein Netz-Join oder
 * SoftAP sonst wegzieht und ESP-NOW stilllegt.
 *
 * 1-Hz-Zeitbasis: esp_timer setzt nur ein volatile-Flag, die Verarbeitung
 * läuft im loop() — identisches Muster wie der V2-Timer1-Interrupt (R-7).
 */

#include "Config.h"
#include "Commands.h"

#include <esp_timer.h>

#include "EpaperDisplay.h"
#include "ButtonManager.h"
#include "RadioManager.h"
#include "OTAManager.h"
#include "OtaScreen.h"
#include "PowerManager.h"
#include "ConfigStore.h"
#include "StateMachine.h"

//=============================================================================
// Globale Instanzen
//=============================================================================

ButtonManager buttons;
EpaperDisplay epaper;
RadioManager radio;
PowerManager power;
ConfigStore configStore;
StateMachine stateMachine(epaper, buttons, radio, power, configStore);
OtaScreen otaScreen(epaper);

//=============================================================================
// Boot-Modus — einmal beim Start entschieden, danach fix
//=============================================================================

static bool bootOtaMode = false;

static bool readOtaModeButtons();
static void setupNormal();
static void setupOta();
static void loopNormal();
static void loopOta();
static void otaUpdateStarted();

//=============================================================================
// 1-Hz-Zeitbasis (esp_timer, R-7) — ersetzt den V2-Timer1-Interrupt
//=============================================================================

volatile bool senderSecondTick = false;     // Flag: Sekunde vergangen?
static esp_timer_handle_t secondTimer = nullptr;

/**
 * @brief Periodischer 1-s-Callback — setzt nur das Flag (wie die V2-ISR)
 */
static void onSecondTick(void* /*arg*/) {
    senderSecondTick = true;
}

/**
 * @brief Setzt den Sender-Timer zurück (für synchronen Start mit dem Empfänger)
 *
 * Äquivalent zu V2 `resetSenderTimer()` (TCNT1 = 0): Timer stoppen und neu
 * starten, damit der nächste Tick exakt 1 s nach dem CMD_START liegt.
 */
void resetSenderTimer() {
    if (secondTimer) {
        esp_timer_stop(secondTimer);
        esp_timer_start_periodic(secondTimer, 1000000ULL);
    }
    senderSecondTick = false;
}

//=============================================================================
// Setup
//=============================================================================

void setup() {
    // ALLERERSTE Aktion: Power-Latch halten! (FR-014, R-13)
    pinMode(Pins::LATCH, OUTPUT);
    digitalWrite(Pins::LATCH, HIGH);

    // Serial für Debugging (natives USB-CDC)
    #if DEBUG_ENABLED
    Serial.begin(System::SERIAL_BAUD);
    // NICHT auf Serial warten — das Gerät muss auch ohne USB booten
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("======================================");
    DEBUG_PRINTF("  %s (Sender)\n", System::VERSION);
    DEBUG_PRINTLN("======================================");
    DEBUG_PRINTF("Build: %s %s\n", __DATE__, __TIME__);
    #endif

    // Button Manager (erkennt den noch gedrückten Einschalt-Taster →
    // Boot-Lockout bis zum ersten Loslassen)
    buttons.begin();

    // Power-Management (Akku-ADC, Lader-Status, USB-Erkennung)
    power.begin();

    // e-Paper: LOAD-Rail an, dann init (FR-018)
    epaper.begin();

    // ----- Boot-Modus-Entscheidung: genau einmal, hier -----
    bootOtaMode = readOtaModeButtons();

    if (bootOtaMode) {
        DEBUG_PRINTLN("Boot-Modus: OTA-WARTUNG (CONFIG+OK gehalten)");
        setupOta();
    } else {
        DEBUG_PRINTLN("Boot-Modus: Normalbetrieb");
        setupNormal();
    }

    DEBUG_PRINTLN("Setup abgeschlossen");
}

/**
 * @brief Prüft, ob beim Einschalten BEIDE Taster gehalten werden
 *
 * Der Einschalt-Taster (SW2/GPIO15, Rolle CONFIG) hält beim Start ohnehin den
 * Latch — entscheidend ist also der zweite (SW1/GPIO9, Rolle OK). Beide werden
 * über ~25 ms abgetastet und müssen durchgehend gedrückt sein, damit ein
 * einzelner Störimpuls die Boot-Entscheidung nicht kippt. Gelesen wird hier
 * bewusst direkt am Pin: der ButtonManager entprellt erst über mehrere
 * loop()-Durchläufe, die es zu diesem Zeitpunkt noch nicht gab.
 */
static bool readOtaModeButtons() {
    constexpr uint8_t SAMPLES = 5;
    for (uint8_t i = 0; i < SAMPLES; i++) {
        if (digitalRead(Pins::BTN1) != HIGH) return false;  // CONFIG, aktiv HIGH
        if (digitalRead(Pins::BTN2) != LOW)  return false;  // OK, aktiv LOW
        delay(5);
    }
    return true;
}

/**
 * @brief Normalbetrieb: ESP-NOW + StateMachine, KEIN WiFi
 */
static void setupNormal() {
    // Funk: ESP-NOW init + Discovery-Start (HELLO-Broadcast läuft in update())
    if (!radio.begin()) {
        DEBUG_PRINTLN("FEHLER: ESP-NOW init fehlgeschlagen!");
        // Weiterbooten — die StateMachine zeigt "keine Verbindung" (US5)
    }

    // 1-Hz-Zeitbasis starten
    const esp_timer_create_args_t timerArgs = {
        .callback = &onSecondTick,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "secondTick",
        .skip_unhandled_events = true
    };
    esp_timer_create(&timerArgs, &secondTimer);
    esp_timer_start_periodic(secondTimer, 1000000ULL);

    // State Machine starten (lädt NVS-Config, zeigt Splash + Verbindungstest)
    stateMachine.begin();
}

/**
 * @brief Wartungsmodus: WiFi-Station + ArduinoOTA, KEIN ESP-NOW, kein Timer
 */
static void setupOta() {
    OTAManager::begin(otaUpdateStarted);
    otaScreen.draw();
}

/**
 * @brief Callback aus ArduinoOTA.onStart — die loop() ruht während des Transfers
 */
static void otaUpdateStarted() {
    otaScreen.showUpdating();
}

//=============================================================================
// Loop
//=============================================================================

void loop() {
    if (bootOtaMode) {
        loopOta();
    } else {
        loopNormal();
    }
}

static void loopNormal() {
    // Button Manager Update (immer zuerst — Gesten/Boot-Lockout)
    buttons.update();

    // Funk: Discovery vorantreiben / HELLO_ACK verarbeiten
    radio.update();

    // State Machine Update (verwaltet alle States inkl. Splash und Power-Off)
    stateMachine.update();

    // Kleine Pause um CPU zu entlasten
    delay(10);
}

static void loopOta() {
    // OTA zuerst — ein laufender Upload wird hier komplett abgewickelt
    OTAManager::handle();

    buttons.update();

    // Ausschalten muss auch im Wartungsmodus möglich sein: einen der beiden
    // Taster 3 s halten. Der Boot-Lockout sperrt die Geste, bis der jeweilige
    // Taster einmal losgelassen wurde — sonst schaltete der Modus-Doppeldruck
    // das Gerät nach 3 s selbst ab.
    if (buttons.wasHeldFor(Button::OK, Timing::POWER_OFF_HOLD_MS)
        || buttons.wasHeldFor(Button::CONFIG, Timing::POWER_OFF_HOLD_MS)) {
        DEBUG_PRINTLN("Wartungsmodus: Power-Off");
        epaper.clearBuffer();
        epaper.printCentered("Auf Wiedersehen!", 90, 2);
        epaper.fullRefresh();
        epaper.hibernate();
        epaper.railOff();
        power.latchOff();  // kehrt nicht zurück
    }

    // Statusfenster nachziehen (zeichnet nur bei Änderung)
    otaScreen.update();

    delay(10);
}
