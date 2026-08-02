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
     *
     * NICHT aus draw() aufrufen! Dort ist das Panel frisch initialisiert und
     * GxEPD2 wertet jeden Partial-Refresh solange zu einem Voll-Refresh auf
     * (`_initial_refresh`) — der Splash blitzte dadurch zweimal. Für den
     * Erstaufbau gibt es drawConnectionStatus(), das nur in den Puffer malt.
     */
    void updateConnectionStatus(const char* status);

    /**
     * @brief Zeigt die Verbindungsqualität an (Partial-Refresh, FR-009)
     * @param qualityPercent Qualität in Prozent (0-100); 0 = "Keine Verbindung"
     */
    void showConnectionQuality(uint8_t qualityPercent);

private:
    EpaperDisplay& epd;

    /**
     * @brief Zeichnet den Statustext nur in den Puffer (ohne Refresh)
     */
    void drawConnectionStatus(const char* status);

    // Partial-Fenster für Status/Qualität.
    //
    // Untere Kante MUSS über der Funk-Info bei HEIGHT-24 (176) bleiben: Das
    // Fenster wird vor jedem Zeichnen weiß gefüllt, und reichte es bis 180,
    // rasierte es die obere Hälfte der Zeile "Funk: ESP-NOW Kanal 1" ab.
    static constexpr uint16_t RESULT_Y = 104;
    static constexpr uint16_t RESULT_H = 64;   // Fenster 104..168, Funk-Info ab 176

    static_assert(RESULT_Y + RESULT_H <= 176,
                  "Result window would clip the radio info line at HEIGHT-24");
};
