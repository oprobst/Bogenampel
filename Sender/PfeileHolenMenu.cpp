/**
 * @file PfeileHolenMenu.cpp
 * @brief Implementierung des Pfeile-Holen-Menüs (e-Paper)
 */

#include "PfeileHolenMenu.h"

namespace {
    // Vertikales Layout. Die Überschrift behält Textgröße 2 und rückt statt
    // dessen dicht unter die Statuszeile (deren Trennlinie liegt auf y=23) —
    // der gewonnene Platz geht an die Buttons, damit unten die Abfolge-Anzeige
    // UND die Hilfezeile Platz haben. Vorher überlagerten sich Gruppen-Info
    // (184-191) und Hilfetext (188-195), der untere Text war unlesbar.
    constexpr int16_t HEADING_Y = 30;   // Textgröße 2 → belegt 30..45
    constexpr int16_t HLINE_Y = 50;

    constexpr int16_t BUTTON_FIRST_Y = 56;
    constexpr int16_t BUTTON_STEP = 33;   // 1 px enger als zuvor (34)
    constexpr int16_t BUTTON_H = 28;
    constexpr int16_t BUTTON_MARGIN = 14;

    constexpr int16_t SEQUENCE_Y = 160;

    // Abfolge-Anzeige bei Textgröße 2: 6x8-Font, also 12 px je Zeichen.
    // Ohne die Schrägstriche in den Gruppennamen ("AB" statt "A/B") wird die
    // Zeile so schmal, dass zwischen den beiden Passen wieder ein Balken Platz
    // hat: "AB CD | CD AB" = 13 Zeichen = 156 px.
    constexpr int16_t SEQ_CHAR_W = 12;
    constexpr int16_t SEQ_TOKEN_W = 2 * SEQ_CHAR_W;   // "AB"
    constexpr int16_t SEQ_SPACE_W = SEQ_CHAR_W;       // Halbpassen einer Passe
    constexpr int16_t SEQ_PIPE_W = 3 * SEQ_CHAR_W;    // " | " zwischen den Passen
    constexpr int16_t SEQ_TOTAL_W = 4 * SEQ_TOKEN_W + 2 * SEQ_SPACE_W + SEQ_PIPE_W;

    static_assert(SEQ_TOTAL_W <= EpaperDisplay::WIDTH,
                  "Sequence line does not fit the panel width");
    static_assert(BUTTON_FIRST_Y + 2 * BUTTON_STEP + BUTTON_H < SEQUENCE_Y,
                  "Third option button would overlap the sequence line");
}

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

    // CONFIG: Cursor weiter mit Wrap-around
    if (buttons.wasClicked(Button::CONFIG)) {
        cursorPosition = (cursorPosition + 1) % numButtons;
        needsUpdate = true;
    }
    // OK: Aktion auswählen
    else if (buttons.wasClicked(Button::OK)) {
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
    epd.printCentered("Pfeile holen", HEADING_Y, 2);
    g.drawFastHLine(10, HLINE_Y, EpaperDisplay::WIDTH - 20, GxEPD_BLACK);

    // Options-Buttons
    drawOptionButton("Naechste Passe", BUTTON_FIRST_Y, cursorPosition == 0);
    if (shooterCount == 4) {
        drawOptionButton("Abfolge", BUTTON_FIRST_Y + BUTTON_STEP, cursorPosition == 1);
        drawOptionButton("Neustart", BUTTON_FIRST_Y + 2 * BUTTON_STEP, cursorPosition == 2);
    } else {
        drawOptionButton("Neustart", BUTTON_FIRST_Y + BUTTON_STEP, cursorPosition == 1);
    }

    // Gruppen-Info (nur 3-4 Schützen)
    drawShooterGroupInfo();

    // Hilfetext unten
    g.setTextSize(1);
    g.setCursor(10, EpaperDisplay::HEIGHT - 12);
    g.print("Stift: waehlen   OK: ausfuehren");

    needsUpdate = false;
}

void PfeileHolenMenu::drawOptionButton(const char* label, int16_t y, bool selected) {
    Adafruit_GFX& g = epd.gfx();
    const int16_t btnW = EpaperDisplay::WIDTH - 2 * BUTTON_MARGIN;

    if (selected) {
        // Ausgewählt: invertiert (schwarzer Button, weiße Schrift)
        g.fillRect(BUTTON_MARGIN, y, btnW, BUTTON_H, GxEPD_BLACK);
        g.setTextColor(GxEPD_WHITE);
    } else {
        g.drawRect(BUTTON_MARGIN, y, btnW, BUTTON_H, GxEPD_BLACK);
        g.setTextColor(GxEPD_BLACK);
    }
    epd.printCentered(label, y + 7, 2);
    g.setTextColor(GxEPD_BLACK);
}

void PfeileHolenMenu::drawShooterGroupInfo() {
    // Nur bei 4 Schützen gibt es Gruppen
    if (shooterCount != 4) return;

    Adafruit_GFX& g = epd.gfx();

    // Die vier Halbpassen in genau der Reihenfolge, die advanceToNextGroup()
    // durchläuft: AB(1.) -> CD(2.) -> CD(1.) -> AB(2.). Dargestellt als
    //   AB CD | CD AB
    // Die beiden Halbpassen einer Passe stehen nebeneinander, der Balken trennt
    // die Passen. Schwarz hinterlegt ist die Gruppe, die als nächstes schießt —
    // der Bediener sieht damit auf einen Blick, wo im Durchgang er steht, statt
    // es aus "ganze/halbe Passe" zu erschließen.
    static const char* const SLOT[4] = { "AB", "CD", "CD", "AB" };
    static const char* const SEP[3] = { nullptr, "|", nullptr };  // nullptr = nur Lücke
    static const int16_t SEP_W[3] = { SEQ_SPACE_W, SEQ_PIPE_W, SEQ_SPACE_W };

    uint8_t activeSlot;
    if (currentGroup == Groups::Type::GROUP_AB) {
        activeSlot = (currentPosition == Groups::Position::POS_1) ? 0 : 3;
    } else {
        activeSlot = (currentPosition == Groups::Position::POS_2) ? 1 : 2;
    }

    int16_t x = (EpaperDisplay::WIDTH - SEQ_TOTAL_W) / 2;
    g.setTextSize(2);

    for (uint8_t i = 0; i < 4; i++) {
        if (i == activeSlot) {
            g.fillRect(x - 1, SEQUENCE_Y - 2, SEQ_TOKEN_W + 2, 20, GxEPD_BLACK);
            g.setTextColor(GxEPD_WHITE);
        } else {
            g.setTextColor(GxEPD_BLACK);
        }
        g.setCursor(x, SEQUENCE_Y);
        g.print(SLOT[i]);
        x += SEQ_TOKEN_W;

        if (i < 3) {
            if (SEP[i] != nullptr) {
                // Trennzeichen in seiner Spalte zentrieren, damit links und
                // rechts davon gleich viel Luft steht
                g.setTextColor(GxEPD_BLACK);
                g.setCursor(x + (SEP_W[i] - SEQ_CHAR_W) / 2, SEQUENCE_Y);
                g.print(SEP[i]);
            }
            x += SEP_W[i];
        }
    }

    g.setTextColor(GxEPD_BLACK);
}
