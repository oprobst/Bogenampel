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

        firstDraw = false;
    }
    // Selective Redraw: Nur bei Phasenwechsel (Vorbereitung ↔ Schießbetrieb)
    // Da wir keine Sekunden mehr anzeigen, kein Update bei jeder Sekunde nötig
    else if (needsUpdate) {
        updateTimer();
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
        // Gruppensequenz: "{A/B -> C/D} {C/D -> A/B}"
        display.setCursor(10, 58);
        display.setTextSize(2);

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
    // Bei 1-2 Schützen: Keine Gruppenanzeige
}

void SchiessBetriebMenu::drawTimer() {
    // Phasentext statt Timer
    const char* phaseText = inPreparationPhase ? "Vorbereitung" : "Alles ins Gold";
    uint16_t phaseColor = inPreparationPhase ? Display::COLOR_ORANGE : ST77XX_GREEN;

    // Position: Bei 3-4 Schützen tiefer setzen (wegen Gruppenanzeige)
    uint16_t phaseY = (shooterCount == 4) ? 88 : 65;

    display.setTextSize(2);
    display.setTextColor(phaseColor);

    // Text horizontal zentrieren
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(phaseText, 0, 0, &x1, &y1, &w, &h);
    uint16_t phaseX = (display.width() - w) / 2;

    display.setCursor(phaseX, phaseY);
    display.print(phaseText);

    // Aktuelle Gruppe groß darunter anzeigen (nur bei 3-4 Schützen)
    if (shooterCount == 4) {
        const char* groupText = (currentGroup == Groups::Type::GROUP_AB) ? "A/B" : "C/D";

        display.setTextSize(6);  // Sehr groß
        display.setTextColor(ST77XX_YELLOW);

        // Text zentrieren
        display.getTextBounds(groupText, 0, 0, &x1, &y1, &w, &h);
        uint16_t groupX = (display.width() - w) / 2;
        uint16_t groupY = 115;  // Unterhalb der Phase

        display.setCursor(groupX, groupY);
        display.print(groupText);
    }
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
    // Phasentext-Position: Bei 3-4 Schützen höher setzen (wegen Gruppenanzeige darunter)
    const uint16_t phaseY = (shooterCount == 4) ? 88 : 65;

    // Lösche gesamten Bereich (Phase + große Gruppe bei 3-4 Schützen)
    const uint16_t clearHeight = (shooterCount == 4) ? 80 : 20;  // Mehr Platz für große Gruppe
    display.fillRect(0, phaseY, display.width(), clearHeight, ST77XX_BLACK);

    // Phasentext und -farbe
    const char* phaseText = inPreparationPhase ? "Vorbereitung" : "Alle ins Gold";
    uint16_t phaseColor = inPreparationPhase ? Display::COLOR_ORANGE : ST77XX_GREEN;

    // Text horizontal zentrieren
    display.setTextSize(2);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(phaseText, 0, 0, &x1, &y1, &w, &h);
    uint16_t phaseX = (display.width() - w) / 2;

    // Phasentext neu zeichnen
    display.setTextColor(phaseColor);
    display.setCursor(phaseX, phaseY);
    display.print(phaseText);

    // Aktuelle Gruppe groß darunter anzeigen (nur bei 3-4 Schützen)
    if (shooterCount == 4) {
        const char* groupText = (currentGroup == Groups::Type::GROUP_AB) ? "A/B" : "C/D";

        display.setTextSize(6);  // Sehr groß
        display.setTextColor(ST77XX_YELLOW);

        // Text zentrieren
        display.getTextBounds(groupText, 0, 0, &x1, &y1, &w, &h);
        uint16_t groupX = (display.width() - w) / 2;
        uint16_t groupY = 115;  // Unterhalb der Phase

        display.setCursor(groupX, groupY);
        display.print(groupText);
    }
}
