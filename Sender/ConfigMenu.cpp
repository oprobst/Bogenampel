/**
 * @file ConfigMenu.cpp
 * @brief Implementierung des Konfigurationsmenüs
 */

#include "ConfigMenu.h"

ConfigMenu::ConfigMenu(Adafruit_ST7789& tft, ButtonManager& btnMgr)
    : display(tft)
    , buttons(btnMgr)
    , shootingTime(EEPROM_Config::DEFAULT_TIME)
    , shooterCount(EEPROM_Config::DEFAULT_COUNT)
    , cursorLine(0)
    , selectedButton(1)  // Default: "Start"
    , complete(false)
    , changeRequested(false)
    , needsUpdate(true)
    , firstDraw(true)
    , lastShootingTime(0)
    , lastShooterCount(0)
    , lastCursorLine(0xFF)
    , lastSelectedButton(0xFF) {
}

void ConfigMenu::begin() {
    // Setze UI-Variablen zurück
    cursorLine = 0;
    selectedButton = 1;  // Default: "Start"
    complete = false;
    changeRequested = false;
    needsUpdate = true;
    firstDraw = true;  // Beim nächsten draw() alles neu zeichnen

    // Vorherige Werte zurücksetzen
    lastShootingTime = 0;
    lastShooterCount = 0;
    lastCursorLine = 0xFF;
    lastSelectedButton = 0xFF;
}

void ConfigMenu::setConfig(uint8_t time, uint8_t count) {
    shootingTime = time;
    shooterCount = count;
    needsUpdate = true;
}

void ConfigMenu::update() {
    // Nichts tun wenn bereits abgeschlossen
    if (complete) return;

    // Button-Handling abhängig von cursorLine

    if (cursorLine == 0) {
        // Zeile 0: Zeit auswählen (120/240)
        if (buttons.wasPressed(Button::LEFT) || buttons.wasPressed(Button::RIGHT)) {
            // Toggle zwischen 120 und 240
            shootingTime = (shootingTime == 120) ? 240 : 120;
            needsUpdate = true;
        }
        else if (buttons.wasPressed(Button::OK)) {
            cursorLine = 1;
            needsUpdate = true;
            
        }
    }
    else if (cursorLine == 1) {
        // Zeile 1: Schützenanzahl auswählen (1-2/3-4)
        if (buttons.wasPressed(Button::LEFT) || buttons.wasPressed(Button::RIGHT)) {
            // Toggle zwischen 2 und 4
            shooterCount = (shooterCount == 2) ? 4 : 2;
            needsUpdate = true;           
        }
        else if (buttons.wasPressed(Button::OK)) {
            cursorLine = 2;
            needsUpdate = true;
        }
    }
    else if (cursorLine == 2) {
        // Zeile 2: Buttons ("Ändern" / "Start")
        if (buttons.wasPressed(Button::LEFT) || buttons.wasPressed(Button::RIGHT)) {
            // Toggle zwischen Ändern (0) und Start (1)
            selectedButton = (selectedButton == 0) ? 1 : 0;
            needsUpdate = true;
        }
        else if (buttons.wasPressed(Button::OK)) {
            if (selectedButton == 0) {
                // "Ändern" → zurück zu Zeile 0
                cursorLine = 0;
                changeRequested = true;
                needsUpdate = true;
                
            }
            else {
                // "Start" → Menü abschließen
                complete = true;               
            }
        }
    }
}

void ConfigMenu::draw() {
    // Beim ersten Aufruf: Komplettes Display zeichnen
    if (firstDraw) {
        display.fillScreen(ST77XX_BLACK);
        drawHeader();
        drawTimeOption();
        drawShooterOption();
        drawButtonOption();
        drawHelp();

        // Werte speichern
        lastShootingTime = shootingTime;
        lastShooterCount = shooterCount;
        lastCursorLine = cursorLine;
        lastSelectedButton = selectedButton;
        firstDraw = false;

    }
    else {
        // Selective Redraw: Nur geänderte Bereiche neu zeichnen

        // Zeit-Zeile neu zeichnen wenn Zeit oder Cursor geändert
        if (shootingTime != lastShootingTime ||
            (cursorLine == 0) != (lastCursorLine == 0)) {
            drawTimeOption();
            lastShootingTime = shootingTime;
        }

        // Schützen-Zeile neu zeichnen wenn Schützenanzahl oder Cursor geändert
        if (shooterCount != lastShooterCount ||
            (cursorLine == 1) != (lastCursorLine == 1)) {
            drawShooterOption();
            lastShooterCount = shooterCount;
        }

        // Button-Zeile neu zeichnen wenn Auswahl oder Cursor geändert
        if (selectedButton != lastSelectedButton ||
            (cursorLine == 2) != (lastCursorLine == 2)) {
            drawButtonOption();
            lastSelectedButton = selectedButton;
        }

        lastCursorLine = cursorLine;
    }

    needsUpdate = false;
}

//=============================================================================
// Private Hilfsfunktionen für Selective Drawing
//=============================================================================

void ConfigMenu::drawHeader() {
    // Überschrift: "Konfiguration"
    display.setTextSize(2);
    display.setTextColor(ST77XX_CYAN);

    // Zentrieren
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(F("Konfiguration"), 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 15);
    display.print(F("Konfiguration"));

    // Trennlinie
    display.drawFastHLine(10, 50, display.width() - 20, Display::COLOR_GRAY);
}

void ConfigMenu::drawTimeOption() {
    // Optionen-Positionen (Portrait: 240 Breite)
    const uint16_t option1_x = 120;
    const uint16_t option2_x = 180;
    const uint16_t y = 65;

    // Bereich löschen (Zeit-Zeile + Beschriftung)
    display.fillRect(0, y, display.width(), 40, ST77XX_BLACK);

    // Label "Zeit:"
    display.setTextSize(2);
    display.setCursor(10, y);
    display.setTextColor(cursorLine == 0 ? ST77XX_YELLOW : ST77XX_WHITE);
    display.print(F("Zeit:"));

    int16_t x1, y1;
    uint16_t w, h;

    // Option 120s
    display.setCursor(option1_x, y);
    display.setTextColor(cursorLine == 0 ? ST77XX_YELLOW : ST77XX_WHITE);
    display.print(F("120s"));
    if (shootingTime == 120) {
        display.getTextBounds(F("120s"), option1_x, y, &x1, &y1, &w, &h);
        display.drawLine(option1_x, y + h + 2, option1_x + w, y + h + 2,
                        cursorLine == 0 ? ST77XX_YELLOW : ST77XX_WHITE);
    }

    // Option 240s
    display.setCursor(option2_x, y);
    display.setTextColor(cursorLine == 0 ? ST77XX_YELLOW : ST77XX_WHITE);
    display.print(F("240s"));
    if (shootingTime == 240) {
        display.getTextBounds(F("240s"), option2_x, y, &x1, &y1, &w, &h);
        display.drawLine(option2_x, y + h + 2, option2_x + w, y + h + 2,
                        cursorLine == 0 ? ST77XX_YELLOW : ST77XX_WHITE);
    }

    // Beschriftung "pro Passe"
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, y + 25);
    display.print(F("pro Passe"));
}

void ConfigMenu::drawShooterOption() {
    // Optionen-Positionen (Portrait: 240 Breite)
    const uint16_t y = 115;

    // Bereich löschen (Schützen-Zeile + Beschriftung)
    display.fillRect(0, y, display.width(), 50, ST77XX_BLACK);

    // Label "Schuetzen:"
    display.setTextSize(2);
    display.setCursor(10, y);
    display.setTextColor(cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    display.print(F("Schuetzen:"));

    // Beschriftung "pro Scheibe" (direkt unter "Schuetzen:")
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, y + 18);
    display.print(F("pro Scheibe"));

    // Optionen auf zweiter Zeile
    const uint16_t optionY = y + 30;
    const uint16_t option1_x = 60;
    const uint16_t option2_x = 140;

    display.setTextSize(2);
    display.setTextColor(cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);

    int16_t x1, y1;
    uint16_t w, h;

    // Option 1-2
    display.setCursor(option1_x, optionY);
    display.print(F("1-2"));
    if (shooterCount == 2) {
        display.getTextBounds(F("1-2"), option1_x, optionY, &x1, &y1, &w, &h);
        display.drawLine(option1_x, optionY + h + 2, option1_x + w, optionY + h + 2,
                        cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    }

    // Option 3-4
    display.setCursor(option2_x, optionY);
    display.print(F("3-4"));
    if (shooterCount == 4) {
        display.getTextBounds(F("3-4"), option2_x, optionY, &x1, &y1, &w, &h);
        display.drawLine(option2_x, optionY + h + 2, option2_x + w, optionY + h + 2,
                        cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    }
}

void ConfigMenu::drawButtonOption() {
    // Portrait: Buttons übereinander
    const uint16_t y = 180;
    const uint16_t buttonHeight = 35;
    const uint16_t buttonSpacing = 10;
    const uint16_t margin = 20;
    const uint16_t buttonWidth = display.width() - 2 * margin;

    // Bereich löschen (beide Buttons inkl. Schatten)
    display.fillRect(0, y, display.width(), 2 * buttonHeight + buttonSpacing + 5, ST77XX_BLACK);

    // Farben für aktive Zeile
    uint16_t activeColor = cursorLine == 2 ? ST77XX_YELLOW : ST77XX_WHITE;
    uint16_t fillColor = Display::COLOR_DARKGRAY;

    int16_t x1, y1;
    uint16_t w, h;

    // --- Button 1: "Aendern" (oben) ---
    uint16_t btn1_y = y;

    if (selectedButton == 0) {
        display.fillRect(margin, btn1_y, buttonWidth, buttonHeight, fillColor);
    }
    display.drawRect(margin, btn1_y, buttonWidth, buttonHeight, activeColor);

    display.setTextSize(2);
    display.getTextBounds(F("Aendern"), 0, 0, &x1, &y1, &w, &h);
    uint16_t text_x = margin + (buttonWidth - w) / 2;
    uint16_t text_y = btn1_y + (buttonHeight - h) / 2;

    display.setCursor(text_x, text_y);
    display.setTextColor(activeColor);
    display.print(F("Aendern"));

    if (selectedButton == 0) {
        display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, activeColor);
    }

    // --- Button 2: "Start" (unten) ---
    uint16_t btn2_y = y + buttonHeight + buttonSpacing;

    if (selectedButton == 1) {
        display.fillRect(margin, btn2_y, buttonWidth, buttonHeight, fillColor);
    }
    display.drawRect(margin, btn2_y, buttonWidth, buttonHeight, activeColor);

    display.getTextBounds(F("Start"), 0, 0, &x1, &y1, &w, &h);
    text_x = margin + (buttonWidth - w) / 2;
    text_y = btn2_y + (buttonHeight - h) / 2;

    display.setCursor(text_x, text_y);
    display.setTextColor(activeColor);
    display.print(F("Start"));

    if (selectedButton == 1) {
        display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, activeColor);
    }
}

void ConfigMenu::drawHelp() {
    // Hilfetext unten (Portrait: mehr Platz)
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 30);
    display.print(F("L/R: Aendern, OK: Weiter"));

    // Alarm-Hinweis (zweite Zeile)
    display.setCursor(10, display.height() - 15);
    display.print(F("Pfeiltaste >2s: Alarm"));
}
