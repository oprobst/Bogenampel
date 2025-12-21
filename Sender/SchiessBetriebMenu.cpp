/**
 * @file SchiessBetriebMenu.cpp
 * @brief Implementierung des Schießbetrieb-Menüs
 */

#include "SchiessBetriebMenu.h"

SchiessBetriebMenu::SchiessBetriebMenu(Adafruit_ST7789& tft, ButtonManager& btnMgr)
    : display(tft)
    , buttons(btnMgr)
    , shootingTime(120)
    , shooterCount(2)
    , currentGroup(Groups::Type::GROUP_AB)
    , currentPosition(Groups::Position::POS_1)
    , inPreparationPhase(true)
    , remainingSec(0)
    , lastRemainingSec(0xFFFF)
    , needsUpdate(true)
    , firstDraw(true)
    , endRequested(false) {
}

void SchiessBetriebMenu::begin() {
    needsUpdate = true;
    firstDraw = true;
    endRequested = false;
    lastRemainingSec = 0xFFFF;  // Force timer redraw
}

void SchiessBetriebMenu::update() {
    // Button-Handling: OK = "Passe beenden"
    if (buttons.wasPressed(Button::OK)) {
        endRequested = true;
    }
}

void SchiessBetriebMenu::draw() {
    // Beim ersten Aufruf: Komplettes Display zeichnen
    if (firstDraw) {
        display.fillScreen(ST77XX_BLACK);
        drawHeader();
        drawGroupSequence();
        drawTimer();
        drawEndButton();
        drawHelp();

        lastRemainingSec = remainingSec;
        firstDraw = false;
    }
    else {
        // Selective Redraw: Nur Timer aktualisieren wenn sich Zeit geändert hat
        if (remainingSec != lastRemainingSec) {
            updateTimer();
            lastRemainingSec = remainingSec;
        }
    }

    needsUpdate = false;
}

void SchiessBetriebMenu::setTournamentConfig(uint8_t shootingTime, uint8_t shooterCount,
                                             Groups::Type group, Groups::Position position) {
    // Prüfe ob sich Gruppe/Position geändert hat (erfordert komplettes Neuzeichnen)
    bool groupChanged = (this->currentGroup != group) || (this->currentPosition != position);

    this->shootingTime = shootingTime;
    this->shooterCount = shooterCount;
    this->currentGroup = group;
    this->currentPosition = position;

    if (groupChanged) {
        firstDraw = true;  // Komplettes Neuzeichnen
        needsUpdate = true;
    }
}

void SchiessBetriebMenu::setPreparationPhase(bool inPrep, uint32_t remainingMs) {
    this->inPreparationPhase = inPrep;
    this->remainingSec = (remainingMs + 999) / 1000;  // Aufrunden auf Sekunden
    needsUpdate = true;
}

void SchiessBetriebMenu::setShootingPhase(uint32_t remainingMs) {
    this->inPreparationPhase = false;
    this->remainingSec = (remainingMs + 999) / 1000;  // Aufrunden auf Sekunden
    needsUpdate = true;
}

//=============================================================================
// Private Hilfsfunktionen für Selective Drawing
//=============================================================================

void SchiessBetriebMenu::drawHeader() {
    // Überschrift: "Schiessbetrieb" in Orange
    display.setTextSize(3);
    display.setTextColor(ST77XX_ORANGE);
    display.setCursor(10, 10);
    display.print(F("Schiessbetrieb"));

    // Trennlinie
    display.drawFastHLine(10, 50, display.width() - 20, Display::COLOR_GRAY);
}

void SchiessBetriebMenu::drawGroupSequence() {
    // Gruppensequenz anzeigen (nur bei 3-4 Schützen)
    if (shooterCount == 4) {
        // Zeile 1: "Aktuell: A/B" oder "Aktuell: C/D"
        display.setCursor(10, 58);
        display.setTextSize(2);
        display.setTextColor(ST77XX_WHITE);
        display.print(F("Aktuell: "));
        display.setTextColor(ST77XX_YELLOW);
        display.println(currentGroup == Groups::Type::GROUP_AB ? F("A/B") : F("C/D"));

        // Zeile 2: Statischer String mit Hervorhebung
        display.setCursor(10, 82);
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
        display.setTextColor(highlightCD2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
        display.print(F("C/D}"));

        // " "
        display.setTextColor(Display::COLOR_GRAY);
        display.print(F(" "));

        // "{C/D"
        display.setTextColor(highlightCD1 ? ST77XX_YELLOW : Display::COLOR_GRAY);
        display.print(F("{C/D"));

        // " -> "
        display.setTextColor(Display::COLOR_GRAY);
        display.print(F(" -> "));

        // "A/B}"
        display.setTextColor(highlightAB2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
        display.print(F("A/B}"));
    }
    // Bei 1-2 Schützen: Keine Gruppenanzeige (nur A/B, keine Rotation)
}

void SchiessBetriebMenu::drawTimer() {
    // Timer-Farbe basierend auf Phase
    uint16_t timerColor = inPreparationPhase ? Display::COLOR_ORANGE : ST77XX_GREEN;

    // Timer-Position: Bei 3-4 Schützen tiefer setzen (wegen Gruppenanzeige)
    uint16_t timerY = (shooterCount == 4) ? 112 : 65;

    display.setTextSize(4);
    display.setTextColor(timerColor);

    // Timer horizontal zentrieren
    char timerText[8];
    sprintf(timerText, "%ds", remainingSec);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timerText, 0, 0, &x1, &y1, &w, &h);
    uint16_t timerX = (display.width() - w) / 2;

    display.setCursor(timerX, timerY);
    display.print(remainingSec);
    display.print(F("s"));
}

void SchiessBetriebMenu::drawEndButton() {
    // Button "Passe beenden" - knapp über dem grauen Text
    const uint16_t btnY = 184;  // 8 Pixel höher als vorher
    const uint16_t btnH = 30;
    const uint16_t margin = 10;
    uint16_t btnW = display.width() - 2 * margin;

    // Grauer Hintergrund (wie bei anderen Buttons)
    display.fillRect(margin, btnY, btnW, btnH, Display::COLOR_DARKGRAY);

    // Oranger Rahmen
    display.drawRect(margin, btnY, btnW, btnH, Display::COLOR_ORANGE);

    display.setTextSize(2);
    display.setTextColor(Display::COLOR_ORANGE);

    // Text zentrieren
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("Passe beenden", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(margin + (btnW - w) / 2, btnY + (btnH - h) / 2);
    display.print(F("Passe beenden"));
}

void SchiessBetriebMenu::drawHelp() {
    // Hinweis unten
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 15);
    display.print(F("OK: Passe beenden"));
}

void SchiessBetriebMenu::updateTimer() {
    // Timer-Position: Bei 3-4 Schützen tiefer setzen (wegen Gruppenanzeige)
    const uint16_t timerY = (shooterCount == 4) ? 112 : 65;
    const uint16_t timerH = 32;   // Höhe für Textgröße 4

    // Timer-Farbe basierend auf Phase
    uint16_t timerColor = inPreparationPhase ? Display::COLOR_ORANGE : ST77XX_GREEN;

    // Timer horizontal zentrieren
    display.setTextSize(4);
    char timerText[8];
    sprintf(timerText, "%ds", remainingSec);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timerText, 0, 0, &x1, &y1, &w, &h);
    uint16_t timerX = (display.width() - w) / 2;

    // Timer-Bereich löschen (volle Breite um Flackern zu vermeiden)
    display.fillRect(0, timerY, display.width(), timerH, ST77XX_BLACK);

    // Timer neu zeichnen
    display.setTextColor(timerColor);
    display.setCursor(timerX, timerY);
    display.print(remainingSec);
    display.print(F("s"));
}
