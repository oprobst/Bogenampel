/**
 * @file SenderV3.cpp
 * @brief Hauptdatei für Bogenampel V3 Sender (Bedieneinheit)
 *
 * Hardware: ESP32-S3-WROOM-1U-N16R8, 1.54" e-Paper (SSD1681), ESP-NOW,
 * LiPo + MCP73837-Lader, One-Button-Soft-Power (TPS62742-Latch).
 *
 * Setup-Reihenfolge (kritisch!):
 * 1. LATCH HIGH — allererste Aktion, sonst geht das Gerät beim Loslassen
 *    von BTN1 wieder aus (FR-014, R-13)
 * 2. ConfigStore (NVS) → 3. Buttons (Boot-Lockout) → 4. LOAD-Rail + Display
 * 5. Funk (ESP-NOW + Discovery) → 6. StateMachine (Splash)
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

    // Funk: ESP-NOW init + Discovery-Start (HELLO-Broadcast läuft in update())
    if (!radio.begin()) {
        DEBUG_PRINTLN("FEHLER: ESP-NOW init fehlgeschlagen!");
        // Weiterbooten — die StateMachine zeigt "keine Verbindung" (US5)
    }

    // OTA-SoftAP starten (nach radio.begin, WiFi ist bereits im AP_STA-Modus)
    OTAManager::begin();

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

    DEBUG_PRINTLN("Setup abgeschlossen");
}

//=============================================================================
// Loop
//=============================================================================

void loop() {
    // OTA bedienen (muss vor allem anderen laufen, damit der Upload nicht abbricht)
    OTAManager::handle();

    // Button Manager Update (immer zuerst — Gesten/Boot-Lockout)
    buttons.update();

    // Funk: Discovery vorantreiben / HELLO_ACK verarbeiten
    radio.update();

    // State Machine Update (verwaltet alle States inkl. Splash und Power-Off)
    stateMachine.update();

    // Kleine Pause um CPU zu entlasten
    delay(10);
}
