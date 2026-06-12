/**
 * @file SplashScreen.h
 * @brief Splash Screen für Startbildschirm (e-Paper)
 *
 * PORT aus V2 (Sender/SplashScreen.h): Logo/Version als ein Voll-Refresh,
 * Status- und Qualitätsanzeige als Partial-Fenster (US5/T032).
 * Timing-Logik wird von der StateMachine verwaltet.
 */

#pragma once

#include "Config.h"
#include "EpaperDisplay.h"

class SplashScreen {
public:
    SplashScreen(EpaperDisplay& epd);

    /**
     * @brief Zeichnet den kompletten Splash Screen in den Puffer (ohne Refresh!)
     */
    void draw();

    /**
     * @brief Aktualisiert die Verbindungsstatus-Anzeige (Partial-Refresh)
     * @param status Statustext (z.B. "Suche Empfaenger...", "Teste Verbindung...")
     */
    void updateConnectionStatus(const char* status);

    /**
     * @brief Zeigt die Verbindungsqualität an (Partial-Refresh, FR-009)
     * @param qualityPercent Qualität in Prozent (0-100); 0 = "Keine Verbindung"
     */
    void showConnectionQuality(uint8_t qualityPercent);

private:
    EpaperDisplay& epd;

    // Partial-Fenster für Status/Qualität (unterer Bereich)
    static constexpr uint16_t RESULT_Y = 116;
    static constexpr uint16_t RESULT_H = 64;
};
