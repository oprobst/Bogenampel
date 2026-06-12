/**
 * @file PfeileHolenMenu.cpp
 * @brief Implementierung des Pfeile-Holen-Menüs (e-Paper)
 */

#include "PfeileHolenMenu.h"

PfeileHolenMenu::PfeileHolenMenu(EpaperDisplay& epdRef, ButtonManager& btnMgr)
    : epd(epdRef)
    , buttons(btnMgr)
    , cursorPosition(0)
    , selectedAction(PfeileHolenAction::NONE)
    , needsUpdate(true)
    , buttonLockoutUntil(0)
    , shooterCount(2)
    , currentGroup(Groups::Type::GROUP_AB)
    , currentPosition(Groups::Position::POS_1) {
}

void PfeileHolenMenu::begin() {
    cursorPosition = 0;  // Start bei "Nächste Passe"
    selectedAction = PfeileHolenAction::NONE;
    needsUpdate = true;
    // Kurze Eingabesperre: verhindert, dass ein nachprallendes OK sofort
    // "Nächste Passe" auslöst, wenn die Passe gerade erst beendet wurde.
    buttonLockoutUntil = millis() + Timing::MENU_LOCKOUT_MS;
}

void PfeileHolenMenu::setTournamentConfig(uint8_t shooters, Groups::Type group,
                                          Groups::Position position) {
    bool changed = (currentGroup != group) || (currentPosition != position)
                || (shooterCount != shooters);

    shooterCount = shooters;
    currentGroup = group;
    currentPosition = position;

    if (changed) {
        needsUpdate = true;
    }
}

void PfeileHolenMenu::update() {
    // Eingabesperre nach Zustandswechsel (Tastenprellen, V2-Verhalten)
    if (millis() < buttonLockoutUntil) return;

    // Anzahl sichtbarer Buttons:
    // 1-2 Schützen: 2 (Nächste Passe, Neustart); 3-4: 3 (+ Abfolge)
    uint8_t numButtons = (shooterCount == 4) ? 3 : 2;

    // BTN2: Cursor weiter mit Wrap-around
    if (buttons.wasClicked(Button::BTN2)) {
        cursorPosition = (cursorPosition + 1) % numButtons;
        needsUpdate = true;
    }
    // BTN1: Aktion auswählen
    else if (buttons.wasClicked(Button::BTN1)) {
        // Bei 1-2 Schützen: Position 1 = Neustart (Abfolge entfällt)
        if (shooterCount == 2 && cursorPosition == 1) {
            selectedAction = PfeileHolenAction::NEUSTART;
        } else {
            selectedAction = static_cast<PfeileHolenAction>(cursorPosition);
        }
    }
}

void PfeileHolenMenu::draw() {
    Adafruit_GFX& g = epd.gfx();

    // Inhalt unterhalb der Statuszeile löschen
    g.fillRect(0, Display::STATUS_H, EpaperDisplay::WIDTH,
               EpaperDisplay::HEIGHT - Display::STATUS_H, GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    // Überschrift
    epd.printCentered("Pfeile holen", 30, 2);
    g.drawFastHLine(10, 50, EpaperDisplay::WIDTH - 20, GxEPD_BLACK);

    // Options-Buttons
    const int16_t firstY = 58;
    const int16_t step = 34;
    drawOptionButton("Naechste Passe", firstY, cursorPosition == 0);
    if (shooterCount == 4) {
        drawOptionButton("Abfolge", firstY + step, cursorPosition == 1);
        drawOptionButton("Neustart", firstY + 2 * step, cursorPosition == 2);
    } else {
        drawOptionButton("Neustart", firstY + step, cursorPosition == 1);
    }

    // Gruppen-Info (nur 3-4 Schützen)
    drawShooterGroupInfo();

    // Hilfetext unten
    g.setTextSize(1);
    g.setCursor(10, EpaperDisplay::HEIGHT - 12);
    g.print("Pfeil: waehlen   OK: ausfuehren");

    needsUpdate = false;
}

void PfeileHolenMenu::drawOptionButton(const char* label, int16_t y, bool selected) {
    Adafruit_GFX& g = epd.gfx();
    const int16_t margin = 14;
    const int16_t btnW = EpaperDisplay::WIDTH - 2 * margin;
    const int16_t btnH = 28;

    if (selected) {
        // Ausgewählt: invertiert (schwarzer Button, weiße Schrift)
        g.fillRect(margin, y, btnW, btnH, GxEPD_BLACK);
        g.setTextColor(GxEPD_WHITE);
    } else {
        g.drawRect(margin, y, btnW, btnH, GxEPD_BLACK);
        g.setTextColor(GxEPD_BLACK);
    }
    epd.printCentered(label, y + 7, 2);
    g.setTextColor(GxEPD_BLACK);
}

void PfeileHolenMenu::drawShooterGroupInfo() {
    // Nur bei 3-4 Schützen anzeigen
    if (shooterCount != 4) return;

    Adafruit_GFX& g = epd.gfx();
    const int16_t infoY = 164;

    // "Naechste: A/B" — die Gruppe, die als nächstes schießt
    g.setTextSize(2);
    g.setCursor(14, infoY);
    g.print("Naechste: ");
    const char* groupText = (currentGroup == Groups::Type::GROUP_AB) ? "A/B" : "C/D";
    // Gruppe invertiert hervorheben
    int16_t gx = g.getCursorX();
    g.fillRect(gx - 2, infoY - 2, 40, 18, GxEPD_BLACK);
    g.setTextColor(GxEPD_WHITE);
    g.setCursor(gx, infoY);
    g.print(groupText);
    g.setTextColor(GxEPD_BLACK);

    // Positions-Hinweis (1. = ganze Passe, 2. = halbe Passe)
    g.setTextSize(1);
    g.setCursor(14, infoY + 20);
    g.print(currentPosition == Groups::Position::POS_1
            ? "ganze Passe (beide Gruppen)"
            : "halbe Passe (nur diese Gruppe)");
}
