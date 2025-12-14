 /**
 * @file PfeileHolenMenu.cpp
 * @brief Implementierung des Pfeile-Holen-Menüs
 */

#include "PfeileHolenMenu.h"

PfeileHolenMenu::PfeileHolenMenu(Adafruit_ST7789& tft, ButtonManager& btnMgr)
    : display(tft)
    , buttons(btnMgr)
    , cursorPosition(0)
    , selectedAction(PfeileHolenAction::NONE)
    , needsUpdate(true)
    , firstDraw(true)
    , lastCursorPosition(0xFF)
    , connectionOk(false)
    , lastConnectionOk(false) {
}

void PfeileHolenMenu::begin() {
    DEBUG_PRINTLN(F("PfeileHolenMenu: Initialisiere..."));

    cursorPosition = 0;  // Start bei "Nächste Passe"
    selectedAction = PfeileHolenAction::NONE;
    needsUpdate = true;
    firstDraw = true;
    lastCursorPosition = 0xFF;
    connectionOk = false;
    lastConnectionOk = false;
}

void PfeileHolenMenu::update() {
    // Button-Handling

    // Links: Cursor nach links (mit Wrapping)
    if (buttons.wasPressed(Button::LEFT)) {
        if (cursorPosition == 0) {
            cursorPosition = 2;  // Wrap to last
        } else {
            cursorPosition--;
        }
        needsUpdate = true;
        DEBUG_PRINT(F("PfeileHolenMenu: Cursor -> "));
        DEBUG_PRINTLN(cursorPosition);
    }
    // Rechts: Cursor nach rechts (mit Wrapping)
    else if (buttons.wasPressed(Button::RIGHT)) {
        cursorPosition = (cursorPosition + 1) % 3;  // 0 → 1 → 2 → 0
        needsUpdate = true;
        DEBUG_PRINT(F("PfeileHolenMenu: Cursor -> "));
        DEBUG_PRINTLN(cursorPosition);
    }
    // OK: Aktion auswählen
    else if (buttons.wasPressed(Button::OK)) {
        selectedAction = static_cast<PfeileHolenAction>(cursorPosition);
        DEBUG_PRINT(F("PfeileHolenMenu: Aktion gewaehlt -> "));
        DEBUG_PRINTLN(cursorPosition);
    }
}

void PfeileHolenMenu::draw() {
    // Beim ersten Aufruf: Komplettes Display zeichnen
    if (firstDraw) {
        display.fillScreen(ST77XX_BLACK);
        drawHeader();
        drawOptions();
        drawHelp();
        drawConnectionIcon();

        lastCursorPosition = cursorPosition;
        lastConnectionOk = connectionOk;
        firstDraw = false;

        DEBUG_PRINTLN(F("PfeileHolenMenu: Initial draw"));
    }
    else {
        // Selective Redraw: Nur Optionen neu zeichnen wenn Cursor sich bewegt
        if (cursorPosition != lastCursorPosition) {
            drawOptions();
            lastCursorPosition = cursorPosition;
            DEBUG_PRINTLN(F("PfeileHolenMenu: Selective redraw"));
        }

        // Verbindungsstatus-Icon neu zeichnen wenn Status sich geändert hat
        if (connectionOk != lastConnectionOk) {
            drawConnectionIcon();
            lastConnectionOk = connectionOk;
            DEBUG_PRINTLN(F("PfeileHolenMenu: Connection status changed"));
        }
    }

    needsUpdate = false;
}

//=============================================================================
// Private Hilfsfunktionen für Selective Drawing
//=============================================================================

void PfeileHolenMenu::drawHeader() {
    // Überschrift: "Pfeile holen"
    display.setTextSize(3);
    display.setTextColor(ST77XX_GREEN);
    display.setCursor(10, 10);
    display.print(F("Pfeile holen"));

    // Trennlinie
    display.drawFastHLine(10, 45, display.width() - 20, Display::COLOR_GRAY);
}

void PfeileHolenMenu::drawOptions() {
    const uint16_t buttonY = 60;
    const uint16_t buttonHeight = 40;
    const uint16_t buttonSpacing = 15;
    const uint16_t buttonWidth = 200;
    const uint16_t buttonX = 10;

    // Bereich löschen (alle 3 Buttons + Spacing)
    display.fillRect(0, buttonY, display.width(), 3 * buttonHeight + 2 * buttonSpacing + 2, ST77XX_BLACK);

    // Button-Texte
    const char* buttonTexts[3] = {
        "Naechste Passe",
        "Reihenfolge",
        "Neustart"
    };

    // Alle 3 Buttons zeichnen
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t y = buttonY + i * (buttonHeight + buttonSpacing);
        bool isSelected = (i == cursorPosition);

        // Schatten (1px rechts und unten)
        display.drawRect(buttonX + 1, y + 1, buttonWidth, buttonHeight, ST77XX_BLACK);

        // Hintergrund wenn ausgewählt
        if (isSelected) {
            display.fillRect(buttonX, y, buttonWidth, buttonHeight, Display::COLOR_DARKGRAY);
        }

        // Rahmen (gelb wenn ausgewählt, weiß sonst)
        uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
        display.drawRect(buttonX, y, buttonWidth, buttonHeight, frameColor);

        // Text zentriert im Button
        int16_t x1, y1;
        uint16_t w, h;
        display.setTextSize(2);
        display.getTextBounds(buttonTexts[i], 0, 0, &x1, &y1, &w, &h);
        uint16_t text_x = buttonX + (buttonWidth - w) / 2;
        uint16_t text_y = y + (buttonHeight - h) / 2;

        display.setCursor(text_x, text_y);
        display.setTextColor(frameColor);
        display.print(buttonTexts[i]);

        // Unterstreichung wenn ausgewählt
        if (isSelected) {
            display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
        }
    }
}

void PfeileHolenMenu::drawHelp() {
    // Hilfetext unten
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 30);
    display.print(F("L/R: Auswaehlen, OK: Bestaetigen"));
}

void PfeileHolenMenu::drawConnectionIcon() {
    // Icon-Position: Rechts oben (WLAN-Balken Icon)
    const uint16_t iconX = display.width() - 25;
    const uint16_t iconY = 10;
    const uint16_t iconWidth = 11;
    const uint16_t iconHeight = 10;

    // Text-Position: Unter dem Icon
    const uint16_t textX = iconX;
    const uint16_t textY = iconY + iconHeight + 2;

    // Bereich löschen (Icon + Text)
    display.fillRect(iconX - 2, iconY - 2, iconWidth + 4, iconHeight + 14, ST77XX_BLACK);

    // WLAN-Balken Icon zeichnen (4 Balken unterschiedlicher Höhe)
    // Balken-Höhen: 2, 4, 6, 8 Pixel (von links nach rechts)
    // Balken-Breite: 2px, Abstand: 1px
    const uint8_t barWidth = 2;
    const uint8_t barSpacing = 1;
    const uint8_t barHeights[4] = {2, 4, 6, 8};

    // Anzahl der gefüllten Balken basierend auf Power Level
    uint8_t activeBars = 0;
    switch (RF::POWER_LEVEL) {
        case RF24_PA_MIN:  activeBars = 1; break;
        case RF24_PA_LOW:  activeBars = 2; break;
        case RF24_PA_HIGH: activeBars = 3; break;
        case RF24_PA_MAX:  activeBars = 4; break;
    }

    // Farbe: Grün wenn verbunden, grau sonst
    uint16_t activeColor = connectionOk ? ST77XX_GREEN : Display::COLOR_GRAY;
    uint16_t inactiveColor = Display::COLOR_DARKGRAY;

    // Zeichne alle 4 Balken
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t barX = iconX + i * (barWidth + barSpacing);
        uint16_t barY = iconY + (iconHeight - barHeights[i]);
        uint16_t barH = barHeights[i];

        // Balken gefüllt wenn i < activeBars, sonst Rahmen
        if (connectionOk && i < activeBars) {
            // Gefüllter Balken (grün)
            display.fillRect(barX, barY, barWidth, barH, activeColor);
        } else if (!connectionOk) {
            // Alle Balken grau wenn nicht verbunden
            display.fillRect(barX, barY, barWidth, barH, inactiveColor);
        } else {
            // Inaktiver Balken (nur Rahmen)
            display.drawRect(barX, barY, barWidth, barH, inactiveColor);
        }
    }

    // Text zeichnen ("OK" oder "NO")
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(textX, textY);
    if (connectionOk) {
        display.print(F("OK"));
    } else {
        display.print(F("NO"));
    }
}

void PfeileHolenMenu::updateConnectionStatus(bool isConnected) {
    if (connectionOk != isConnected) {
        connectionOk = isConnected;
        needsUpdate = true;

        DEBUG_PRINT(F("PfeileHolenMenu: Verbindungsstatus -> "));
        DEBUG_PRINTLN(isConnected ? F("OK") : F("NO"));
    }
}
