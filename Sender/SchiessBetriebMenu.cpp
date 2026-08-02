/**
 * @file SchiessBetriebMenu.cpp
 * @brief Implementierung des Schießbetrieb-Menüs (e-Paper)
 */

#include "SchiessBetriebMenu.h"

#include <stdio.h>

SchiessBetriebMenu::SchiessBetriebMenu(EpaperDisplay& epdRef, ButtonManager& btnMgr)
    : epd(epdRef)
    , buttons(btnMgr)
    , shootingTime(120)
    , shooterCount(2)
    , currentGroup(Groups::Type::GROUP_AB)
    , currentPosition(Groups::Position::POS_1)
    , inPreparationPhase(true)
    , lastDrawnSec(0xFFFFFFFF)
    , needsUpdate(true)
    , endRequested(false) {
}

void SchiessBetriebMenu::begin() {
    needsUpdate = true;
    endRequested = false;
    lastDrawnSec = 0xFFFFFFFF;
}

void SchiessBetriebMenu::update() {
    // OK kurz = "Passe beenden". Der erste Klick einer Alarm-Folge landet hier
    // ebenfalls — der Alarm überschreibt den Zustand danach (FR-013).
    if (buttons.wasClicked(Button::OK)) {
        endRequested = true;
    }
}

void SchiessBetriebMenu::setTournamentConfig(uint16_t time, uint8_t count,
                                             Groups::Type group, Groups::Position position) {
    bool groupChanged = (currentGroup != group) || (currentPosition != position);

    shootingTime = time;
    shooterCount = count;
    currentGroup = group;
    currentPosition = position;

    if (groupChanged) {
        needsUpdate = true;
    }
}

void SchiessBetriebMenu::setPreparationPhase(bool inPrep, uint32_t remainingMs) {
    inPreparationPhase = inPrep;
    lastDrawnSec = 0xFFFFFFFF;  // Countdown-Fenster neu zeichnen
    needsUpdate = true;
    (void)remainingMs;  // Restzeit kommt über updateCountdown()
}

void SchiessBetriebMenu::setShootingPhase(uint32_t remainingMs) {
    setPreparationPhase(false, remainingMs);
}

void SchiessBetriebMenu::draw() {
    Adafruit_GFX& g = epd.gfx();

    // Inhalt unterhalb der Statuszeile löschen
    g.fillRect(0, Display::STATUS_H, EpaperDisplay::WIDTH,
               EpaperDisplay::HEIGHT - Display::STATUS_H, GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    // Überschrift
    epd.printCentered("Schiessbetrieb", 30, 2);

    // Phasentext (V2: "Vorbereitung" orange / "Alles ins Gold" grün —
    // auf s/w-e-Paper nur Text)
    const char* phaseText = inPreparationPhase ? "Vorbereitung" : "Alles ins Gold";
    epd.printCentered(phaseText, 54, 2);

    // Aktuelle Gruppe (nur bei 3-4 Schützen): invertierter Balken
    if (shooterCount == 4) {
        const char* groupText = (currentGroup == Groups::Type::GROUP_AB) ? "A/B" : "C/D";
        const int16_t gy = 148;
        g.fillRect(60, gy - 4, 80, 32, GxEPD_BLACK);
        g.setTextColor(GxEPD_WHITE);
        epd.printCentered(groupText, gy, 3);
        g.setTextColor(GxEPD_BLACK);
    }

    // Hilfetext unten
    g.setTextSize(1);
    g.setCursor(10, EpaperDisplay::HEIGHT - 24);
    g.print("OK: Passe beenden");
    g.setCursor(10, EpaperDisplay::HEIGHT - 12);
    g.print("OK 2s halten: ALARM");

    // Countdown-Fenster initial füllen (Refresh übernimmt die StateMachine)
    if (lastDrawnSec != 0xFFFFFFFF) {
        drawCountdownValue(lastDrawnSec);
    }

    needsUpdate = false;
}

void SchiessBetriebMenu::drawCountdownValue(uint32_t remainingSec) {
    Adafruit_GFX& g = epd.gfx();

    // Countdown-Fenster löschen (data-model.md §7)
    g.fillRect(Display::COUNTDOWN_X, Display::COUNTDOWN_Y,
               Display::COUNTDOWN_W, Display::COUNTDOWN_H, GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    // Vorbereitung: Sekunden ("10"); Schießphase: mm:ss ("2:00")
    char buf[8];
    if (inPreparationPhase) {
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)remainingSec);
    } else {
        snprintf(buf, sizeof(buf), "%lu:%02lu",
                 (unsigned long)(remainingSec / 60), (unsigned long)(remainingSec % 60));
    }

    // Größe 4 (Zeichen 24x32): "4:00" = 96 px — passt ins 120-px-Fenster
    int16_t x1, y1;
    uint16_t w, h;
    g.setTextSize(4);
    g.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
    g.setCursor(Display::COUNTDOWN_X + (Display::COUNTDOWN_W - w) / 2,
                Display::COUNTDOWN_Y + (Display::COUNTDOWN_H - h) / 2);
    g.print(buf);
}

void SchiessBetriebMenu::updateCountdown(uint32_t remainingSec) {
    if (remainingSec == lastDrawnSec) return;
    lastDrawnSec = remainingSec;

    drawCountdownValue(remainingSec);

    // Partial-Refresh nur des Countdown-Fensters (1 Hz, kein Blitzen — SC-007)
    epd.partialUpdate(Display::COUNTDOWN_X, Display::COUNTDOWN_Y,
                      Display::COUNTDOWN_W, Display::COUNTDOWN_H);
}
