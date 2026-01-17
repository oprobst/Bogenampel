/**
 * @file AlarmScreen.cpp
 * @brief Implementierung des Alarm-Screens
 */

#include "AlarmScreen.h"

AlarmScreen::AlarmScreen(Adafruit_ST7789& tft, ButtonManager& btnMgr)
    : display(tft)
    , buttons(btnMgr) {
}

void AlarmScreen::begin() {
    // Nichts zu initialisieren
}

void AlarmScreen::update() {
    // Keine vorzeitige Beendigung möglich
}

void AlarmScreen::draw() {
    // Schwarzer Hintergrund
    display.fillScreen(ST77XX_BLACK);

    // Portrait: Mehr vertikaler Platz, zentriert
    int16_t x1, y1;
    uint16_t w, h;

    // Großer "ALARM" Text in Rot (zentriert)
    display.setTextSize(4);
    display.setTextColor(ST77XX_RED);

    const char* alarmText = "ALARM";
    display.getTextBounds(alarmText, 0, 0, &x1, &y1, &w, &h);
    uint16_t alarmX = (display.width() - w) / 2;
    uint16_t alarmY = 100;  // Portrait: mehr Platz oben

    display.setCursor(alarmX, alarmY);
    display.print(alarmText);

    // Erklärungstext (zentriert)
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);

    const char* line1 = "Schiessbetrieb";
    const char* line2 = "abgebrochen";

    display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
    uint16_t line1X = (display.width() - w) / 2;
    display.setCursor(line1X, 180);  // Portrait: angepasst
    display.println(line1);

    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    uint16_t line2X = (display.width() - w) / 2;
    display.setCursor(line2X, 210);  // Portrait: angepasst
    display.println(line2);
}
