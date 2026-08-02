/**
 * @file ConfigMenu.cpp
 * @brief Implementierung des Konfigurationsmenüs (e-Paper)
 */

#include "ConfigMenu.h"

ConfigMenu::ConfigMenu(EpaperDisplay& epdRef, ButtonManager& btnMgr)
    : epd(epdRef)
    , buttons(btnMgr)
    , shootingTime(TournamentDefaults::DEFAULT_TIME)
    , shooterCount(TournamentDefaults::DEFAULT_COUNT)
    , cursorLine(0)
    , complete(false)
    , needsUpdate(true) {
}

void ConfigMenu::begin() {
    cursorLine = 0;
    complete = false;
    needsUpdate = true;
}

void ConfigMenu::setConfig(uint16_t time, uint8_t count) {
    shootingTime = time;
    shooterCount = count;
    needsUpdate = true;
}

void ConfigMenu::update() {
    // Nichts tun wenn bereits abgeschlossen
    if (complete) return;

    if (cursorLine == 0) {
        // Zeile 0: Zeit auswählen (120/240)
        if (buttons.wasClicked(Button::CONFIG)) {
            shootingTime = (shootingTime == 120) ? 240 : 120;
            needsUpdate = true;
        } else if (buttons.wasClicked(Button::OK)) {
            cursorLine = 1;
            needsUpdate = true;
        }
    } else if (cursorLine == 1) {
        // Zeile 1: Schützenanzahl auswählen (1-2/3-4)
        if (buttons.wasClicked(Button::CONFIG)) {
            shooterCount = (shooterCount == 2) ? 4 : 2;
            needsUpdate = true;
        } else if (buttons.wasClicked(Button::OK)) {
            cursorLine = 2;
            needsUpdate = true;
        }
    } else {
        // Zeile 2: "Start" — CONFIG = Wrap-around zurück zur Konfiguration
        if (buttons.wasClicked(Button::CONFIG)) {
            cursorLine = 0;
            needsUpdate = true;
        } else if (buttons.wasClicked(Button::OK)) {
            complete = true;
        }
    }
}

void ConfigMenu::draw() {
    Adafruit_GFX& g = epd.gfx();

    // Inhalt unterhalb der Statuszeile löschen
    g.fillRect(0, Display::STATUS_H, EpaperDisplay::WIDTH,
               EpaperDisplay::HEIGHT - Display::STATUS_H, GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    // Überschrift
    epd.printCentered("Konfiguration", 32, 2);
    g.drawFastHLine(10, 52, EpaperDisplay::WIDTH - 20, GxEPD_BLACK);

    // --- Zeile 0: Zeit ---
    const int16_t timeY = 64;
    g.setTextSize(2);
    g.setCursor(14, timeY);
    g.print("Zeit");
    if (cursorLine == 0) {
        g.setCursor(2, timeY);
        g.print('>');
    }
    // Optionen 120s / 240s, aktive Auswahl unterstrichen
    g.setCursor(86, timeY);
    g.print("120s");
    g.setCursor(146, timeY);
    g.print("240s");
    if (shootingTime == 120) {
        g.drawFastHLine(86, timeY + 16, 46, GxEPD_BLACK);
        g.drawFastHLine(86, timeY + 17, 46, GxEPD_BLACK);
    } else {
        g.drawFastHLine(146, timeY + 16, 46, GxEPD_BLACK);
        g.drawFastHLine(146, timeY + 17, 46, GxEPD_BLACK);
    }

    // --- Zeile 1: Schützen ---
    const int16_t shooterY = 98;
    g.setTextSize(2);
    g.setCursor(14, shooterY);
    g.print("Sch.");
    if (cursorLine == 1) {
        g.setCursor(2, shooterY);
        g.print('>');
    }
    g.setCursor(86, shooterY);
    g.print("1-2");
    g.setCursor(146, shooterY);
    g.print("3-4");
    if (shooterCount == 2) {
        g.drawFastHLine(86, shooterY + 16, 34, GxEPD_BLACK);
        g.drawFastHLine(86, shooterY + 17, 34, GxEPD_BLACK);
    } else {
        g.drawFastHLine(146, shooterY + 16, 34, GxEPD_BLACK);
        g.drawFastHLine(146, shooterY + 17, 34, GxEPD_BLACK);
    }

    // --- Zeile 2: Start-Button ---
    const int16_t btnY = 134;
    const int16_t btnH = 32;
    const int16_t margin = 30;
    const int16_t btnW = EpaperDisplay::WIDTH - 2 * margin;
    if (cursorLine == 2) {
        // Ausgewählt: invertiert (schwarzer Button, weiße Schrift)
        g.fillRect(margin, btnY, btnW, btnH, GxEPD_BLACK);
        g.setTextColor(GxEPD_WHITE);
    } else {
        g.drawRect(margin, btnY, btnW, btnH, GxEPD_BLACK);
        g.setTextColor(GxEPD_BLACK);
    }
    epd.printCentered("Start", btnY + 9, 2);
    g.setTextColor(GxEPD_BLACK);

    // Hilfetext unten
    g.setTextSize(1);
    g.setCursor(10, EpaperDisplay::HEIGHT - 24);
    g.print("Stift: aendern   OK: weiter");
    g.setCursor(10, EpaperDisplay::HEIGHT - 12);
    g.print("Taste 3s halten: Aus");

    needsUpdate = false;
}
