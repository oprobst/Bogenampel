/**
 * @file NoticeScreen.cpp
 * @brief Implementierung des Warnhinweis-Screens (e-Paper)
 */

#include "NoticeScreen.h"

NoticeScreen::NoticeScreen(EpaperDisplay& epdRef)
    : epd(epdRef) {
}

void NoticeScreen::drawStopFailed() {
    Adafruit_GFX& g = epd.gfx();

    // Invertierter Vollbild-Hintergrund, wie beim Alarm — maximale Auffälligkeit
    g.fillScreen(GxEPD_BLACK);
    g.setTextColor(GxEPD_WHITE);

    epd.printCentered("ABBRUCH", 44, 3);
    epd.printCentered("NICHT", 74, 3);
    epd.printCentered("BESTAETIGT", 104, 2);

    // Was der Bediener jetzt tun muss. Die Ampel zeigt weiter die laufende
    // Passe — sie beendet sie zwar autonom, aber eben erst nach Ablauf der
    // vollen Zeit, nicht sofort.
    epd.printCentered("Ampel laeuft weiter!", 136, 1);
    epd.printCentered("Muendlich abbrechen!", 150, 1);

    epd.printCentered("Taste: weiter", EpaperDisplay::HEIGHT - 14, 1);

    g.setTextColor(GxEPD_BLACK);
}
