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
    // Eingänge: Taster (mit internem Pull-Up, aktiv LOW)
    //-------------------------------------------------------------------------
    constexpr uint8_t BTN_DEBUG  = 5;   // J1: Debug-Taster

} // namespace Pins

//=============================================================================
// RF KOMMUNIKATION (NRF24L01)
//=============================================================================

namespace RF {

    // RF-Kanal (MUSS IDENTISCH MIT SENDER SEIN!)
    constexpr uint8_t CHANNEL = 76;  // 2.476 GHz

    // RF-Datenrate (verwende RF24-Library Enums direkt)
    constexpr rf24_datarate_e DATA_RATE = RF24_1MBPS;

    // RF-Power Level (verwende RF24-Library Enums direkt)
    constexpr rf24_pa_dbm_e POWER_LEVEL = RF24_PA_MAX;

    // Pipe-Adressen (5 Bytes) - MUSS IDENTISCH MIT SENDER SEIN!
    const uint8_t PIPE_ADDRESS[5] PROGMEM = {'B', 'A', 'M', 'P', 'L'};  // "BAMPL" = Bogenampel

    // Auto-ACK aktiviert (Empfänger sendet automatisch ACK)
    constexpr bool AUTO_ACK_ENABLED = true;

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

} // namespace Timing

//=============================================================================
// SYSTEMKONSTANTEN
//=============================================================================

namespace System {

    // Versionsinformation (im Flash gespeichert)
    const char VERSION[] PROGMEM = "Bogenampel Empfaenger V1.0";
    const char BUILD_DATE[] PROGMEM = __DATE__;
    const char BUILD_TIME[] PROGMEM = __TIME__;

    // Serial Baud Rate (für Debugging)
    constexpr uint32_t SERIAL_BAUD = 115200;

    // Debugging aktivieren/deaktivieren
    #define DEBUG_ENABLED 1  // 1 = Debug-Ausgaben an, 0 = aus

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
