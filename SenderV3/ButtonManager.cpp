/**
 * @file ButtonManager.cpp
 * @brief Button Manager Implementierung (2 Taster, V3)
 */

#include "ButtonManager.h"

ButtonManager::ButtonManager()
    : bootLockout(false) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        buttons[i].pressed = false;
        buttons[i].lastRawState = false;
        buttons[i].lastChangeTime = 0;
        buttons[i].pressTime = 0;
        buttons[i].clickedFlag = false;
        buttons[i].reportedHoldMs = 0;
    }
}

void ButtonManager::begin() {
    // BTN1: aktiv HIGH über externen Teiler R2/R7 — KEINE internen Pulls
    pinMode(Pins::BTN1, INPUT);
    // BTN2: gegen GND, kein externer Pullup → interner Pullup, aktiv LOW
    pinMode(Pins::BTN2, INPUT_PULLUP);

    // Initiale Zustände lesen
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        Button btn = static_cast<Button>(i);
        buttons[i].lastRawState = readRawState(btn);
        buttons[i].pressed = buttons[i].lastRawState;
    }

    // Boot-Lockout: BTN1 ist der Einschalt-Taster — solange er seit dem
    // Einschalt-Druck nicht losgelassen wurde, sind alle BTN1-Gesten gesperrt
    bootLockout = buttons[static_cast<uint8_t>(Button::BTN1)].pressed;
}

void ButtonManager::update() {
    uint32_t now = millis();

    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        Button btn = static_cast<Button>(i);
        ButtonState& state = buttons[i];

        // Aktuellen rohen Zustand lesen (true = gedrückt)
        bool rawPressed = readRawState(btn);

        // Debouncing: Hat sich der rohe Zustand geändert?
        if (rawPressed != state.lastRawState) {
            state.lastRawState = rawPressed;
            state.lastChangeTime = now;
        }

        // Prüfe ob genug Zeit vergangen ist (Debounce)
        if ((now - state.lastChangeTime) >= Timing::DEBOUNCE_MS) {
            // Button wurde gedrückt (Flanke)
            if (rawPressed && !state.pressed) {
                state.pressed = true;
                state.pressTime = now;
                state.reportedHoldMs = 0;

                // BTN2 hat keine Halte-Geste → Klick sofort beim Drücken
                if (btn == Button::BTN2) {
                    state.clickedFlag = true;
                }
            }
            // Button wurde losgelassen (Flanke)
            else if (!rawPressed && state.pressed) {
                state.pressed = false;

                if (btn == Button::BTN1) {
                    if (bootLockout) {
                        // Einschalt-Druck endet hier — ab jetzt Gesten erlaubt
                        bootLockout = false;
                    } else if ((now - state.pressTime) < Timing::ALARM_THRESHOLD_MS) {
                        // Kurz gehalten → Klick (lange Drücke sind Gesten)
                        state.clickedFlag = true;
                    }
                }
            }
        }
    }
}

bool ButtonManager::isPressed(Button btn) const {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;
    return buttons[idx].pressed;
}

bool ButtonManager::wasClicked(Button btn) {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;

    // Read-once: Flag wird beim Lesen gelöscht
    if (buttons[idx].clickedFlag) {
        buttons[idx].clickedFlag = false;
        return true;
    }
    return false;
}

bool ButtonManager::wasHeldFor(Button btn, uint16_t ms) {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;

    ButtonState& state = buttons[idx];
    if (!state.pressed) return false;

    // Boot-Lockout: Einschalt-Druck darf keine Geste auslösen
    if (btn == Button::BTN1 && bootLockout) return false;

    uint32_t duration = millis() - state.pressTime;
    if (duration >= ms && state.reportedHoldMs < ms) {
        state.reportedHoldMs = ms;  // one-shot pro Schwelle
        return true;
    }
    return false;
}

bool ButtonManager::isAnyPressed() const {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        if (buttons[i].pressed) return true;
    }
    return false;
}

//=============================================================================
// Private Hilfsfunktionen
//=============================================================================

bool ButtonManager::readRawState(Button btn) const {
    if (btn == Button::BTN1) {
        // Aktiv HIGH (Teiler R2/R7 an +BATT, ~2,5-3,5 V bei gedrücktem Taster)
        return digitalRead(Pins::BTN1) == HIGH;
    }
    // BTN2: aktiv LOW (interner Pullup)
    return digitalRead(Pins::BTN2) == LOW;
}
