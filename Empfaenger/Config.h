/**
 * @file Config.h
 * @brief Zentrale Konfigurationsdatei für Bogenampel V3 Empfänger (Anzeigeeinheit)
 *
 * Enthält alle Hardware-Pin-Definitionen, Timing-Konstanten und
 * Konfigurationsparameter für den Empfänger.
 *
 * Hardware: Seeed XIAO ESP32C3 (U2, Zusatzplatine/Lochraster)
 * - WS2811 LED Strip (12V, IC-gesteuert, RGB; 7-Segment + Gruppen) — Versorgung extern
 * - ESP-NOW Funk (integriert, kein externes Modul)
 * - Piezo-Transducer 12V über BC337, Lüfter über 2N7002 (Low-Side-PWM)
 * - 3 Potis (Lautstärke, Helligkeit, Lüfter-Drehzahl), Debug-Taster, Status-LED
 *
 * Verbindliche Pin-Quelle: specs/004-v3-esp32-port/contracts/hardware-pins.md
 * (extrahiert aus Schaltung-Empfaenger/Empfaenger.kicad_sch)
 *
 * ESP32-C3-Strapping-Hinweise (alle im Schaltplan gelöst, Stand 2026-06-10):
 * - GPIO2/D0 (Lautstärke-Poti): Fußpunkt an D4 (POTI_GND) statt GND
 * - GPIO9/D9 (Status-LED): aktiv LOW (3V3 → LED → R6 → Pin)
 * - GPIO8/D8: frei (Lüfter-Tacho bewusst nicht angeschlossen)
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
#define DEBUG_ENABLED 0  // 1 = Debug-Ausgaben an, 0 = aus

// Verkürzte Zeiten für Tests (nur wenn DEBUG_ENABLED = 1)
#define DEBUG_SHORT_TIMES 0  // 1 = Verkürzte Zeiten, 0 = Normale Zeiten

//=============================================================================
// HARDWARE PIN-DEFINITIONEN (XIAO-Nummerierung → GPIO)
//=============================================================================

namespace Pins {

    //-------------------------------------------------------------------------
    // Analoge Eingänge: Potis (alle ADC1 — ADC2 ist mit aktivem Funk unbrauchbar!)
    //-------------------------------------------------------------------------
    constexpr uint8_t VOLUME_POTI     = 2;  // D0/GPIO2 (ADC1_CH2): Lautstärke (J2),
                                            // Schleifer über R1 1k; Fußpunkt an POTI_GND!
    constexpr uint8_t BRIGHTNESS_POTI = 3;  // D1/GPIO3 (ADC1_CH3): Helligkeit (J3),
                                            // über R2 1k (umverdrahtet von D5 — GPIO7 hat keinen ADC)
    constexpr uint8_t FAN_POTI        = 4;  // D2/GPIO4 (ADC1_CH4): Lüfter-Drehzahl (J8),
                                            // Schleifer über R7 1k

    //-------------------------------------------------------------------------
    // Geschalteter Poti-Fußpunkt (Strapping-Fix GPIO2, Befund 2)
    //-------------------------------------------------------------------------
    constexpr uint8_t POTI_GND = 6;  // D4/GPIO6: Fußpunkt des Lautstärke-Potis (J2 Pin 3);
                                     // beim Reset hochohmig → GPIO2 liegt HIGH (Boot ok);
                                     // in setup() FRÜH auf OUTPUT + LOW setzen!

    //-------------------------------------------------------------------------
    // Ausgänge: Signalgeber und Lüfter
    //-------------------------------------------------------------------------
    constexpr uint8_t BUZZER  = 5;   // D3/GPIO5 (LEDC): Piezo 12V-Transducer (J6) über
                                     // R3 2k2 → BC337 (Q1), R4 10k Basis-Pulldown;
                                     // R8 470Ω parallel zum Piezo (Entlade-Pfad am Collector —
                                     // ohne den schwingt der Piezo nicht, nur Brummen; 470Ω
                                     // für max. Lautstärke, ~25 mA → 0,5-W-Typ)
                                     // (GPIO5 ist ADC2, wird aber rein digital genutzt)
    constexpr uint8_t FAN_PWM = 21;  // D6/GPIO21 (LEDC): Gate des 2N7002 (Q2, J9);
                                     // Q2 invertiert (Open-Drain auf der PWM-Leitung):
                                     // Gate HIGH = Leitung LOW = langsam! R5 10k =
                                     // Pull-up an 3V3 → PWM-Leitung beim Boot LOW,
                                     // Lüfter auf Minimaldrehzahl, bis die Firmware
                                     // übernimmt (früh Duty setzen!)

    //-------------------------------------------------------------------------
    // Ausgänge: LEDs
    //-------------------------------------------------------------------------
    constexpr uint8_t LED_STRIP  = 10;  // D10/GPIO10: WS2811 Data (J7), 158 Pixel
    constexpr uint8_t STATUS_LED = 9;   // D9/GPIO9: Status-LED — AKTIV LOW!
                                        // (3V3 → LED → R6 220Ω → Pin sinkt Strom;
                                        // Strapping-Fix Befund 3, Firmware invertiert)

    //-------------------------------------------------------------------------
    // Eingänge: Taster
    //-------------------------------------------------------------------------
    constexpr uint8_t BTN_DEBUG = 20;  // D7/GPIO20 (J5): Debug-/Testtaster gegen GND,
                                       // INPUT_PULLUP, aktiv LOW

    // Frei/Reserve: D5/GPIO7, D8/GPIO8 (Tacho bewusst nicht angeschlossen)

} // namespace Pins

// Status-LED-Polarität (aktiv LOW, siehe oben)
constexpr uint8_t STATUS_LED_ON  = LOW;
constexpr uint8_t STATUS_LED_OFF = HIGH;

//=============================================================================
// OTA (WiFi-Station + ArduinoOTA, nur im OTA-Wartungsmodus)
//=============================================================================

// wifi_credentials.h (gitignored) definiert WIFI_SSID_OVERRIDE + WIFI_PASS_OVERRIDE.
// Vorlage: wifi_credentials.h.example im Repo-Root.
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#endif

namespace OTA {

    constexpr const char* HOSTNAME = "bogenampel-empfaenger";
    // OTA-Passwort (WPA2, min. 8 Zeichen) — ArduinoOTA-Auth (FR-012)
    constexpr const char* PASSWORD = "bogenampel";

    // Heimnetz-WLAN für den OTA-Wartungsmodus — aus wifi_credentials.h. Ohne
    // Zugangsdaten bleibt das Gerät im Wartungsmodus im CONNECTING-Blinken
    // (kein SoftAP-Fallback mehr, FR-009). ESP-NOW läuft im Wartungsmodus nicht.
#ifdef WIFI_SSID_OVERRIDE
    constexpr const char* WIFI_SSID = WIFI_SSID_OVERRIDE;
    constexpr const char* WIFI_PASS = WIFI_PASS_OVERRIDE;
#else
    constexpr const char* WIFI_SSID = "";
    constexpr const char* WIFI_PASS = "";
#endif
    constexpr uint16_t WIFI_TIMEOUT = 10000;  // ms bis zum erneuten WiFi-Connect-Versuch (OTA-Wartungsmodus)

} // namespace OTA

//=============================================================================
// OTA-WARTUNGSMODUS — Status-LED-Signalmuster (D9, aktiv LOW)
// Drei unterscheidbare Muster, alle Zeiten sind Zielwerte (±~50 ms Toleranz):
//   CONNECTING (FR-004a): schnelles kurzes Blitzen      (100 ms an / 500 ms aus)
//   READY      (FR-004b): langsames gleichmäßiges Blinken (500 ms an / 500 ms aus)
//   UPDATING   (FR-004c): leuchtet, alle 500 ms kurz aus  (500 ms an / 100 ms aus)
//=============================================================================

namespace OtaSignal {

    constexpr uint16_t CONNECTING_PERIOD_MS = 600;   // FR-004a
    constexpr uint16_t CONNECTING_ON_MS     = 100;
    constexpr uint16_t READY_PERIOD_MS      = 1000;  // FR-004b
    constexpr uint16_t READY_ON_MS          = 500;
    constexpr uint16_t UPDATING_PERIOD_MS   = 600;   // FR-004c
    constexpr uint16_t UPDATING_ON_MS       = 500;

} // namespace OtaSignal

//=============================================================================
// FUNK (ESP-NOW)
//=============================================================================

namespace Radio {

    // WLAN-Kanal (fest, beide Geräte identisch — espnow-protocol.md)
    constexpr uint8_t CHANNEL = 1;

} // namespace Radio

//=============================================================================
// TIMING-KONSTANTEN
//=============================================================================

namespace Timing {

    // Status-LED-Feedback
    constexpr uint16_t LED_BLINK_DURATION_MS = 100;  // Kurzes Blinken bei Empfang

    // Button Debouncing
    constexpr uint8_t DEBOUNCE_MS = 50;  // 50ms Entprellzeit

    // Buzzer-Feedback
    // Resonanzfrequenz des Transducers (Datenblatt CPS-4013-110PM, rated 3250 Hz):
    // ein Piezo ist GENAU bei seiner Resonanz am lautesten — daneben (vorher 2700 Hz)
    // kaum hörbar. Das ist der Lautstärke-Hebel.
    constexpr uint16_t BUZZER_FREQUENCY_HZ = 3250;

    // Poti-Abtastung (alle Regler live, ≥ 10 Hz — FR-021/FR-022/FR-023)
    constexpr uint16_t POTI_UPDATE_INTERVAL_MS = 100;  // 10 Hz

    // Regler-Vorschau: beim Drehen an Lautstärke-/Helligkeits-Poti läuft eine
    // Vorschau (Ton bzw. "888"-Anzeige), die noch bis zu dieser Zeit nach der
    // letzten Bewegung nachklingt.
    constexpr uint16_t PREVIEW_HOLD_MS = 1000;  // max. 1 s Nachlauf

} // namespace Timing

//=============================================================================
// ADC-HILFSFUNKTIONEN
//=============================================================================

namespace Adc {

    // Leichte softwareseitige Rauschunterdrückung: Mehrfachmessung mitteln
    // (Oversampling). Reduziert weißes ADC-Rauschen ~√N, ohne Latenz über die
    // Zeit (Reglerstellung bleibt sofort sichtbar). Default 6 Samples.
    inline uint16_t readAveraged(uint8_t pin, uint8_t samples = 6) {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < samples; i++) {
            sum += analogRead(pin);
        }
        return (uint16_t)(sum / samples);
    }

} // namespace Adc

//=============================================================================
// LAUTSTÄRKE-POTI (D0) — verpolt verbaut, in Software invertiert
//=============================================================================

namespace Volume {

    // ESP32-C3-ADC ist 12-bit (0…4095). Ab diesem Rohwert (nahe Maximum) ist der
    // Buzzer KOMPLETT STUMM; darunter quadratische Kennlinie bis volle Lautstärke
    // bei ADC = 0. (Poti verpolt → Invertierung in updatePotis().)
    // Schwelle bei 99 % der Range → nur im letzten 1 % der Reglerstellung stumm.
    constexpr uint16_t OFF_THRESHOLD = 4054;  // 4095 × 0,99 ≈ 4054 (letztes 1 %)

} // namespace Volume

//=============================================================================
// LED STRIP KONFIGURATION (WS2811 12V, IC-gesteuert, RGB)
//=============================================================================

namespace LEDStrip {

    // LED Strip Konfiguration (WS2811 12V — 1 Pixel pro schneidbarem Abschnitt):
    // Strip-Reihenfolge (vom HW-Aufbau bestätigt 2026-06-17):
    // - Gruppe C/D: 12 Pixel (LED 1-12,  Array Index 0-11)
    // - Gruppe A/B: 12 Pixel (LED 13-24, Array Index 12-23)
    // - 3 Digits × 7 Segmente × 2 Pixel = 42 Pixel (Array Index 24-65)
    // Total: 66 Pixel

    constexpr uint8_t GROUP_CD_LEDS = 12;      // LED 1-12  (Index 0-11)
    constexpr uint8_t GROUP_AB_LEDS = 12;      // LED 13-24 (Index 12-23)
    constexpr uint8_t GROUP_CD_START = 0;                            // C/D zuerst im Strip
    constexpr uint8_t GROUP_AB_START = GROUP_CD_START + GROUP_CD_LEDS;  // A/B direkt dahinter

    constexpr uint8_t LEDS_PER_SEGMENT = 2;    // 2 Pixel pro 7-Segment-Balken (12V-Streifen)
    constexpr uint8_t SEGMENTS_PER_DIGIT = 7;  // 7 Segmente pro Ziffer (B, A, F, G, C, D, E)
    constexpr uint8_t NUM_DIGITS = 3;          // 3 Ziffern (1er, 10er, 100er)
    constexpr uint8_t DIGIT_START = GROUP_AB_START + GROUP_AB_LEDS;  // Ziffern beginnen nach beiden Gruppen

    constexpr uint8_t LEDS_PER_DIGIT = LEDS_PER_SEGMENT * SEGMENTS_PER_DIGIT;  // 14 Pixel pro Ziffer
    constexpr uint8_t TOTAL_LEDS = GROUP_AB_LEDS + GROUP_CD_LEDS + (NUM_DIGITS * LEDS_PER_DIGIT);

    // Start-Indizes für die einzelnen Ziffern (relativ zu DIGIT_START)
    constexpr uint8_t DIGIT_1_START = DIGIT_START;                              // 1er-Stelle (links)
    constexpr uint8_t DIGIT_10_START = DIGIT_START + LEDS_PER_DIGIT;            // 10er-Stelle (mitte)
    constexpr uint8_t DIGIT_100_START = DIGIT_START + (2 * LEDS_PER_DIGIT);     // 100er-Stelle (rechts)

    // Helligkeitsbereich für Poti-Steuerung (FR-022)
    constexpr uint8_t BRIGHTNESS_MIN = 38;      // 15% Helligkeit (weiter herunterdimmbar)

    // ÜBERGANGS-DECKEL (USB-Versorgung, 12V-WS2811 über 5V→12V-Step-up):
    // Der Design-Wert ist 255 (100%). Solange der Streifen aber über USB versorgt
    // wird, überlastet volle Helligkeit die 5V-Schiene (der Step-up vervielfacht
    // den Strom) → ESP-Hang/Reset. Messung: 15% (brightness 38) lief stabil, 100%
    // (255) brach ein. Deshalb ist die Obergrenze hier hart gedeckelt.
    //   → Mit dem geplanten PD-12V-PCB (12V direkt, kein Step-up) wieder auf 255.
    //   → Wert ist der obere Poti-Anschlag; bei Instabilität weiter senken (Richtung
    //     BRIGHTNESS_MIN), bei besserer Versorgung anheben.
    constexpr uint8_t BRIGHTNESS_MAX = 64;      // ~25% — USB-Übergangsdeckel (Design: 255)

} // namespace LEDStrip

//=============================================================================
// LÜFTER (PWM über 2N7002, Drehzahl vom Poti — FR-023)
//=============================================================================

namespace Fan {

    constexpr uint32_t PWM_FREQUENCY_HZ = 25000;  // ≥ 25 kHz (4-Draht-Lüfter-Spec,
                                                  // außerhalb des Hörbereichs)
    constexpr uint8_t PWM_RESOLUTION_BITS = 8;    // Duty 0-255

    // Lüfter-Poti (D2) verpolt → in Software invertiert (ADC klein = volle Drehzahl).
    // Ab diesem Rohwert (nahe Maximum) Lüfter aus (bzw. Minimaldrehzahl bei 4-Draht-
    // Lüftern, die per PWM nicht vollständig stoppen).
    constexpr uint16_t OFF_THRESHOLD = 4000;

    // Kennlinie Poti → Drehzahl: Gamma < 1 (konkav) verschiebt die Drehzahl-
    // änderung nach "früher" im Drehweg und macht die GEFÜHLTE Änderung gleich-
    // mäßiger (Luftstrom/Lärm ~ Drehzahl^~2,5 → γ≈0,4 linearisiert die Wahrnehmung).
    // Kleiner = mehr Spreizung nach unten; 1,0 = linear. Tunbar.
    constexpr float CURVE_GAMMA = 0.4f;

} // namespace Fan

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

    // Versionsinformation
    constexpr const char* VERSION = "Bogenampel Empfaenger V3.0";
    constexpr const char* BUILD_DATE = __DATE__;
    constexpr const char* BUILD_TIME = __TIME__;

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
// KONFIGURATION VALIDIEREN (zur Compile-Zeit)
//=============================================================================

namespace ConfigValidation {

    // Alle Potis müssen auf ADC1 liegen (GPIO0-4 beim ESP32-C3; GPIO5 = ADC2!)
    static_assert(Pins::VOLUME_POTI <= 4, "VOLUME_POTI must be on ADC1 (radio blocks ADC2)");
    static_assert(Pins::BRIGHTNESS_POTI <= 4, "BRIGHTNESS_POTI must be on ADC1");
    static_assert(Pins::FAN_POTI <= 4, "FAN_POTI must be on ADC1");

    // LED-Gesamtzahl: 66 = 12 (C/D) + 12 (A/B) + 3×7×2 (Ziffern)
    static_assert(LEDStrip::TOTAL_LEDS == 66, "LED strip layout must total 66 pixels (12+12 Gruppen + 42 Ziffern)");

} // namespace ConfigValidation
