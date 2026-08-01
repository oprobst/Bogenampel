/**
 * @file Config.h
 * @brief Zentrale Konfigurationsdatei für Bogenampel V3 Sender (Bedieneinheit)
 *
 * Enthält alle Hardware-Pin-Definitionen, Timing-Konstanten und
 * Konfigurationsparameter für den Sender.
 *
 * Hardware: ESP32-S3-WROOM-1U-N16R8 ("Universal-Fernbedienung", IC2)
 * - Waveshare 1.54" e-Paper V2 (SSD1681, 200x200) als rohes Panel an J1
 * - ESP-NOW Funk (integriert, kein externes Modul)
 * - 2 Taster (SW2/GPIO15 = CONFIG + Einschalten, SW1/GPIO9 = OK), Power-Latch (TPS62742)
 * - LiPo-Lader MCP73837, Batteriespannungsmessung über Teiler
 *
 * Verbindliche Pin-Quelle: specs/004-v3-esp32-port/contracts/hardware-pins.md
 * (extrahiert aus Schaltung_v3/BogenampelV3/BogenampelV3.kicad_sch Rev 3.0)
 *
 * @date 2026-06-11
 * @version 3.0
 */

#pragma once

#include <Arduino.h>

//=============================================================================
// DEBUG-KONFIGURATION (muss VOR allen anderen Definitionen stehen!)
//=============================================================================

// Debugging aktivieren/deaktivieren
#define DEBUG_ENABLED 1  // 1 = Debug-Ausgaben an (USB-CDC), 0 = aus

// Verkürzte Zeiten für Tests (nur wenn DEBUG_ENABLED = 1)
#define DEBUG_SHORT_TIMES 0  // 1 = Verkürzte Zeiten, 0 = Normale Zeiten

//=============================================================================
// HARDWARE PIN-DEFINITIONEN
//=============================================================================

namespace Pins {

    //-------------------------------------------------------------------------
    // Power-Management (TPS62742 + Latch)
    //-------------------------------------------------------------------------
    constexpr uint8_t LATCH = 16;  // über D1 (BAS40-05) an EN des TPS62742;
                                   // ERSTE Aktion in setup(): OUTPUT + HIGH!
                                   // LOW = Selbstabschaltung
    constexpr uint8_t LOAD  = 7;   // TPS62742 LOAD-Eingang: schaltet 3V3_LOAD-Rail
                                   // fürs Display; HIGH vor Display-Init, LOW vor Power-Off

    //-------------------------------------------------------------------------
    // Taster (SW2 = BTN1, SW1 = BTN2) — die Namen bezeichnen den PIN, nicht die
    // Bedienrolle. Die Rollen sind in ButtonManager::readRawState() zugeordnet
    // und wurden an die Gehäusebeschriftung angepasst:
    //   BTN1 (GPIO15) → Rolle CONFIG (Weiter/Ändern), zugleich Einschalt-Taster
    //   BTN2 (GPIO9)  → Rolle OK (Bestätigen, Alarm 2s, Power-Off 3s)
    // Welcher Taster einschaltet, legt der Power-Latch in der Hardware fest.
    //-------------------------------------------------------------------------
    constexpr uint8_t BTN1 = 15;   // SW2: AKTIV HIGH (externer Teiler
                                   // R2/R7 an +BATT, 100nF C1); KEINE internen Pulls!
    constexpr uint8_t BTN2 = 9;    // SW1: gegen GND, INPUT_PULLUP, aktiv LOW

    //-------------------------------------------------------------------------
    // Batterie / USB / Lader (MCP73837)
    //-------------------------------------------------------------------------
    constexpr uint8_t ADC_BAT = 10;  // ADC1_CH9, Teiler R4/R8 = 150k/100k an +BATT
                                     // → V_BAT = V_ADC × 2,5 (Sensor stromlos wenn aus)
    constexpr uint8_t USB_CON = 8;   // VBUS-Teiler (R16/R18), aktiv HIGH bei USB
    constexpr uint8_t C_ST1   = 11;  // Lader STAT1 (Open-Drain → INPUT_PULLUP)
    constexpr uint8_t C_ST2   = 12;  // Lader STAT2 (Open-Drain → INPUT_PULLUP)
    constexpr uint8_t C_PG    = 17;  // Lader Power Good (Open-Drain → INPUT_PULLUP)
    constexpr uint8_t C_PRG   = 18;  // Ladestrom-Umschaltung über R20 an PROG2;
                                     // bleibt INPUT (hochohmig) = Default-Ladestrom

    //-------------------------------------------------------------------------
    // e-Paper (Waveshare 1.54" V2 / SSD1681, rohes Panel an J1, write-only SPI)
    //-------------------------------------------------------------------------
    constexpr uint8_t EPD_CS   = 21;  // J1 Pin 12 (CS_D)
    constexpr uint8_t EPD_DC   = 47;  // J1 Pin 11
    constexpr uint8_t EPD_RST  = 48;  // J1 Pin 10
    constexpr uint8_t EPD_BUSY = 38;  // J1 Pin 9
    constexpr uint8_t SPI_CLK  = 14;  // J1 Pin 13
    constexpr uint8_t SPI_MOSI = 13;  // J1 Pin 14 (kein MISO — e-Paper write-only)

    // Hinweis: GPIO19/20 = natives USB (CDC für Debug/Flash), GPIO35-37 = Octal-PSRAM
    // (N16R8) — NICHT benutzen!

} // namespace Pins

//=============================================================================
// DISPLAY KONFIGURATION (e-Paper, 200x200)
//=============================================================================

namespace Display {

    // Panel-Auflösung (GDEH0154D67 / SSD1681)
    constexpr uint16_t WIDTH  = 200;
    constexpr uint16_t HEIGHT = 200;

    // Display-Orientierung (GxEPD2: 0-3) — 2 = um 180° gedreht
    constexpr uint8_t ROTATION = 2;

    // Partial-Refresh-Fenster (data-model.md §7)
    // Statuszeile (oben): Akku %, USB-/Lade-Symbol, Funkstatus
    constexpr uint16_t STATUS_X = 0;
    constexpr uint16_t STATUS_Y = 0;
    constexpr uint16_t STATUS_W = 200;
    constexpr uint16_t STATUS_H = 24;

    // Countdown-Fenster (zentriert): mm:ss bzw. Sekunden, 1 Hz partial
    constexpr uint16_t COUNTDOWN_X = 40;
    constexpr uint16_t COUNTDOWN_Y = 76;
    constexpr uint16_t COUNTDOWN_W = 120;
    constexpr uint16_t COUNTDOWN_H = 64;

    // Ghosting-Budget: So viele Partial-Refreshes (Fenster + Screenwechsel)
    // dürfen aufeinander folgen, bevor der nächste Screenwechsel einmal voll
    // durchblitzt und die Restschatten löscht (R-2).
    constexpr uint8_t PARTIAL_REFRESH_LIMIT = 20;

} // namespace Display

//=============================================================================
// OTA (WiFi-Station im Wartungsmodus + ArduinoOTA)
//=============================================================================

// wifi_credentials.h (gitignored) definiert WIFI_SSID_OVERRIDE + WIFI_PASS_OVERRIDE.
// Vorlage: wifi_credentials.h.example im Repo-Root.
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#endif

namespace OTA {

    constexpr const char* HOSTNAME = "bogenampel-sender";

    // Heimnetz-WLAN für den Wartungsmodus — aus wifi_credentials.h. Ohne
    // Zugangsdaten bleibt der Wartungsmodus in der CONNECTING-Anzeige stehen;
    // einen SoftAP-Fallback gibt es bewusst nicht (Kanalkonflikt-Vermeidung,
    // identisch zum Empfänger).
    // Kanal-Hinweis: ESP-NOW läuft auf Kanal 1; sobald sich der Sender in ein
    // Netz auf einem anderen Kanal einbucht, ist ESP-NOW tot. Genau deshalb
    // sind Wartungsmodus und Normalbetrieb strikt getrennt.
#ifdef WIFI_SSID_OVERRIDE
    constexpr const char* WIFI_SSID = WIFI_SSID_OVERRIDE;
    constexpr const char* WIFI_PASS = WIFI_PASS_OVERRIDE;
#else
    constexpr const char* WIFI_SSID = "";
    constexpr const char* WIFI_PASS = "";
#endif
    constexpr uint16_t WIFI_TIMEOUT = 10000;  // ms bis zum nächsten Verbindungsversuch

} // namespace OTA

//=============================================================================
// FUNK (ESP-NOW)
//=============================================================================

namespace Radio {

    // WLAN-Kanal (fest, beide Geräte identisch — espnow-protocol.md)
    constexpr uint8_t CHANNEL = 1;

    // Transport-Retry (Send-Callback NACK → erneut senden)
    constexpr uint8_t MAX_RETRIES = 3;          // max. 3 Versuche
    constexpr uint16_t RETRY_DELAY_MS = 50;     // 50 ms Abstand
    constexpr uint16_t TRANSMIT_TIMEOUT_MS = 500;  // Gesamtbudget pro Kommando (FR-007)

    // Discovery (FT_HELLO-Broadcast bis FT_HELLO_ACK)
    constexpr uint16_t HELLO_INTERVAL_MS = 1000;   // max. 1 Hz (Anfangsphase)

    // Backoff: Ist nach HELLO_FAST_COUNT Versuchen kein Empfänger aufgetaucht,
    // ist er vermutlich aus. Dann genügt ein langsamer Suchlauf — das hält den
    // Log lesbar und spart Sendezeit. Beim Einschalten des Empfängers dauert die
    // Erkennung dann max. HELLO_SLOW_INTERVAL_MS.
    constexpr uint8_t  HELLO_FAST_COUNT = 10;         // ~10 s im 1-Hz-Raster
    constexpr uint16_t HELLO_SLOW_INTERVAL_MS = 5000; // danach alle 5 s

    // Nur die ersten HELLO_LOG_COUNT Broadcasts werden geloggt — danach meldet
    // sich die Discovery erst wieder, wenn ein Empfänger antwortet.
    constexpr uint8_t HELLO_LOG_COUNT = 3;

    // Kanal-Wächter: Prüfintervall für den tatsächlichen WLAN-Home-Channel
    constexpr uint16_t CHANNEL_CHECK_MS = 2000;

    // Connection Quality Test (Splash, R-5)
    constexpr uint8_t QUALITY_TEST_PINGS = 10;        // Anzahl Pings
    constexpr uint16_t QUALITY_TEST_INTERVAL_MS = 250;  // 250 ms Raster (10 in 2,5 s)

} // namespace Radio

//=============================================================================
// BATTERIE-ÜBERWACHUNG
//=============================================================================

namespace Battery {

    // LiPo-Spannungsgrenzen (in Millivolt, V2-Schwellen)
    constexpr uint16_t VOLTAGE_MIN_MV = 3000;   // 3.0V = 0% (LiPo Cutoff)
    constexpr uint16_t VOLTAGE_MAX_MV = 4200;   // 4.2V = 100% (LiPo voll)
    constexpr uint16_t VOLTAGE_LOW_MV = 3300;   // 3.3V = ~10% (Low Battery Warnung)

    // Spannungsteiler R4/R8 = 150k/100k → V_BAT = V_ADC × 2,5
    constexpr float DIVIDER_RATIO = 2.5f;

    // Median-Filter Größe
    constexpr uint8_t FILTER_SIZE = 5;  // 5 Messwerte für Median

    // Aktualisierungsintervall (Millisekunden)
    constexpr uint16_t UPDATE_INTERVAL_MS = 5000;  // Alle 5 Sekunden

} // namespace Battery

//=============================================================================
// TIMING-KONSTANTEN
//=============================================================================

namespace Timing {

    // Splash Screen
    constexpr uint16_t SPLASH_DURATION_MS = 15000;  // 15 Sekunden
    constexpr uint16_t QUALITY_DISPLAY_DURATION_MS = 5000;  // 5 Sekunden Qualitätsanzeige

    // Button Debouncing
    constexpr uint8_t DEBOUNCE_MS = 80;          // 80ms Entprellzeit (V2-Wert)
    constexpr uint16_t MENU_LOCKOUT_MS = 400;    // 400ms Eingabesperre nach Zustandswechsel

    // Schießbetrieb
    #if DEBUG_SHORT_TIMES
        constexpr uint16_t PREPARATION_TIME_MS = 5000;   // 5 Sekunden (DEBUG)
    #else
        constexpr uint16_t PREPARATION_TIME_MS = 10000;  // 10 Sekunden Vorbereitungsphase
    #endif

    // Tastergesten (Rolle OK)
    constexpr uint16_t ALARM_THRESHOLD_MS = 2000;    // 2s halten im Schießbetrieb = Alarm
    constexpr uint16_t POWER_OFF_HOLD_MS = 3000;     // 3s halten (außerhalb Schießbetrieb) = Aus

    // Alarm-App-Retries (V2-Werte; jede Wiederholung = neuer Frame mit neuer seq)
    constexpr uint16_t ALARM_RETRY_DELAY_MS = 200;   // 200ms zwischen Alarm-Retries
    constexpr uint8_t ALARM_MAX_RETRIES = 3;         // 3 Versuche für Alarm-Kommando

} // namespace Timing

//=============================================================================
// NVS-KONFIGURATION (ersetzt AVR-EEPROM, R-6)
//=============================================================================

namespace NVS {

    constexpr const char* NAMESPACE_NAME = "bogenampel";
    constexpr const char* KEY_SHOOTING_TIME = "timeS";  // uint16: 120 oder 240
    constexpr const char* KEY_SHOOTER_COUNT = "count";  // uint8: 2 oder 4

} // namespace NVS

//=============================================================================
// GRUPPEN-DEFINITIONEN (für Anzeige auf Display)
//=============================================================================

namespace Groups {

    // Gruppen-Typen
    enum class Type : uint8_t {
        GROUP_AB = 0,  // Gruppe A/B
        GROUP_CD = 1   // Gruppe C/D
    };

    // Gruppen-Namen
    constexpr const char* NAME_AB = "A/B";
    constexpr const char* NAME_CD = "C/D";

    // Positions-Marker für 4-State Cycle (siehe Spec 002-shooter-groups)
    enum class Position : uint8_t {
        POS_1 = 1,  // Position 1
        POS_2 = 2   // Position 2
    };

} // namespace Groups

//=============================================================================
// TURNIER-KONFIGURATION (Wertebereiche, Persistenz in ConfigStore)
//=============================================================================

namespace TournamentDefaults {

    // Gültige Werte für shootingTime (Sekunden)
    constexpr uint16_t TIME_120_SEC = 120;
    constexpr uint16_t TIME_240_SEC = 240;

    // Gültige Werte für shooterCount
    constexpr uint8_t SHOOTERS_1_2 = 2;   // Anzeige: "1-2 Schützen"
    constexpr uint8_t SHOOTERS_3_4 = 4;   // Anzeige: "3-4 Schützen"

    // Default-Werte (bei fehlenden/ungültigen NVS-Einträgen, FR-005)
    constexpr uint16_t DEFAULT_TIME = TIME_120_SEC;
    constexpr uint8_t DEFAULT_COUNT = SHOOTERS_1_2;

} // namespace TournamentDefaults

//=============================================================================
// SYSTEMKONSTANTEN
//=============================================================================

namespace System {

    // Versionsinformation
    constexpr const char* VERSION = "Bogenampel V3.0";
    constexpr const char* BUILD_DATE = __DATE__;
    constexpr const char* BUILD_TIME = __TIME__;

    // Serial Baud Rate (USB-CDC, für Debugging)
    constexpr uint32_t SERIAL_BAUD = 115200;

    #if DEBUG_ENABLED
        #define DEBUG_PRINT(...)   Serial.print(__VA_ARGS__)
        #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
        #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
    #else
        #define DEBUG_PRINT(...)
        #define DEBUG_PRINTLN(...)
        #define DEBUG_PRINTF(...)
    #endif

} // namespace System

//=============================================================================
// KONFIGURATION VALIDIEREN (zur Compile-Zeit)
//=============================================================================

namespace ConfigValidation {

    // BTN1/SW2 liegt auf ADC2 (GPIO15) → darf nur digital gelesen werden (Funk aktiv!)
    static_assert(Pins::BTN1 == 15, "BTN1 must stay on GPIO15 (digital only, ADC2!)");

    // Taster-Pins unterschiedlich
    static_assert(Pins::BTN1 != Pins::BTN2, "Button pins must be unique");

    // ADC_BAT muss auf ADC1 liegen (GPIO1-10 beim ESP32-S3)
    static_assert(Pins::ADC_BAT <= 10, "ADC_BAT must be on ADC1 (radio blocks ADC2)");

    // Partial-Fenster innerhalb des Panels
    static_assert(Display::COUNTDOWN_X + Display::COUNTDOWN_W <= Display::WIDTH,
                  "Countdown window exceeds panel width");
    static_assert(Display::COUNTDOWN_Y + Display::COUNTDOWN_H <= Display::HEIGHT,
                  "Countdown window exceeds panel height");

} // namespace ConfigValidation
