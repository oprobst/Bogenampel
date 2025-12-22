/**
 * @file AlarmScreen.h
 * @brief Alarm-Screen für Notfall-Abbruch
 */

#pragma once

#include <Adafruit_ST7789.h>
#include "Config.h"
#include "ButtonManager.h"

/**
 * @brief Alarm-Screen für Notfall-Abbruch des Schießbetriebs
 *
 * Zeigt großen "ALARM" Text.
 * Läuft automatisch ~4.5 Sekunden und endet dann.
 */
class AlarmScreen {
public:
    /**
     * @brief Konstruktor
     * @param tft Display-Referenz
     * @param btnMgr ButtonManager-Referenz
     */
    AlarmScreen(Adafruit_ST7789& tft, ButtonManager& btnMgr);

    /**
     * @brief Initialisiert den Screen
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     */
    void update();

    /**
     * @brief Zeichnet den Alarm-Screen
     */
    void draw();

private:
    Adafruit_ST7789& display;
    ButtonManager& buttons;
};
