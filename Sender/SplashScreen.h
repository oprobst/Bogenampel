/**
 * @file SplashScreen.h
 * @brief Splash Screen für Startbildschirm
 *
 * Enthält nur die UI-Darstellung des Splash Screens.
 * Timing-Logik wird von der StateMachine verwaltet.
 */

#pragma once

#include <Adafruit_ST7789.h>
#include "Config.h"

class SplashScreen {
public:
    SplashScreen(Adafruit_ST7789& tft);

    /**
     * @brief Zeichnet den kompletten Splash Screen
     */
    void draw();

    /**
     * @brief Aktualisiert die Verbindungsstatus-Anzeige
     * @param status Statustext (z.B. "Suche Empfaenger...", "Verbunden", "Nicht verbunden")
     */
    void updateConnectionStatus(const char* status);

    /**
     * @brief Zeigt Verbindungsqualität an
     * @param qualityPercent Qualität in Prozent (0-100)
     */
    void showConnectionQuality(uint8_t qualityPercent);

private:
    Adafruit_ST7789& display;

    // Position für Status-Text (wird in draw() initialisiert)
    static constexpr uint16_t STATUS_Y = 180;
};
