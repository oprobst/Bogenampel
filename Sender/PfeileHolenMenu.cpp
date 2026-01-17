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
    , pingHistoryIndex(0)
    , pingHistoryUpdated(false)
    , batteryVoltage(0)
    , isUsbPowered(true)
    , batteryUpdated(false)
    , shooterCount(2)  // Default: 1-2 Schützen
    , currentGroup(Groups::Type::GROUP_AB)
    , currentPosition(Groups::Position::POS_1)
    , groupConfigChanged(false) {
    // Ping-Historie mit false initialisieren (keine ACKs)
    for (uint8_t i = 0; i < 4; i++) {
        pingHistory[i] = false;
    }
}

void PfeileHolenMenu::begin() {
    cursorPosition = 0;  // Start bei "Nächste Passe"
    selectedAction = PfeileHolenAction::NONE;
    needsUpdate = true;
    firstDraw = true;
    lastCursorPosition = 0xFF;
    connectionOk = false;
    lastConnectionOk = false;

    // Ping-Historie zurücksetzen
    pingHistoryIndex = 0;
    pingHistoryUpdated = false;
    for (uint8_t i = 0; i < 4; i++) {
        pingHistory[i] = false;
    }

    // Batteriestatus zurücksetzen
    batteryVoltage = 0;
    isUsbPowered = true;
    batteryUpdated = false;

    // Gruppen-Konfiguration zurücksetzen
    groupConfigChanged = false;
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
    }
    // Rechts: Cursor nach rechts (mit Wrapping)
    else if (buttons.wasPressed(Button::RIGHT)) {
        cursorPosition = (cursorPosition + 1) % numButtons;
        needsUpdate = true;
    }
    // OK: Aktion auswählen
    else if (buttons.wasPressed(Button::OK)) {
        // Bei 1-2 Schützen: Position 0=Nächste, 1=Neustart → Position 1 auf 2 mappen
        if (shooterCount == 2 && cursorPosition == 1) {
            selectedAction = PfeileHolenAction::NEUSTART;
        } else {
            selectedAction = static_cast<PfeileHolenAction>(cursorPosition);
        }
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
        drawBatteryIcon();       // Batteriestatus
        drawConnectionIcon();    // Verbindungsstatus

        lastCursorPosition = cursorPosition;
        lastConnectionOk = connectionOk;
        firstDraw = false;
    }
    else {
        // Selective Redraw: Nur Optionen neu zeichnen wenn Cursor sich bewegt
        if (cursorPosition != lastCursorPosition) {
            drawOptions();
            drawShooterGroupInfo();  // Schützengruppen-Info aktualisieren
            lastCursorPosition = cursorPosition;
        }

        // Verbindungsstatus-Icon neu zeichnen wenn Ping-Historie aktualisiert wurde
        if (pingHistoryUpdated) {
            drawConnectionIcon();
            pingHistoryUpdated = false;
        }

        // Batterie-Icon neu zeichnen wenn Status aktualisiert wurde
        if (batteryUpdated) {
            drawBatteryIcon();
            batteryUpdated = false;
        }

        // Schützengruppen-Info neu zeichnen wenn Konfiguration geändert wurde
        if (groupConfigChanged) {
            drawShooterGroupInfo();
            groupConfigChanged = false;
        }
    }

    needsUpdate = false;
}

//=============================================================================
// Private Hilfsfunktionen für Selective Drawing
//=============================================================================

void PfeileHolenMenu::drawHeader() {
    // Überschrift: "Pfeile holen" (linksbündig, damit Batterie-Icon nicht überdeckt)
    display.setTextSize(2);
    display.setTextColor(ST77XX_GREEN);
    display.setCursor(10, 15);
    display.print(F("Pfeile holen"));

    // Trennlinie
    display.drawFastHLine(10, 45, display.width() - 20, Display::COLOR_GRAY);
}

void PfeileHolenMenu::drawOptions() {
    // Portrait: Alle Buttons übereinander (volle Breite)
    const uint16_t buttonY = 60;
    const uint16_t buttonHeight = 40;
    const uint16_t buttonSpacing = 10;
    const uint16_t margin = 20;
    const uint16_t buttonWidth = display.width() - 2 * margin;

    // Anzahl Buttons bestimmen
    uint8_t numButtons = (shooterCount == 4) ? 3 : 2;

    // Bereich löschen
    display.fillRect(0, buttonY, display.width(), numButtons * (buttonHeight + buttonSpacing) + 10, ST77XX_BLACK);

    int16_t x1, y1;
    uint16_t w, h;

    // =========================================================================
    // Button 0: "Nächste Passe"
    // =========================================================================
    {
        uint16_t btnY = buttonY;
        bool isSelected = (cursorPosition == 0);

        if (isSelected) {
            display.fillRect(margin, btnY, buttonWidth, buttonHeight, Display::COLOR_DARKGRAY);
        }

        uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
        display.drawRect(margin, btnY, buttonWidth, buttonHeight, frameColor);

        display.setTextSize(2);
        display.getTextBounds("Naechste Passe", 0, 0, &x1, &y1, &w, &h);
        uint16_t text_x = margin + (buttonWidth - w) / 2;
        uint16_t text_y = btnY + (buttonHeight - h) / 2;
        display.setCursor(text_x, text_y);
        display.setTextColor(frameColor);
        display.print("Naechste Passe");

        if (isSelected) {
            display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
        }
    }

    if (shooterCount == 4) {
        // =========================================================================
        // Button 1: "Abfolge" (nur bei 3-4 Schützen)
        // =========================================================================
        {
            uint16_t btnY = buttonY + buttonHeight + buttonSpacing;
            bool isSelected = (cursorPosition == 1);

            if (isSelected) {
                display.fillRect(margin, btnY, buttonWidth, buttonHeight, Display::COLOR_DARKGRAY);
            }

            uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
            display.drawRect(margin, btnY, buttonWidth, buttonHeight, frameColor);

            display.setTextSize(2);
            display.getTextBounds("Abfolge", 0, 0, &x1, &y1, &w, &h);
            uint16_t text_x = margin + (buttonWidth - w) / 2;
            uint16_t text_y = btnY + (buttonHeight - h) / 2;
            display.setCursor(text_x, text_y);
            display.setTextColor(frameColor);
            display.print("Abfolge");

            if (isSelected) {
                display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
            }
        }

        // =========================================================================
        // Button 2: "Neustart"
        // =========================================================================
        {
            uint16_t btnY = buttonY + 2 * (buttonHeight + buttonSpacing);
            bool isSelected = (cursorPosition == 2);

            if (isSelected) {
                display.fillRect(margin, btnY, buttonWidth, buttonHeight, Display::COLOR_DARKGRAY);
            }

            uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
            display.drawRect(margin, btnY, buttonWidth, buttonHeight, frameColor);

            display.setTextSize(2);
            display.getTextBounds("Neustart", 0, 0, &x1, &y1, &w, &h);
            uint16_t text_x = margin + (buttonWidth - w) / 2;
            uint16_t text_y = btnY + (buttonHeight - h) / 2;
            display.setCursor(text_x, text_y);
            display.setTextColor(frameColor);
            display.print("Neustart");

            if (isSelected) {
                display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
            }
        }
    } else {
        // =========================================================================
        // Button 1: "Neustart" (bei 1-2 Schützen)
        // =========================================================================
        {
            uint16_t btnY = buttonY + buttonHeight + buttonSpacing;
            bool isSelected = (cursorPosition == 1);

            if (isSelected) {
                display.fillRect(margin, btnY, buttonWidth, buttonHeight, Display::COLOR_DARKGRAY);
            }

            uint16_t frameColor = isSelected ? ST77XX_YELLOW : ST77XX_WHITE;
            display.drawRect(margin, btnY, buttonWidth, buttonHeight, frameColor);

            display.setTextSize(2);
            display.getTextBounds("Neustart", 0, 0, &x1, &y1, &w, &h);
            uint16_t text_x = margin + (buttonWidth - w) / 2;
            uint16_t text_y = btnY + (buttonHeight - h) / 2;
            display.setCursor(text_x, text_y);
            display.setTextColor(frameColor);
            display.print("Neustart");

            if (isSelected) {
                display.drawLine(text_x, text_y + h + 1, text_x + w, text_y + h + 1, frameColor);
            }
        }
    }
}

void PfeileHolenMenu::drawHelp() {
    // Hilfetext unten (Portrait: mehr Platz)
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 20);
    display.print(F("L/R: Auswaehlen"));
    display.setCursor(10, display.height() - 8);
    display.print(F("OK: Bestaetigen"));
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

    // Farben
    uint16_t successColor = ST77XX_GREEN;      // Grün für erfolgreichen ACK
    uint16_t failColor = Display::COLOR_GRAY;  // Grau für fehlgeschlagenen ACK

    // Zeichne alle 4 Balken basierend auf Ping-Historie
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t barX = iconX + i * (barWidth + barSpacing);
        uint16_t barY = iconY + (iconHeight - barHeights[i]);
        uint16_t barH = barHeights[i];

        // Balken grün wenn ACK empfangen wurde, sonst grau
        uint16_t barColor = pingHistory[i] ? successColor : failColor;
        display.fillRect(barX, barY, barWidth, barH, barColor);
    }

    // Zähle erfolgreiche Pings für Text-Anzeige
    uint8_t successfulPings = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (pingHistory[i]) successfulPings++;
    }

    // Text zeichnen: Anzahl erfolgreicher Pings von 4 (z.B. "3/4")
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(textX, textY);
    display.print(successfulPings);
    display.print(F("/4"));
}

void PfeileHolenMenu::drawBatteryIcon() {
    // Icon-Position: Links vom Empfangs-Icon
    const uint16_t iconX = display.width() - 60;  // 35 Pixel links vom Empfangs-Icon
    const uint16_t iconY = 10;
    const uint16_t iconWidth = 20;
    const uint16_t iconHeight = 10;

    // Text-Position: Unter dem Icon
    const uint16_t textX = iconX - 5;
    const uint16_t textY = iconY + iconHeight + 2;

    // Bereich löschen (Icon + Text)
    display.fillRect(iconX - 7, iconY - 2, iconWidth + 14, iconHeight + 16, ST77XX_BLACK);

    // Batterie-Icon zeichnen
    const uint16_t bodyWidth = 16;
    const uint16_t bodyHeight = 8;
    const uint16_t terminalWidth = 2;
    const uint16_t terminalHeight = 4;

    // Batterie-Rahmen (Körper)
    uint16_t frameColor = ST77XX_WHITE;
    display.drawRect(iconX, iconY, bodyWidth, bodyHeight, frameColor);

    // Batterie-Terminal (Pluspol, rechts)
    display.fillRect(iconX + bodyWidth, iconY + 2, terminalWidth, terminalHeight, frameColor);

    // Füllstand berechnen und zeichnen
    if (isUsbPowered) {
        // USB-Modus: Volle Batterie (grün)
        display.fillRect(iconX + 2, iconY + 2, bodyWidth - 4, bodyHeight - 4, ST77XX_GREEN);
    } else {
        // Batteriemodus: Füllstand basierend auf Spannung
        // Prozentsatz berechnen
        uint16_t percent = 0;
        if (batteryVoltage >= Battery::VOLTAGE_MAX_MV) {
            percent = 100;
        } else if (batteryVoltage <= Battery::VOLTAGE_MIN_MV) {
            percent = 0;
        } else {
            percent = ((batteryVoltage - Battery::VOLTAGE_MIN_MV) * 100) /
                      (Battery::VOLTAGE_MAX_MV - Battery::VOLTAGE_MIN_MV);
        }

        // Füllbalken-Breite berechnen
        uint16_t fillWidth = ((bodyWidth - 4) * percent) / 100;

        // Farbe basierend auf Füllstand
        uint16_t fillColor;
        if (percent > 50) {
            fillColor = ST77XX_GREEN;
        } else if (percent > 20) {
            fillColor = ST77XX_YELLOW;
        } else {
            fillColor = ST77XX_RED;
        }

        // Füllbalken zeichnen
        if (fillWidth > 0) {
            display.fillRect(iconX + 2, iconY + 2, fillWidth, bodyHeight - 4, fillColor);
        }
    }

    // Text zeichnen: "USB" oder Spannung
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(textX, textY);

    if (isUsbPowered) {
        display.print(F("USB"));
    } else {
        // Spannung in Volt anzeigen (z.B. "7.2V") - ohne Float!
        display.print(batteryVoltage / 1000);  // Volt-Anteil
        display.print(F("."));
        display.print((batteryVoltage % 1000) / 100);  // Erste Dezimalstelle
        display.print(F("V"));
    }
}

void PfeileHolenMenu::updateConnectionStatus(bool isConnected) {
    // Neues Ping-Ergebnis im Ring-Buffer speichern
    pingHistory[pingHistoryIndex] = isConnected;
    pingHistoryIndex = (pingHistoryIndex + 1) % 4;  // Nächster Index (0-3 mit Wrapping)

    // connectionOk Status aktualisieren (aktuellstes Ping-Ergebnis)
    connectionOk = isConnected;

    // Flags setzen für Neuzeichnung
    pingHistoryUpdated = true;
    needsUpdate = true;
}

void PfeileHolenMenu::updateBatteryStatus(uint16_t voltageMillivolts, bool usbPowered) {
    batteryVoltage = voltageMillivolts;
    isUsbPowered = usbPowered;
    batteryUpdated = true;
    needsUpdate = true;
}

void PfeileHolenMenu::setTournamentConfig(uint8_t shooters, Groups::Type group, Groups::Position position) {
    // Prüfe ob sich Gruppe oder Position geändert hat
    bool changed = (currentGroup != group) || (currentPosition != position) || (shooterCount != shooters);

    shooterCount = shooters;
    currentGroup = group;
    currentPosition = position;

    if (changed) {
        groupConfigChanged = true;
        needsUpdate = true;
    }
}

void PfeileHolenMenu::drawShooterGroupInfo() {
    // Nur bei 3-4 Schützen anzeigen
    if (shooterCount != 4) return;

    // Portrait: Position unter den 3 Buttons (60 + 3*50 = 210)
    const uint16_t infoY = 220;
    const uint16_t infoX = 10;
    const uint16_t lineHeight = 22;

    // Bereich löschen
    display.fillRect(0, infoY, display.width(), 70, ST77XX_BLACK);

    // Zeile 1: "Nächste: A/B" oder "Nächste: C/D"
    display.setCursor(infoX, infoY);
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);
    display.print(F("Naechste: "));
    display.setTextColor(ST77XX_YELLOW);
    display.print(currentGroup == Groups::Type::GROUP_AB ? F("A/B") : F("C/D"));

    // Bestimme welcher Teil gelb sein soll
    bool highlightAB1 = (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_1);
    bool highlightCD1 = (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_1);
    bool highlightCD2 = (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_2);
    bool highlightAB2 = (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_2);

    // Portrait: Zwei Zeilen für Gruppensequenz
    // Zeile 2: {A/B -> C/D}
    display.setCursor(infoX, infoY + lineHeight);
    display.setTextSize(2);
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F("{"));
    display.setTextColor(highlightAB1 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("A/B"));
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F(" -> "));
    display.setTextColor(highlightCD2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("C/D"));
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F("}"));

    // Zeile 3: {C/D -> A/B}
    display.setCursor(infoX, infoY + 2 * lineHeight);
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F("{"));
    display.setTextColor(highlightCD1 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("C/D"));
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F(" -> "));
    display.setTextColor(highlightAB2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
    display.print(F("A/B"));
    display.setTextColor(Display::COLOR_GRAY);
    display.print(F("}"));
}
