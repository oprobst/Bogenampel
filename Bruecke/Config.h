/**
 * @file Config.h
 * @brief Zentrale Konfiguration der Bluetooth-Brücke (Bogenampel V3)
 *
 * Die Brücke bekommt die Signaltöne des Empfängers über eine UART-Leitung und
 * spielt dieselbe Tonfolge über eine Bluetooth-Box (A2DP-Source). Sie sendet
 * nichts zurück — der Empfänger läuft mit und ohne Brücke gleich.
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

    // Signalleitung vom Empfänger (D5/GPIO7, ca. 330 Ω in Reihe, gemeinsame Masse)
    constexpr uint8_t SIGNAL_RX = 4;

} // namespace Pins

//=============================================================================
// SIGNALLEITUNG VOM EMPFÄNGER (UART, nur Empfänger → Brücke)
//=============================================================================

// Namespace wortgleich in Empfaenger/Config.h und Bruecke/Config.h halten!
// Telegramm: START, Anzahl Töne, START ^ Anzahl. Die Prüfung verwirft Störungen
// auf der Leitung und den undefinierten Pegel, solange der Empfänger bootet.
namespace SignalLink {

    constexpr uint32_t BAUD = 9600;  // langsam: robust auf dem Draht, 3 Bytes ≈ 3 ms
    constexpr uint8_t FRAME_START = 0xA5;
    constexpr uint8_t MAX_COUNT = 8;  // Alarm, längste Tonfolge

} // namespace SignalLink

//=============================================================================
// FUNK-NACHBARSCHAFT (Rücksicht auf ESP-NOW im selben Gehäuse)
//=============================================================================

namespace Radio {

    // Kanal, auf dem Sender und Empfänger funken (espnow-protocol.md). Die Brücke
    // selbst nutzt kein ESP-NOW, stört mit ihrem Bluetooth aber die Antenne
    // nebenan.
    constexpr uint8_t CHANNEL = 1;

    // Bluetooth-Kanäle, die die Brücke meidet (AFH-Sperre): alle, die den
    // ESP-NOW-Kanal überlappen. WLAN-Kanal N liegt bei 2407 + 5·N MHz (±11 MHz),
    // Bluetooth-Kanal k bei 2402 + k MHz → Kanal 1 ergibt k = 0..21.
    // Das automatische AFH erkennt die seltenen ESP-NOW-Frames kaum, deshalb fest.
    constexpr uint16_t ESPNOW_CENTER_MHZ = 2407 + 5 * CHANNEL;
    constexpr uint8_t AFH_BLOCK_FIRST = 0;
    constexpr uint8_t AFH_BLOCK_LAST = ESPNOW_CENTER_MHZ + 11 - 2402;
    static_assert(ESPNOW_CENTER_MHZ - 11 <= 2402, "AFH_BLOCK_FIRST muss angepasst werden");
    static_assert(79 - (AFH_BLOCK_LAST - AFH_BLOCK_FIRST + 1) >= 20,
                  "Bluetooth verlangt mindestens 20 freie Kanaele");

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

//=============================================================================
// OTA (WLAN-Station + ArduinoOTA, nur im Wartungsmodus)
//=============================================================================

// wifi_credentials.h (gitignored) definiert WIFI_SSID_OVERRIDE + WIFI_PASS_OVERRIDE.
// Vorlage: wifi_credentials.h.example im Repo-Root.
#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#endif

namespace OTA {

    constexpr const char* HOSTNAME = "bogenampel-bruecke";

#ifdef WIFI_SSID_OVERRIDE
    constexpr const char* WIFI_SSID = WIFI_SSID_OVERRIDE;
    constexpr const char* WIFI_PASS = WIFI_PASS_OVERRIDE;
#else
    constexpr const char* WIFI_SSID = "";
    constexpr const char* WIFI_PASS = "";
#endif
    constexpr uint16_t WIFI_TIMEOUT_MS = 10000;  // bis zum erneuten Verbindungsversuch

} // namespace OTA

namespace System {
    constexpr const char* VERSION = "Bogenampel Bruecke 0.2";
}
