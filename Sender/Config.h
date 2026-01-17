/**
 * @file Config.h
 * @brief Zentrale Konfigurationsdatei für Bogenampel Sender
 *
 * Enthält alle Hardware-Pin-Definitionen, Timing-Konstanten und
 * Konfigurationsparameter für den Sender (Bedieneinheit).
 *
 * Hardware: Arduino Nano V3
 * - ST7789 TFT Display (240x320) über TXS0108EPW Level Shifter
 * - NRF24L01 Funkmodul
 * - 3x Taster, 3x Status-LEDs
 * - Batteriespannungsmessung (A7)
 *
 * @date 2025-12-13
 * @version 1.0
 */

#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>
#include <RF24.h>  // Für rf24_pa_dbm_e und rf24_datarate_e

//=============================================================================
// DEBUG-KONFIGURATION (muss VOR allen anderen Definitionen stehen!)
//=============================================================================

// Debugging aktivieren/deaktivieren
#define DEBUG_ENABLED 1  // 1 = Debug-Ausgaben an, 0 = aus

// Verkürzte Zeiten für Tests (nur wenn DEBUG_ENABLED = 1)
#define DEBUG_SHORT_TIMES 0  // 1 = Verkürzte Zeiten, 0 = Normale Zeiten

//=============================================================================
// HARDWARE PIN-DEFINITIONEN
//=============================================================================

namespace Pins {

    //-------------------------------------------------------------------------
    // SPI-Bus (gemeinsam für Display und NRF24L01)
    //-------------------------------------------------------------------------
    constexpr uint8_t SPI_SCK  = 13;  // Hardware SPI Clock
    constexpr uint8_t SPI_MOSI = 11;  // Hardware SPI Master Out Slave In
    constexpr uint8_t SPI_MISO = 12;  // Hardware SPI Master In Slave Out

    //-------------------------------------------------------------------------
    // ST7789 TFT Display (über TXS0108EPW Level Shifter)
    //-------------------------------------------------------------------------
    constexpr uint8_t TFT_CS   = A2;  // Display Chip Select (UI_CS → DSPL_CS)
    constexpr uint8_t TFT_DC   = 10;  // Display Data/Command (UI_DC/RS → DSPL_DC/RS)
    constexpr uint8_t TFT_RST  = A3;  // Display Reset (UI_RES → DSPL_RES)
    // Hinweis: TFT_MOSI, TFT_SCK, TFT_MISO = SPI-Bus (siehe oben)
    // Backlight ist fest an 3.3V (kein PWM-Control)

    //-------------------------------------------------------------------------
    // NRF24L01 Funkmodul
    //-------------------------------------------------------------------------
    constexpr uint8_t NRF_CE   = 9;   // NRF24 Chip Enable (D9)
    constexpr uint8_t NRF_CSN  = 8;   // NRF24 Chip Select (D8)
    // Hinweis: NRF_MOSI, NRF_SCK, NRF_MISO = SPI-Bus (siehe oben)

    //-------------------------------------------------------------------------
    // Eingänge: Taster (alle mit internem Pull-Up, aktiv LOW)
    //-------------------------------------------------------------------------
    constexpr uint8_t BTN_LEFT   = 5;  // J1: Menü-Navigation links
    constexpr uint8_t BTN_OK     = 6;  // J2: Menü-Auswahl bestätigen
    constexpr uint8_t BTN_RIGHT  = 7;  // J3: Menü-Navigation rechts

    //-------------------------------------------------------------------------
    // Ausgänge: Status-LEDs
    //-------------------------------------------------------------------------
    constexpr uint8_t LED_RED = A0;  // D1: Rote LED (Debug/Status)

    //-------------------------------------------------------------------------
    // Ausgänge: Buzzer
    //-------------------------------------------------------------------------
    constexpr uint8_t BUZZER = 2;  // D4: KY-006 Passiver Buzzer für Tastenton

    //-------------------------------------------------------------------------
    // Analoge Eingänge
    //-------------------------------------------------------------------------
    constexpr uint8_t VOLTAGE_SENSE = A5;  // Batteriespannung (1:1 Spannungsteiler)

} // namespace Pins

//=============================================================================
// DISPLAY KONFIGURATION
//=============================================================================

namespace Display {

    // Display-Auflösung (ST7789)
    constexpr uint16_t WIDTH  = 240;
    constexpr uint16_t HEIGHT = 320;

    // Display-Orientierung (0, 1, 2, 3 = 0°, 90°, 180°, 270°)
    constexpr uint8_t ROTATION = 0;  // 0 = 0° (Portrait: 240x320)

    // Hinweis: Adafruit_ST7789 nutzt Standard-Farbdefinitionen:
    // ST77XX_BLACK, ST77XX_WHITE, ST77XX_RED, ST77XX_GREEN, ST77XX_BLUE, etc.
    // Hier nur Custom-Farben in RGB565:
    constexpr uint16_t COLOR_GRAY    = 0x8410;
    constexpr uint16_t COLOR_DARKGRAY = 0x4208;
    constexpr uint16_t COLOR_ORANGE  = 0xFD20;

    // Status-Bereich (obere rechte Ecke für Batterie/USB-Symbol)
    constexpr uint8_t STATUS_AREA_X = 200;
    constexpr uint8_t STATUS_AREA_Y = 5;
    constexpr uint8_t STATUS_AREA_WIDTH = 35;
    constexpr uint8_t STATUS_AREA_HEIGHT = 20;

} // namespace Display

//=============================================================================
// RF KOMMUNIKATION (NRF24L01)
//=============================================================================

namespace RF {

    // SPI-Frequenz für NRF24L01 (max 10 MHz)
    constexpr uint32_t SPI_FREQUENCY = 10000000UL;  // 10 MHz

    // RF-Kanal (0-125, 2.4 GHz + Kanal MHz)
    constexpr uint8_t CHANNEL = 76;  // 2.476 GHz

    // RF-Datenrate (verwende RF24-Library Enums direkt)
    // RF24_250KBPS = robuster bei schlechten Verbindungen/langen Kabeln!
    constexpr rf24_datarate_e DATA_RATE = RF24_250KBPS;

    // RF-Power Level (verwende RF24-Library Enums direkt)
    // RF24_PA_MAX = 0dBm (höchste Leistung, ~50m Reichweite)
    // WICHTIG: Benötigt externe 3.3V Versorgung (AMS1117) + 100µF Kondensator!
    constexpr rf24_pa_dbm_e POWER_LEVEL = RF24_PA_MAX;
    //RF24_PA_MIN;

    // Pipe-Adressen (5 Bytes)
    // Sender schreibt an Pipe 0, Empfänger liest von Pipe 0
    const uint8_t PIPE_ADDRESS[5] PROGMEM = {'B', '4', 'M', 'P', 'L'};  // "BAMPL" = Bogenampel

    // Auto-ACK Einstellungen
    constexpr bool AUTO_ACK_ENABLED = true;  // ACK aktivieren für Verbindungskontrolle

    // Retry-Einstellungen (für ACK-Retransmission)
    constexpr uint8_t RETRY_DELAY = 5;    // Delay: (delay + 1) * 250µs = 1.5ms
    constexpr uint8_t RETRY_COUNT = 15;   // Max 15 Retries

    // Payload-Größe
    constexpr uint8_t PAYLOAD_SIZE = 2;   // 2 Bytes (Command + Checksum)

    // Connection Quality Test
    constexpr uint8_t QUALITY_TEST_PINGS = 10;        // Anzahl Pings für Qualitätstest
    constexpr uint16_t QUALITY_TEST_DURATION_MS = 5000;  // 5 Sekunden für Test
    constexpr uint16_t QUALITY_TEST_INTERVAL_MS = 500;   // 500ms zwischen Pings (10 in 5s)

} // namespace RF

//=============================================================================
// BATTERIE-ÜBERWACHUNG
//=============================================================================

namespace Battery {

    // Spannungsgrenzen (in Millivolt)
    constexpr uint16_t VOLTAGE_MIN_MV = 6000;   // 6.0V = 0% (leer)
    constexpr uint16_t VOLTAGE_MAX_MV = 9600;   // 9.6V = 100% (voll)
    constexpr uint16_t VOLTAGE_LOW_MV = 6600;   // 6.6V = 20% (Low Battery Warnung)

    // Spannungsteiler-Verhältnis (1:1 = 10kΩ : 10kΩ)
    constexpr float DIVIDER_RATIO = 2.0f;  // Vbat = Vmeasured * 2.0

    // ADC-Referenzspannung (Arduino Nano: 5V)
    constexpr float ADC_VREF = 5.0f;
    constexpr uint16_t ADC_MAX = 1023;  // 10-bit ADC

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
    constexpr uint8_t DEBOUNCE_MS = 50;  // 50ms Entprellzeit

    // Buzzer Click-Ton
    constexpr uint16_t CLICK_FREQUENCY_HZ = 1600;  // 1,6 kHz für satten Klick
    constexpr uint8_t CLICK_DURATION_MS = 25;      // 25ms kurzer Klick

    // Schießbetrieb
    #if DEBUG_SHORT_TIMES
        constexpr uint16_t PREPARATION_TIME_MS = 5000;   // 5 Sekunden (DEBUG)
    #else
        constexpr uint16_t PREPARATION_TIME_MS = 10000;  // 10 Sekunden Vorbereitungsphase
    #endif

    // Alarm Detection
    constexpr uint16_t ALARM_THRESHOLD_MS = 2000;  // 2 Sekunden OK-Button halten für Alarm

    // Display-Aktualisierung
    constexpr uint16_t DISPLAY_UPDATE_MS = 100;  // 100ms (10 fps)

    // LED-Blink-Intervalle
    constexpr uint16_t LED_BLINK_FAST_MS = 250;   // Schnelles Blinken
    constexpr uint16_t LED_BLINK_SLOW_MS = 1000;  // Langsames Blinken

    // RF-Timeout
    constexpr uint16_t RF_TRANSMIT_TIMEOUT_MS = 500;  // Max 500ms für Übertragung (inkl. Retries)
    constexpr uint16_t ALARM_RETRY_DELAY_MS = 200;   // 200ms zwischen Alarm-Retries
    constexpr uint8_t ALARM_MAX_RETRIES = 3;         // 3 Versuche für Alarm-Kommando

} // namespace Timing

//=============================================================================
// HINWEIS: KOMMANDO-DEFINITIONEN
//=============================================================================
// Die RF-Kommando-Definitionen befinden sich in Commands.h
// (RadioCommand, RadioPacket, calculateChecksum, validateChecksum)

//=============================================================================
// SYSTEMKONSTANTEN
//=============================================================================

namespace System {

    // Versionsinformation (im Flash gespeichert)
    const char VERSION[] PROGMEM = "Bogenampeln V1.0";
    const char BUILD_DATE[] PROGMEM = __DATE__;
    const char BUILD_TIME[] PROGMEM = __TIME__;

    // Serial Baud Rate (für Debugging)
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
// GRUPPEN-DEFINITIONEN (für Anzeige auf Display)
//=============================================================================

namespace Groups {

    // Gruppen-Typen
    enum class Type : uint8_t {
        GROUP_AB = 0,  // Gruppe A/B
        GROUP_CD = 1   // Gruppe C/D
    };

    // Gruppen-Namen (im Flash)
    const char GROUP_AB[] PROGMEM = "A/B";
    const char GROUP_CD[] PROGMEM = "C/D";

    // Positions-Marker für 4-State Cycle (siehe Spec 002-shooter-groups)
    enum class Position : uint8_t {
        POS_1 = 1,  // Position 1
        POS_2 = 2   // Position 2
    };

} // namespace Groups

//=============================================================================
// EEPROM KONFIGURATION
//=============================================================================

namespace EEPROM_Config {

    // EEPROM-Adressen
    constexpr uint16_t CONFIG_ADDR = 0;  // TournamentConfig ab Adresse 0

    /**
     * @brief Turnier-Konfiguration (gespeichert im EEPROM)
     *
     * Diese Struktur wird im EEPROM gespeichert, um die Konfiguration
     * über Power-Cycles hinweg zu erhalten.
     */
    struct TournamentConfig {
        uint8_t shootingTime;   // 120 oder 240 (Sekunden)
        uint8_t shooterCount;   // 2 (1-2 Schützen) oder 4 (3-4 Schützen)
        uint8_t checksum;       // CRC8-Checksumme zur Validierung
    } __attribute__((packed));

    // Gültige Werte für shootingTime
    enum class ShootingTime : uint8_t {
        TIME_120_SEC = 120,
        TIME_240_SEC = 240
    };

    // Gültige Werte für shooterCount
    enum class ShooterCount : uint8_t {
        SHOOTERS_1_2 = 2,   // Anzeige: "1-2 Schützen"
        SHOOTERS_3_4 = 4    // Anzeige: "3-4 Schützen"
    };

    // Default-Werte
    constexpr uint8_t DEFAULT_TIME = static_cast<uint8_t>(ShootingTime::TIME_120_SEC);
    constexpr uint8_t DEFAULT_COUNT = static_cast<uint8_t>(ShooterCount::SHOOTERS_1_2);

} // namespace EEPROM_Config

//=============================================================================
// HELPER MAKROS
//=============================================================================

// Flash-String-Helper (PROGMEM)
#define F(string_literal) (reinterpret_cast<const __FlashStringHelper *>(PSTR(string_literal)))

// Sichere Pin-Modi-Definitionen
#define SAFE_PIN_MODE(pin, mode) do { pinMode(pin, mode); } while(0)
#define SAFE_DIGITAL_WRITE(pin, value) do { digitalWrite(pin, value); } while(0)
#define SAFE_DIGITAL_READ(pin) digitalRead(pin)

// Array-Größe ermitteln
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Min/Max (falls nicht von Arduino.h definiert)
#ifndef min
    #define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
    #define max(a, b) ((a) > (b) ? (a) : (b))
#endif

//=============================================================================
// ENDE DER KONFIGURATION
//=============================================================================

/**
 * @brief Konfiguration validieren (zur Compile-Zeit)
 *
 * Stellt sicher, dass keine Pin-Konflikte existieren und
 * alle Werte in gültigen Bereichen liegen.
 */
namespace ConfigValidation {

    // Prüfe, dass SPI-Pins korrekt sind (Hardware SPI)
    static_assert(Pins::SPI_SCK == 13, "SPI SCK must be D13 on Arduino Nano");
    static_assert(Pins::SPI_MOSI == 11, "SPI MOSI must be D11 on Arduino Nano");
    static_assert(Pins::SPI_MISO == 12, "SPI MISO must be D12 on Arduino Nano");

    // Prüfe, dass Chip Select Pins unterschiedlich sind
    static_assert(Pins::TFT_CS != Pins::NRF_CSN, "TFT_CS and NRF_CSN must be different");

    // Prüfe, dass Button-Pins unterschiedlich sind
    static_assert(Pins::BTN_LEFT != Pins::BTN_OK, "Button pins must be unique");
    static_assert(Pins::BTN_LEFT != Pins::BTN_RIGHT, "Button pins must be unique");
    static_assert(Pins::BTN_OK != Pins::BTN_RIGHT, "Button pins must be unique");

    // Prüfe Display-Auflösung
    static_assert(Display::WIDTH == 240, "ST7789 width is 240 pixels");
    static_assert(Display::HEIGHT == 320, "ST7789 height is 320 pixels");

    // Prüfe RF-Payload-Größe
    static_assert(RF::PAYLOAD_SIZE <= 32, "NRF24L01 max payload is 32 bytes");

} // namespace ConfigValidation
