/**
 * @file SplashScreen.cpp
 * @brief Splash Screen Implementierung (e-Paper)
 */

#include "SplashScreen.h"

SplashScreen::SplashScreen(EpaperDisplay& epdRef)
    : epd(epdRef) {
}

void SplashScreen::draw() {
    Adafruit_GFX& g = epd.gfx();

    g.fillScreen(GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    // Logo-Text "BOGENAMPEL" mit Rahmen
    g.drawRect(10, 36, EpaperDisplay::WIDTH - 20, 44, GxEPD_BLACK);
    g.drawRect(11, 37, EpaperDisplay::WIDTH - 22, 42, GxEPD_BLACK);
    epd.printCentered("BOGENAMPEL", 51, 2);

    // Versions-Text
    epd.printCentered(System::VERSION, 92, 1);

    // Initialer Status — nur in den Puffer. Ein Partial-Refresh an dieser
    // Stelle würde von GxEPD2 zu einem Voll-Refresh aufgewertet (Panel frisch
    // initialisiert) und der Splash blitzte zweimal.
    drawConnectionStatus("Suche Empfaenger...");

    // Funk-Info (unten, klein)
    g.setTextSize(1);
    g.setCursor(10, EpaperDisplay::HEIGHT - 24);
    g.print("Funk: ESP-NOW Kanal ");
    g.print(Radio::CHANNEL);

    // Hinweis zum Überspringen
    g.setCursor(10, EpaperDisplay::HEIGHT - 12);
    g.print("OK: ueberspringen");
}

void SplashScreen::drawConnectionStatus(const char* status) {
    Adafruit_GFX& g = epd.gfx();

    // Status-Bereich löschen und Text zentriert zeichnen
    g.fillRect(0, RESULT_Y, EpaperDisplay::WIDTH, RESULT_H, GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);
    epd.printCentered(status, RESULT_Y + 8, 1);
}

void SplashScreen::updateConnectionStatus(const char* status) {
    drawConnectionStatus(status);
    epd.partialUpdate(0, RESULT_Y, EpaperDisplay::WIDTH, RESULT_H);
}

void SplashScreen::showConnectionQuality(uint8_t qualityPercent) {
    Adafruit_GFX& g = epd.gfx();

    // Ergebnis-Bereich löschen
    g.fillRect(0, RESULT_Y, EpaperDisplay::WIDTH, RESULT_H, GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    // Überschrift + Prozentwert
    epd.printCentered("Verbindung", RESULT_Y + 2, 1);

    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", qualityPercent);
    epd.printCentered(buf, RESULT_Y + 14, 2);

    // Balken-Anzeige
    const int16_t barWidth = 140;
    const int16_t barHeight = 10;
    const int16_t barX = (EpaperDisplay::WIDTH - barWidth) / 2;
    const int16_t barY = RESULT_Y + 36;
    g.drawRect(barX, barY, barWidth, barHeight, GxEPD_BLACK);
    int16_t fillWidth = (barWidth - 4) * qualityPercent / 100;
    if (fillWidth > 0) {
        g.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, GxEPD_BLACK);
    }

    // Bewertungstext (V2-Schwellen)
    const char* qualityText;
    if (qualityPercent >= 80) {
        qualityText = "Sehr gut";
    } else if (qualityPercent >= 50) {
        qualityText = "Mittel";
    } else if (qualityPercent > 0) {
        qualityText = "Schlecht";
    } else {
        qualityText = "Keine Verbindung";
    }
    epd.printCentered(qualityText, barY + barHeight + 4, 1);

    epd.partialUpdate(0, RESULT_Y, EpaperDisplay::WIDTH, RESULT_H);
}
