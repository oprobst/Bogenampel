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

    // Großer "ALARM" Text in Rot (zentriert, oben)
    display.setTextSize(4);
    display.setTextColor(ST77XX_RED);

    const char* alarmText = "ALARM";
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(alarmText, 0, 0, &x1, &y1, &w, &h);
    uint16_t alarmX = (display.width() - w) / 2;
    uint16_t alarmY = 60;

    display.setCursor(alarmX, alarmY);
    display.print(alarmText);

    // Erklärungstext (zentriert, Mitte)
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);

    const char* line1 = "Schiessbetrieb";
    const char* line2 = "abgebrochen";

    display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
    uint16_t line1X = (display.width() - w) / 2;
    display.setCursor(line1X, 130);
    display.println(line1);

    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    uint16_t line2X = (display.width() - w) / 2;
    display.setCursor(line2X, 150);
    display.println(line2);
}
