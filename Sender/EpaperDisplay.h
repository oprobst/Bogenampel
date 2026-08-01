/**
 * @file EpaperDisplay.h
 * @brief GxEPD2-Wrapper für das 1.54" e-Paper (SSD1681, 200x200)
 *
 * NEU in V3: ersetzt das ST7789-TFT aus V2. Das Panel ist ein rohes
 * Waveshare-1.54"-V2-Panel (GDEH0154D67) an J1 mit diskretem Boost —
 * elektrisch identisch zum Standard-Modul, Treiberklasse GxEPD2_154_D67 (R-1).
 *
 * Refresh-Strategie (R-2):
 * - refreshScreen() für Screen-/Zustandswechsel: Partial-Waveform über das
 *   volle Fenster (~0,5 s, blitzt NICHT); alle PARTIAL_REFRESH_LIMIT Refreshes
 *   automatisch einmal voll, um angesammeltes Ghosting zu löschen
 * - Partial-Refresh (~300-400 ms) für 1-Hz-Countdown und Statuszeile
 * - fullRefresh() (~2,6 s, blitzt) nur noch erzwungen: Splash, Abschalt-Screen
 * - LOAD-Rail (GPIO7) MUSS vor init() an sein (FR-018); vor Power-Off
 *   hibernate() und Rail aus (sonst Geisterbilder)
 */

#pragma once

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include "Config.h"

class EpaperDisplay {
public:
    EpaperDisplay();

    /**
     * @brief Schaltet die LOAD-Rail ein und initialisiert das Panel
     *
     * Reihenfolge laut FR-018: erst 3V3_LOAD (GPIO7 HIGH), dann SPI + init().
     */
    void begin();

    /**
     * @brief Zeichenfläche (Adafruit_GFX-API) — Menüs zeichnen hierauf
     *
     * Gezeichnet wird in den RAM-Puffer; sichtbar wird es erst mit
     * fullRefresh() bzw. partialUpdate().
     */
    Adafruit_GFX& gfx() { return display; }

    /**
     * @brief Standard-Ausgabe bei Screen-/Zustandswechsel (~0,5 s, kein Blitzen)
     *
     * Gibt den gesamten Puffer über die schnelle Partial-Waveform aus. Das Panel
     * invertiert dabei NICHT mehrfach schwarz/weiß — der Wechsel bleibt ruhig.
     *
     * Preis: Ghosting sammelt sich an. Deshalb zählt der Wrapper alle Partials
     * (auch die Fenster-Updates von Countdown/Statuszeile) mit und schaltet nach
     * Display::PARTIAL_REFRESH_LIMIT selbsttätig auf einen Voll-Refresh um. Der
     * fällt so immer auf einen Screenwechsel und nie mitten in den Countdown.
     */
    void refreshScreen();

    /**
     * @brief Erzwungener Voll-Refresh des gesamten Panels (~2,6 s, blitzt)
     *
     * Nur dort verwenden, wo das Blitzen akzeptabel und ein garantiert
     * schattenfreies Bild gefragt ist: Splash und Abschalt-Screen. Für alle
     * übrigen Screenwechsel refreshScreen() nehmen.
     */
    void fullRefresh();

    /**
     * @brief Partial-Refresh eines Fensters (~300-400 ms, 1-Hz-tauglich)
     * @param x,y,w,h Fenster in Panel-Koordinaten
     */
    void partialUpdate(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    /**
     * @brief Löscht den Zeichenpuffer (weiß)
     */
    void clearBuffer();

    /**
     * @brief Panel in Tiefschlaf versetzen (vor Power-Off, gegen Geisterbilder)
     */
    void hibernate();

    /**
     * @brief LOAD-Rail abschalten (NACH hibernate(), letzter Schritt vor LATCH=LOW)
     */
    void railOff();

    /**
     * @brief Hilfsfunktion: Text horizontal zentriert ausgeben
     * @param text Text
     * @param y Cursor-Y (Baseline gemäß aktueller Textgröße)
     * @param textSize GFX-Textgröße (1-6)
     */
    void printCentered(const char* text, int16_t y, uint8_t textSize);

    /**
     * @brief Lader-Anzeige-Zustand für die Statuszeile
     *
     * SUSPENDED = Eingangsspannung liegt an, es wird aber nicht geladen
     * (Temperatur-/Timer-Fehler oder Lader im Standby) — muss sichtbar bleiben
     * und darf nicht als COMPLETE durchgehen.
     */
    enum class ChargeIcon : uint8_t { NONE, CHARGING, COMPLETE, SUSPENDED, FAULT };

    /**
     * @brief Zeichnet die Statuszeile in den Puffer (FR-016/FR-017, T023)
     * @param batteryPercent Akku 0-100 %
     * @param usbConnected USB-Versorgung liegt an
     * @param charge Lader-Zustand (Symbol/Text)
     * @param lowBattery Warnung < 3,3 V
     * @param radioConnected Empfänger per Discovery gefunden
     *
     * Sichtbar machen: partialUpdate(STATUS-Fenster) bzw. nächster Voll-Refresh.
     */
    void drawStatusLine(uint8_t batteryPercent, bool usbConnected, ChargeIcon charge,
                        bool lowBattery, bool radioConnected);

    /**
     * @brief Statuszeile per Partial-Refresh anzeigen (nach drawStatusLine)
     */
    void showStatusLine() {
        partialUpdate(Display::STATUS_X, Display::STATUS_Y, Display::STATUS_W, Display::STATUS_H);
    }

    // Panel-Geometrie
    static constexpr uint16_t WIDTH = Display::WIDTH;
    static constexpr uint16_t HEIGHT = Display::HEIGHT;

private:
    // Volle Pufferung: 200x200/8 = 5 kB — unkritisch auf dem ESP32-S3
    GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display;
    bool railEnabled;

    // Ghosting-Buchhaltung für refreshScreen()
    uint8_t partialCount;   // Partials seit dem letzten Voll-Refresh
    bool forceFullNext;     // nächster Screenwechsel muss voll sein (nach begin/hibernate)
};
