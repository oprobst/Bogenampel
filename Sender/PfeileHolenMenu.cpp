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
    , lastConnectionOk(false)
    , shooterCount(2)  // Default: 1-2 Schützen
    , currentGroup(Groups::Type::GROUP_AB)
    , currentPosition(Groups::Position::POS_1) {
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
    // Anzahl sichtbarer Buttons bestimmen
    // Bei 1-2 Schützen: 2 Buttons (Nächste Passe, Neustart)
    // Bei 3-4 Schützen: 3 Buttons (Nächste Passe, Reihenfolge, Neustart)
    uint8_t numButtons = (shooterCount == 4) ? 3 : 2;
    uint8_t maxPosition = numButtons - 1;

    // Button-Handling
    // Links: Cursor nach links (mit Wrapping)
    if (buttons.wasPressed(Button::LEFT)) {
        if (cursorPosition == 0) {
            cursorPosition = maxPosition;  // Wrap to last
        } else {
            cursorPosition--;
        }
        needsUpdate = true;
        DEBUG_PRINT(F("PfeileHolenMenu: Cursor -> "));
        DEBUG_PRINTLN(cursorPosition);
    }
    // Rechts: Cursor nach rechts (mit Wrapping)
    else if (buttons.wasPressed(Button::RIGHT)) {
        cursorPosition = (cursorPosition + 1) % numButtons;
        needsUpdate = true;
        DEBUG_PRINT(F("PfeileHolenMenu: Cursor -> "));
        DEBUG_PRINTLN(cursorPosition);
    }
    // OK: Aktion auswählen
    else if (buttons.wasPressed(Button::OK)) {
        // Bei 1-2 Schützen: Position 0=Nächste, 1=Neustart → Position 1 auf 2 mappen
        if (shooterCount == 2 && cursorPosition == 1) {
            selectedAction = PfeileHolenAction::NEUSTART;
        } else {
            selectedAction = static_cast<PfeileHolenAction>(cursorPosition);
        }
        DEBUG_PRINT(F("PfeileHolenMenu: Aktion gewaehlt -> "));
        DEBUG_PRINTLN(static_cast<int>(selectedAction));
    }
}

void PfeileHolenMenu::draw() {
    // Beim ersten Aufruf: Komplettes Display zeichnen
    if (firstDraw) {
        display.fillScreen(ST77XX_BLACK);
        drawHeader();
        drawOptions();
        drawShooterGroupInfo();  // Schützengruppen-Info (nur bei 3-4 Schützen)
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
            drawShooterGroupInfo();  // Schützengruppen-Info aktualisieren
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
    const uint16_t buttonHeight = 35;
    const uint16_t buttonSpacing = 10;
    const uint16_t margin = 10;

    // Anzahl Buttons bestimmen
    uint8_t numButtons = (shooterCount == 4) ? 3 : 2;

    // Bereich löschen
    display.fillRect(0, buttonY, display.width(), 2 * buttonHeight + buttonSpacing + 60, ST77XX_BLACK);

    // =========================================================================
    // Button 0: "Nächste Passe" - Volle Breite oben
    // =========================================================================
    {
        uint16_t btnX = margin;
        uint16_t btnY = buttonY;
        uint16_t btnW = display.width() - 2 * margin;
        uint16_t btnH = buttonHeight;
        bool isSelected = (cursorPosition == 0);

        // Hintergrund wenn ausgewählt
        if (isSelected) {
            display.fillRect(btnX, btnY, btnW, btnH, Display::COLOR_DARKGRAY);
        }

        // Rahmen
        uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
        display.drawRect(btnX, btnY, btnW, btnH, frameColor);

        // Text zentriert
        int16_t x1, y1;
        uint16_t w, h;
        display.setTextSize(2);  // Größere Schrift für Hauptbutton
        display.getTextBounds("Naechste Passe", 0, 0, &x1, &y1, &w, &h);
        display.setCursor(btnX + (btnW - w) / 2, btnY + (btnH - h) / 2);
        display.setTextColor(frameColor);
        display.print("Naechste Passe");

        if (isSelected) {
            uint16_t text_x = btnX + (btnW - w) / 2;
            uint16_t text_y = btnY + (btnH - h) / 2;
            display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
        }
    }

    // =========================================================================
    // Button 1 & 2: "Reihenfolge" (nur bei 3-4 Schützen) und "Neustart" - Halbe Breite nebeneinander
    // =========================================================================
    const uint16_t row2Y = buttonY + buttonHeight + buttonSpacing;
    const uint16_t halfWidth = (display.width() - 3 * margin) / 2;

    if (shooterCount == 4) {
        // Bei 3-4 Schützen: "Reihenfolge" links, "Neustart" rechts

        // "Abfolge" (Position 1)
        {
            uint16_t btnX = margin;
            uint16_t btnY = row2Y;
            uint16_t btnW = halfWidth;
            uint16_t btnH = buttonHeight;
            bool isSelected = (cursorPosition == 1);

            if (isSelected) {
                display.fillRect(btnX, btnY, btnW, btnH, Display::COLOR_DARKGRAY);
            }

            uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
            display.drawRect(btnX, btnY, btnW, btnH, frameColor);

            int16_t x1, y1;
            uint16_t w, h;
            display.setTextSize(2);
            display.getTextBounds("Abfolge", 0, 0, &x1, &y1, &w, &h);
            display.setCursor(btnX + (btnW - w) / 2, btnY + (btnH - h) / 2);
            display.setTextColor(frameColor);
            display.print("Abfolge");

            if (isSelected) {
                uint16_t text_x = btnX + (btnW - w) / 2;
                uint16_t text_y = btnY + (btnH - h) / 2;
                display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
            }
        }

        // "Neustart" (Position 2)
        {
            uint16_t btnX = margin + halfWidth + margin;
            uint16_t btnY = row2Y;
            uint16_t btnW = halfWidth;
            uint16_t btnH = buttonHeight;
            bool isSelected = (cursorPosition == 2);

            if (isSelected) {
                display.fillRect(btnX, btnY, btnW, btnH, Display::COLOR_DARKGRAY);
            }

            uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
            display.drawRect(btnX, btnY, btnW, btnH, frameColor);

            int16_t x1, y1;
            uint16_t w, h;
            display.setTextSize(2);
            display.getTextBounds("Neustart", 0, 0, &x1, &y1, &w, &h);
            display.setCursor(btnX + (btnW - w) / 2, btnY + (btnH - h) / 2);
            display.setTextColor(frameColor);
            display.print("Neustart");

            if (isSelected) {
                uint16_t text_x = btnX + (btnW - w) / 2;
                uint16_t text_y = btnY + (btnH - h) / 2;
                display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
            }
        }
    } else {
        // Bei 1-2 Schützen: Nur "Neustart" volle Breite
        {
            uint16_t btnX = margin;
            uint16_t btnY = row2Y;
            uint16_t btnW = display.width() - 2 * margin;
            uint16_t btnH = buttonHeight;
            bool isSelected = (cursorPosition == 1);

            if (isSelected) {
                display.fillRect(btnX, btnY, btnW, btnH, Display::COLOR_DARKGRAY);
            }

            uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
            display.drawRect(btnX, btnY, btnW, btnH, frameColor);

            int16_t x1, y1;
            uint16_t w, h;
            display.setTextSize(2);
            display.getTextBounds("Neustart", 0, 0, &x1, &y1, &w, &h);
            display.setCursor(btnX + (btnW - w) / 2, btnY + (btnH - h) / 2);
            display.setTextColor(frameColor);
            display.print("Neustart");

            if (isSelected) {
                uint16_t text_x = btnX + (btnW - w) / 2;
                uint16_t text_y = btnY + (btnH - h) / 2;
                display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
            }
        }
    }
}

void PfeileHolenMenu::drawHelp() {
    // Hilfetext unten
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 15);
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
    }
}

void PfeileHolenMenu::setTournamentConfig(uint8_t shooters, Groups::Type group, Groups::Position position) {
    shooterCount = shooters;
    currentGroup = group;
    currentPosition = position;
    needsUpdate = true;  
}

void PfeileHolenMenu::drawShooterGroupInfo() {
    // Nur bei 3-4 Schützen anzeigen
    if (shooterCount != 4) return;

    // Position: Unter dem "Neustart" Button
    const uint16_t infoY = display.height() - 65;
    const uint16_t infoX = 10;
    const uint16_t lineHeight = 20; // Zeilenhöhe für Textgröße 2

    // Bereich löschen
    display.fillRect(0, infoY, display.width(), 40, ST77XX_BLACK);

    // Zeile 1: "Nächste: A/B" oder "Nächste: C/D"
    display.setCursor(infoX, infoY);
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);
    display.print(F("Naechste: "));
    display.setTextColor(ST77XX_YELLOW);
    display.println(currentGroup == Groups::Type::GROUP_AB ? F("A/B") : F("C/D"));

    // Zeile 2: Statischer String mit Hervorhebung
    display.setCursor(infoX, infoY + lineHeight);
    display.setTextSize(2);

    // "{A/B -> C/D} {C/D -> A/B}"
    // Teile: "{A/B", " -> ", "C/D}", " ", "{C/D", " -> ", "A/B}"

    // Bestimme welcher Teil gelb sein soll
    bool highlightAB1 = (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_1);
    bool highlightCD1 = (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_1);
    bool highlightCD2 = (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_2);
    bool highlightAB2 = (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_2);

    // "{A/B"
    display.setTextColor(highlightAB1 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("{A/B"));

    // " -> "
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F(" -> "));

    // "C/D}"
    display.setTextColor(highlightCD1 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("C/D}"));

    // " "
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F(" "));

    // "{C/D"
    display.setTextColor(highlightCD2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("{C/D"));

    // " -> "
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F(" -> "));

    // "A/B}"
    display.setTextColor(highlightAB2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("A/B}"));
}
