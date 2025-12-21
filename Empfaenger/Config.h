/**
 * @file Config.h
 * @brief Zentrale Konfigurationsdatei für Bogenampel Empfänger
 *
 * Enthält alle Hardware-Pin-Definitionen, Timing-Konstanten und
 * Konfigurationsparameter für den Empfänger (Anzeigeeinheit).
 *
 * Hardware: Arduino Nano V3
 * - 3x Status-LEDs (Grün, Gelb, Rot)
 * - NRF24L01 Funkmodul
 * - 1x Debug-Taster
 * - (Später: LED Strip, Buzzer)
 *
 * @date 2025-12-14
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <RF24.h>  // Für rf24_pa_dbm_e und rf24_datarate_e

//=============================================================================
// HARDWARE PIN-DEFINITIONEN
//=============================================================================

namespace Pins {

    //-------------------------------------------------------------------------
    // SPI-Bus (für NRF24L01)
    //-------------------------------------------------------------------------
    constexpr uint8_t SPI_SCK  = 13;  // Hardware SPI Clock
    constexpr uint8_t SPI_MOSI = 11;  // Hardware SPI Master Out Slave In
    constexpr uint8_t SPI_MISO = 12;  // Hardware SPI Master In Slave Out

    //-------------------------------------------------------------------------
    // NRF24L01 Funkmodul
    //-------------------------------------------------------------------------
    constexpr uint8_t NRF_CE   = 9;   // NRF24 Chip Enable (D9)
    constexpr uint8_t NRF_CSN  = 8;   // NRF24 Chip Select (D8)

    //-------------------------------------------------------------------------
    // Ausgänge: Status-LEDs
    //-------------------------------------------------------------------------
    constexpr uint8_t LED_GREEN  = A2;  // D1: Grüne LED (Bereit/Aktiv)
    constexpr uint8_t LED_YELLOW = A3;  // D2: Gelbe LED (Empfang/Warnung)
    constexpr uint8_t LED_RED    = A4;  // D3: Rote LED (Stop/Alarm)

    //-------------------------------------------------------------------------
    // Ausgänge: Signalgeber
    //-------------------------------------------------------------------------
    constexpr uint8_t BUZZER     = 4;   // D6: KY-006 Passiver Piezo Buzzer

    //-------------------------------------------------------------------------
    // Ausgänge: WS2812B LED Strip
    //-------------------------------------------------------------------------
    constexpr uint8_t LED_STRIP  = 3;   // D3: WS2812B Data Pin

    //-------------------------------------------------------------------------
    // Eingänge: Taster (mit internem Pull-Up, aktiv LOW)
    //-------------------------------------------------------------------------
    constexpr uint8_t BTN_DEBUG   = 5;   // J1: Debug-Taster
    constexpr uint8_t DEBUG_JUMPER = 2;  // D2: Debug-Jumper (LOW = Debug-Modus)

} // namespace Pins

//=============================================================================
// RF KOMMUNIKATION (NRF24L01)
//=============================================================================

namespace RF {

    // RF-Kanal (MUSS IDENTISCH MIT SENDER SEIN!)
    constexpr uint8_t CHANNEL = 76;  // 2.476 GHz

    // RF-Datenrate (verwende RF24-Library Enums direkt)
    // RF24_250KBPS = robuster bei schlechten Verbindungen/langen Kabeln!
    constexpr rf24_datarate_e DATA_RATE = RF24_250KBPS;

    // RF-Power Level (verwende RF24-Library Enums direkt)
    // RF24_PA_MAX = 0dBm (höchste Leistung, ~50m Reichweite)
    // WICHTIG: Benötigt externe 3.3V Versorgung (AMS1117) + 100µF Kondensator!
    constexpr rf24_pa_dbm_e POWER_LEVEL = RF24_PA_MIN;

    // Pipe-Adressen (5 Bytes) - MUSS IDENTISCH MIT SENDER SEIN!
    const uint8_t PIPE_ADDRESS[5] PROGMEM = {'B', '4', 'M', 'P', 'L'};  // "BAMPL" = Bogenampel

    // Auto-ACK aktiviert (Empfänger sendet automatisch ACK zurück an Sender)
    constexpr bool AUTO_ACK_ENABLED = true;

    // Retry-Einstellungen (muss mit Sender übereinstimmen)
    constexpr uint8_t RETRY_DELAY = 5;    // Delay: (delay + 1) * 250µs = 1.5ms
    constexpr uint8_t RETRY_COUNT = 15;   // Max 15 Retries

    // Payload-Größe
    constexpr uint8_t PAYLOAD_SIZE = 2;   // 2 Bytes (Command + Checksum)

} // namespace RF

//=============================================================================
// TIMING-KONSTANTEN
//=============================================================================

namespace Timing {

    // LED-Feedback
    constexpr uint16_t LED_BLINK_DURATION_MS = 100;  // Kurzes Blinken bei Empfang

    // Button Debouncing
    constexpr uint8_t DEBOUNCE_MS = 50;  // 50ms Entprellzeit

    // Buzzer-Feedback
    constexpr uint16_t BUZZER_BEEP_DURATION_MS = 1000;  // Dauer eines Pieptons
    constexpr uint16_t BUZZER_FREQUENCY_HZ = 2700;     // Frequenz des Piezo-Tons (2kHz)

} // namespace Timing

//=============================================================================
// LED STRIP KONFIGURATION (WS2812B)
//=============================================================================

namespace LEDStrip {

    // LED Strip Konfiguration:
    // - 16 LEDs für Gruppe A/B (LED 1-16, Array Index 0-15)
    // - 16 LEDs für Gruppe C/D (LED 17-32, Array Index 16-31)
    // - 3 Digits × 7 Segmente × 6 LEDs = 126 LEDs (LED 33-158, Array Index 32-157)
    // Total: 158 LEDs

    constexpr uint8_t GROUP_AB_LEDS = 16;      // LEDs 1-16 (Index 0-15)
    constexpr uint8_t GROUP_CD_LEDS = 16;      // LEDs 17-32 (Index 16-31)
    constexpr uint8_t GROUP_AB_START = 0;      // Start-Index für Gruppe A/B
    constexpr uint8_t GROUP_CD_START = 16;     // Start-Index für Gruppe C/D

    constexpr uint8_t LEDS_PER_SEGMENT = 6;    // 6 LEDs pro 7-Segment-Balken
    constexpr uint8_t SEGMENTS_PER_DIGIT = 7;  // 7 Segmente pro Ziffer (B, A, F, G, C, D, E)
    constexpr uint8_t NUM_DIGITS = 3;          // 3 Ziffern (1er, 10er, 100er)
    constexpr uint8_t DIGIT_START = 32;        // Start-Index der 7-Segment-Anzeigen (nach A/B + C/D)

    constexpr uint8_t LEDS_PER_DIGIT = LEDS_PER_SEGMENT * SEGMENTS_PER_DIGIT;  // 42 LEDs pro Ziffer
    constexpr uint8_t TOTAL_LEDS = GROUP_AB_LEDS + GROUP_CD_LEDS + (NUM_DIGITS * LEDS_PER_DIGIT);  // 158 LEDs

    // Start-Indizes für die einzelnen Ziffern
    // Physische Hardware-Anordnung im Strip:
    // - LED 33-74 (Index 32): Erste Display-Position → 1er-Stelle
    // - LED 75-116 (Index 74): Zweite Display-Position → 10er-Stelle
    // - LED 117-158 (Index 116): Dritte Display-Position → 100er-Stelle
    constexpr uint8_t DIGIT_1_START = DIGIT_START;                              // LED 33 (Index 32): 1er-Stelle (links)
    constexpr uint8_t DIGIT_10_START = DIGIT_START + LEDS_PER_DIGIT;           // LED 75 (Index 74): 10er-Stelle (mitte)
    constexpr uint8_t DIGIT_100_START = DIGIT_START + (2 * LEDS_PER_DIGIT);    // LED 117 (Index 116): 100er-Stelle (rechts)

    // Helligkeit (0-255)
    constexpr uint8_t BRIGHTNESS_NORMAL = 255;  // 100% Helligkeit
    constexpr uint8_t BRIGHTNESS_DEBUG = 64;    // 25% Helligkeit (255 * 0.25 = 64)

} // namespace LEDStrip

//=============================================================================
// GRUPPEN-DEFINITIONEN (für 3-4 Schützen Modus)
//=============================================================================

namespace Groups {

    // Gruppen-Typen
    enum class Type : uint8_t {
        GROUP_AB = 0,  // Gruppe A/B
        GROUP_CD = 1   // Gruppe C/D
    };

    // Positions-Marker für 4-State Cycle
    enum class Position : uint8_t {
        POS_1 = 1,  // Position 1 (erste Hälfte der Passe)
        POS_2 = 2   // Position 2 (zweite Hälfte der Passe)
    };

} // namespace Groups

//=============================================================================
// SYSTEMKONSTANTEN
//=============================================================================

namespace System {

    // Versionsinformation (im Flash gespeichert)
    const char VERSION[] PROGMEM = "Bogenampel Empfaenger V1.0";
    const char BUILD_DATE[] PROGMEM = __DATE__;
    const char BUILD_TIME[] PROGMEM = __TIME__;

    // Serial Baud Rate (für Debugging)
    constexpr uint32_t SERIAL_BAUD = 57600;

    // Debugging aktivieren/deaktivieren
    #define DEBUG_ENABLED 1  // 1 = Debug-Ausgaben an, 0 = aus

    // Verkürzte Zeiten für Tests (nur wenn DEBUG_ENABLED = 1)
    #define DEBUG_SHORT_TIMES 0  // 1 = Verkürzte Zeiten, 0 = Normale Zeiten

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
// HELPER MAKROS
//=============================================================================

// Flash-String-Helper (PROGMEM)
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))

//=============================================================================
// ENDE DER KONFIGURATION
//=============================================================================
