/**
 * @file Config.h
 * @brief Zentrale Konfiguration der Bluetooth-Brücke (Bogenampel V3)
 *
 * Die Brücke hört per ESP-NOW auf die Signal-Frames (FT_SIGNAL) des Empfängers
 * und spielt dieselbe Tonfolge über eine Bluetooth-Box (A2DP-Source). Sie sendet
 * selbst nichts — Sender und Empfänger wissen nichts von ihr.
 *
 * Hardware (Steckbrett-Stand 2026-11-03):
 * - ESP32-DevKitC V4 (ESP32-WROOM-32): nur der klassische ESP32 kann Classic
 *   Bluetooth/A2DP, S3 und C3 haben ausschließlich BLE
 * - LCD 16x2, HD44780, 5-V-Variante (U3 unbestückt), parallel im 4-Bit-Modus.
 *   R/W fest an GND → das LCD treibt den Bus nie, kein 5-V-Rückweg in den ESP32.
 *   3,3-V-Pegel vom ESP32 genügen (HD44780: VIH ≥ 2,2 V bei 5 V).
 * - KY-040-Drehgeber: + an 3V3 (NICHT 5 V — die Pull-ups auf dem Modul ziehen
 *   CLK/DT sonst auf 5 V), SW hat keinen Pull-up auf dem Modul → INPUT_PULLUP
 *
 * Pin-Auswahl: Strapping-Pins (0, 2, 5, 12, 15), Flash-Pins (6-11) und die
 * input-only-Pins (34-39) sind gemieden. GPIO16/17 bleiben frei (bei einem
 * WROVER-Modul hängt dort das PSRAM), 21/22 bleiben für I²C frei.
 * Akkuspannung später auf GPIO34-39 (ADC1) — ADC2 ist gesperrt, solange
 * WLAN/ESP-NOW läuft.
 *
 * @date 2026-11-03
 */

#pragma once

#include <Arduino.h>

//=============================================================================
// DEBUG-KONFIGURATION
//=============================================================================

#ifndef DEBUG_ENABLED
#define DEBUG_ENABLED 1  // Steckbrett: Ausgaben an
#endif

#if DEBUG_ENABLED
    #define DEBUG_PRINT(...)   Serial.print(__VA_ARGS__)
    #define DEBUG_PRINTLN(...) Serial.println(__VA_ARGS__)
    #define DEBUG_PRINTF(...)  Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...)
    #define DEBUG_PRINTLN(...)
    #define DEBUG_PRINTF(...)
#endif

//=============================================================================
// PINS
//=============================================================================

namespace Pins {

    // LCD 16x2 (HD44780, 4-Bit-Modus)
    constexpr uint8_t LCD_RS = 25;
    constexpr uint8_t LCD_E  = 26;
    constexpr uint8_t LCD_D4 = 27;
    constexpr uint8_t LCD_D5 = 14;
    constexpr uint8_t LCD_D6 = 32;
    constexpr uint8_t LCD_D7 = 33;

    // KY-040 Drehgeber
    constexpr uint8_t ENC_CLK = 18;  // Spur A (Modul-Pull-up an 3V3)
    constexpr uint8_t ENC_DT  = 19;  // Spur B (Modul-Pull-up an 3V3)
    constexpr uint8_t ENC_SW  = 23;  // Taster gegen GND, INPUT_PULLUP

} // namespace Pins

//=============================================================================
// FUNK (ESP-NOW)
//=============================================================================

namespace Radio {

    // Fester Kanal, identisch mit Sender und Empfänger (espnow-protocol.md)
    constexpr uint8_t CHANNEL = 1;

} // namespace Radio

//=============================================================================
// AUDIO (A2DP-Source, Tonerzeugung)
//=============================================================================

namespace Audio {

    // A2DP liefert immer 44,1 kHz Stereo, 16 Bit
    constexpr uint32_t SAMPLE_RATE = 44100;

    // Tonfrequenz. Der Piezo im Empfänger läuft auf seiner Resonanz (3250 Hz),
    // das wäre auf einer Box schrill — 1 kHz trägt draußen gut und nervt weniger.
    constexpr uint16_t TONE_HZ = 1000;

    // Tonfolge wie BuzzerManager im Empfänger: 500 ms Ton, 500 ms Pause
    constexpr uint16_t BEEP_MS  = 500;
    constexpr uint16_t PAUSE_MS = 500;

    // Ein-/Ausblenden gegen Knackser an den Tonflanken
    constexpr uint16_t RAMP_MS = 5;

    // Kurzer Vorhörton beim Drehen der Lautstärke
    constexpr uint16_t PREVIEW_MS = 150;

    // Lautstärke in Stufen 0..VOLUME_STEPS (0 = stumm), Kennlinie quadratisch
    constexpr uint8_t VOLUME_STEPS = 20;
    constexpr uint8_t VOLUME_DEFAULT = 14;
    constexpr int16_t AMPLITUDE_MAX = 30000;  // knapp unter Vollaussteuerung

    // Gerätename, unter dem die Brücke in der Box auftaucht
    constexpr const char* LOCAL_NAME = "Bogenampel";

} // namespace Audio

//=============================================================================
// BEDIENUNG
//=============================================================================

namespace Ui {

    // KY-040: eine Rastung = ein voller Gray-Zyklus (4 Flanken). Springt der
    // Wert pro Rastung um zwei, hat das Exemplar 2 Flanken je Rastung → 2.
    constexpr uint8_t ENC_STEPS_PER_DETENT = 4;

    constexpr uint16_t BUTTON_DEBOUNCE_MS = 30;
    constexpr uint16_t LONG_PRESS_MS = 1000;

    // Lautstärke erst nach dieser Ruhezeit in den NVS schreiben (Flash schonen)
    constexpr uint16_t SAVE_DELAY_MS = 2000;

    // Statusanzeige: „vor N s" höchstens so oft neu zeichnen
    constexpr uint16_t REFRESH_MS = 500;

    // Box-Suche: so viele Geräte merkt sich die Liste
    constexpr uint8_t SCAN_MAX_DEVICES = 8;

} // namespace Ui

//=============================================================================
// NVS
//=============================================================================

namespace Nvs {
    constexpr const char* NAMESPACE = "bruecke";
    constexpr const char* KEY_VOLUME = "volume";
    constexpr const char* KEY_SCAN = "scan";  // 1 = nächster Boot startet die Box-Suche
    constexpr const char* KEY_BOX_NAME = "boxname";  // nur für die Anzeige; verbunden
                                                     // wird über die Adresse, die die
                                                     // A2DP-Bibliothek selbst speichert
} // namespace Nvs

namespace System {
    constexpr const char* VERSION = "Bogenampel Bruecke 0.1";
}
