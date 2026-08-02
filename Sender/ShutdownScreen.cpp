/**
 * @file ShutdownScreen.cpp
 * @brief Implementierung des Abschalt-Screens (e-Paper)
 */

#include "ShutdownScreen.h"

namespace {
    // Titel-Block identisch zum Splash — dasselbe Gerät, dieselbe Optik
    constexpr int16_t TITLE_FRAME_Y = 36;
    constexpr int16_t TITLE_FRAME_H = 44;
    constexpr int16_t TITLE_BASELINE = 51;

    constexpr int16_t REASON_Y = 100;
    constexpr int16_t HINT_LINE1_Y = 140;
    constexpr int16_t HINT_LINE2_Y = 158;

    // Fußzeile am unteren Rand — gleiche Höhe wie die Hinweiszeilen des Splash
    constexpr int16_t BATTERY_Y = EpaperDisplay::HEIGHT - 12;
}

ShutdownScreen::ShutdownScreen(EpaperDisplay& epdRef)
    : epd(epdRef) {
}

void ShutdownScreen::draw(const char* reason, int16_t batteryPercent) {
    Adafruit_GFX& g = epd.gfx();

    g.fillScreen(GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    // Doppelter Rahmen + Schriftzug wie auf dem Splash-Screen
    g.drawRect(10, TITLE_FRAME_Y, EpaperDisplay::WIDTH - 20, TITLE_FRAME_H, GxEPD_BLACK);
    g.drawRect(11, TITLE_FRAME_Y + 1, EpaperDisplay::WIDTH - 22, TITLE_FRAME_H - 2, GxEPD_BLACK);
    epd.printCentered("BOGENAMPEL", TITLE_BASELINE, 2);

    if (reason != nullptr && reason[0] != '\0') {
        epd.printCentered(reason, REASON_Y, 1);
    }

    // Einschalt-Hinweis: zweizeilig, damit er in der kleinen Schrift nicht
    // über die Panelbreite hinausläuft (200 px = 33 Zeichen bei Textgröße 1)
    epd.printCentered("Schwarze Taste", HINT_LINE1_Y, 1);
    epd.printCentered("zum Einschalten", HINT_LINE2_Y, 1);

    // Momentaufnahme des Akkustands. "beim Ausschalten" ist keine Floskel,
    // sondern der entscheidende Hinweis: Am Ladekabel bleibt dieses Bild
    // unverändert stehen, der Wert wird also mit jeder Lademinute falscher.
    if (batteryPercent >= 0) {
        char line[32];
        snprintf(line, sizeof(line), "Akku beim Ausschalten: %d%%", (int)batteryPercent);
        epd.printCentered(line, BATTERY_Y, 1);
    }
}
