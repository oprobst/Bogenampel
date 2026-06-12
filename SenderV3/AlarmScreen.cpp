/**
 * @file AlarmScreen.cpp
 * @brief Implementierung des Alarm-Screens (e-Paper)
 */

#include "AlarmScreen.h"

AlarmScreen::AlarmScreen(EpaperDisplay& epdRef)
    : epd(epdRef) {
}

void AlarmScreen::draw(bool delivered) {
    Adafruit_GFX& g = epd.gfx();

    // Invertierter Vollbild-Hintergrund für maximale Auffälligkeit
    g.fillScreen(GxEPD_BLACK);
    g.setTextColor(GxEPD_WHITE);

    // Großer "ALARM"-Schriftzug
    epd.printCentered("ALARM", 50, 4);

    // Erklärungstext
    epd.printCentered("Schiessbetrieb", 100, 2);
    epd.printCentered("abgebrochen", 122, 2);

    // Zustellstatus (US3-Szenario 3: bei Fehlschlag muss der Bediener
    // wissen, dass er mündlich abbrechen muss)
    if (delivered) {
        epd.printCentered("Empfaenger: bestaetigt", 154, 1);
    } else {
        epd.printCentered("!! KEINE BESTAETIGUNG !!", 150, 1);
        epd.printCentered("Muendlich abbrechen!", 162, 1);
    }

    // Quittierungs-Hinweis
    epd.printCentered("OK: quittieren", EpaperDisplay::HEIGHT - 14, 1);

    g.setTextColor(GxEPD_BLACK);
}
