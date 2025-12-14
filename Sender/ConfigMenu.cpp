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
    DEBUG_PRINTLN(F("ConfigMenu: Initialisiere..."));

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

    DEBUG_PRINT(F("  Zeit: "));
    DEBUG_PRINT(shootingTime);
    DEBUG_PRINTLN(F("s"));
    DEBUG_PRINT(F("  Schuetzen: "));
    DEBUG_PRINTLN(shooterCount == 2 ? F("1-2") : F("3-4"));
}

void ConfigMenu::setConfig(uint8_t time, uint8_t count) {
    shootingTime = time;
    shooterCount = count;
    needsUpdate = true;

    DEBUG_PRINT(F("ConfigMenu: Setze Config - Zeit: "));
    DEBUG_PRINT(time);
    DEBUG_PRINT(F("s, Schuetzen: "));
    DEBUG_PRINTLN(count == 2 ? F("1-2") : F("3-4"));
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
            DEBUG_PRINT(F("ConfigMenu: Zeit -> "));
            DEBUG_PRINT(shootingTime);
            DEBUG_PRINTLN(F("s"));
        }
        else if (buttons.wasPressed(Button::OK)) {
            cursorLine = 1;
            needsUpdate = true;
            DEBUG_PRINTLN(F("ConfigMenu: Cursor -> Zeile 1 (Schuetzen)"));
        }
    }
    else if (cursorLine == 1) {
        // Zeile 1: Schützenanzahl auswählen (1-2/3-4)
        if (buttons.wasPressed(Button::LEFT) || buttons.wasPressed(Button::RIGHT)) {
            // Toggle zwischen 2 und 4
            shooterCount = (shooterCount == 2) ? 4 : 2;
            needsUpdate = true;
            DEBUG_PRINT(F("ConfigMenu: Schuetzen -> "));
            DEBUG_PRINTLN(shooterCount == 2 ? F("1-2") : F("3-4"));
        }
        else if (buttons.wasPressed(Button::OK)) {
            cursorLine = 2;
            needsUpdate = true;
            DEBUG_PRINTLN(F("ConfigMenu: Cursor -> Zeile 2 (Buttons)"));
        }
    }
    else if (cursorLine == 2) {
        // Zeile 2: Buttons ("Ändern" / "Start")
        if (buttons.wasPressed(Button::LEFT) || buttons.wasPressed(Button::RIGHT)) {
            // Toggle zwischen Ändern (0) und Start (1)
            selectedButton = (selectedButton == 0) ? 1 : 0;
            needsUpdate = true;
            DEBUG_PRINT(F("ConfigMenu: Button -> "));
            DEBUG_PRINTLN(selectedButton == 0 ? F("Aendern") : F("Start"));
        }
        else if (buttons.wasPressed(Button::OK)) {
            if (selectedButton == 0) {
                // "Ändern" → zurück zu Zeile 0
                cursorLine = 0;
                changeRequested = true;
                needsUpdate = true;
                DEBUG_PRINTLN(F("ConfigMenu: Aendern -> zurueck zu Zeile 0"));
            }
            else {
                // "Start" → Menü abschließen
                complete = true;
                DEBUG_PRINTLN(F("ConfigMenu: Start bestaetigt"));
                DEBUG_PRINT(F("  Finale Config - Zeit: "));
                DEBUG_PRINT(shootingTime);
                DEBUG_PRINT(F("s, Schuetzen: "));
                DEBUG_PRINTLN(shooterCount == 2 ? F("1-2") : F("3-4"));
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

        DEBUG_PRINTLN(F("ConfigMenu: Initial draw"));
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

        DEBUG_PRINTLN(F("ConfigMenu: Selective redraw"));
    }

    needsUpdate = false;
}

//=============================================================================
// Private Hilfsfunktionen für Selective Drawing
//=============================================================================

void ConfigMenu::drawHeader() {
    // Überschrift: "Konfiguration"
    display.setTextSize(3);
    display.setTextColor(ST77XX_CYAN);
    display.setCursor(10, 10);
    display.print(F("Konfiguration"));

    // Trennlinie
    display.drawFastHLine(10, 45, display.width() - 20, Display::COLOR_GRAY);
}

void ConfigMenu::drawTimeOption() {
    // Optionen-Positionen
    const uint16_t option1_x = 140;
    const uint16_t option2_x = 210;
    const uint16_t y = 60;

    // Bereich löschen (Zeit-Zeile + Beschriftung)
    display.fillRect(0, y, display.width(), 35, ST77XX_BLACK);

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
    display.setCursor(10, 85);
    display.print(F("pro Passe"));
}

void ConfigMenu::drawShooterOption() {
    // Optionen-Positionen
    const uint16_t option1_x = 140;
    const uint16_t option2_x = 210;
    const uint16_t y = 105;

    // Bereich löschen (Schützen-Zeile + Beschriftung)
    display.fillRect(0, y, display.width(), 35, ST77XX_BLACK);

    // Label "Schuetzen:"
    display.setTextSize(2);
    display.setCursor(10, y);
    display.setTextColor(cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    display.print(F("Schuetzen:"));

    int16_t x1, y1;
    uint16_t w, h;

    // Option 1-2
    display.setCursor(option1_x, y);
    display.setTextColor(cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    display.print(F("1-2"));
    if (shooterCount == 2) {
        display.getTextBounds(F("1-2"), option1_x, y, &x1, &y1, &w, &h);
        display.drawLine(option1_x, y + h + 2, option1_x + w, y + h + 2,
                        cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    }

    // Option 3-4
    display.setCursor(option2_x, y);
    display.setTextColor(cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    display.print(F("3-4"));
    if (shooterCount == 4) {
        display.getTextBounds(F("3-4"), option2_x, y, &x1, &y1, &w, &h);
        display.drawLine(option2_x, y + h + 2, option2_x + w, y + h + 2,
                        cursorLine == 1 ? ST77XX_YELLOW : ST77XX_WHITE);
    }

    // Beschriftung "pro Scheibe"
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, 130);
    display.print(F("pro Scheibe"));
}

void ConfigMenu::drawButtonOption() {
    const uint16_t y = 160;
    const uint16_t buttonHeight = 30;
    const uint16_t buttonSpacing = 10;

    // Button-Definitionen
    const uint16_t btn1_x = 10;
    const uint16_t btn1_width = 100;
    const uint16_t btn2_x = btn1_x + btn1_width + buttonSpacing;
    const uint16_t btn2_width = 80;

    // Bereich löschen (komplette Button-Zeile inkl. Schatten)
    display.fillRect(0, y, display.width(), buttonHeight + 2, ST77XX_BLACK);

    // Farben für aktive Zeile
    uint16_t activeColor = cursorLine == 2 ? ST77XX_YELLOW : ST77XX_WHITE;
    uint16_t fillColor = Display::COLOR_DARKGRAY;  // Hintergrund für ausgewählten Button

    // --- Button 1: "Ändern" ---

    // Schatten (1px rechts und unten)
    display.drawRect(btn1_x + 1, y + 1, btn1_width, buttonHeight, ST77XX_BLACK);

    // Hintergrund wenn ausgewählt
    if (selectedButton == 0) {
        display.fillRect(btn1_x, y, btn1_width, buttonHeight, fillColor);
    }

    // Rahmen
    display.drawRect(btn1_x, y, btn1_width, buttonHeight, activeColor);

    // Text zentriert im Button
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(2);
    display.getTextBounds(F("Aendern"), 0, 0, &x1, &y1, &w, &h);
    uint16_t text_x = btn1_x + (btn1_width - w) / 2;
    uint16_t text_y = y + (buttonHeight - h) / 2;

    display.setCursor(text_x, text_y);
    display.setTextColor(activeColor);
    display.print(F("Aendern"));

    // Unterstreichung wenn ausgewählt
    if (selectedButton == 0) {
        display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, activeColor);
    }

    // --- Button 2: "Start" ---

    // Schatten (1px rechts und unten)
    display.drawRect(btn2_x + 1, y + 1, btn2_width, buttonHeight, ST77XX_BLACK);

    // Hintergrund wenn ausgewählt
    if (selectedButton == 1) {
        display.fillRect(btn2_x, y, btn2_width, buttonHeight, fillColor);
    }

    // Rahmen
    display.drawRect(btn2_x, y, btn2_width, buttonHeight, activeColor);

    // Text zentriert im Button
    display.getTextBounds(F("Start"), 0, 0, &x1, &y1, &w, &h);
    text_x = btn2_x + (btn2_width - w) / 2;
    text_y = y + (buttonHeight - h) / 2;

    display.setCursor(text_x, text_y);
    display.setTextColor(activeColor);
    display.print(F("Start"));

    // Unterstreichung wenn ausgewählt
    if (selectedButton == 1) {
        display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, activeColor);
    }
}

void ConfigMenu::drawHelp() {
    // Hilfetext unten
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 30);
    display.print(F("L/R: Aendern, OK: Weiter"));
}
