/**
 * @file ButtonManager.cpp
 * @brief Button Manager Implementierung (2 Taster, V3)
 */

#include "ButtonManager.h"

ButtonManager::ButtonManager()
    : lastActivity(0) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        buttons[i].pressed = false;
        buttons[i].lastRawState = false;
        buttons[i].lastChangeTime = 0;
        buttons[i].pressTime = 0;
        buttons[i].clickedFlag = false;
        buttons[i].reportedHoldMs = 0;
        buttons[i].bootLockout = false;
    }
}

void ButtonManager::begin() {
    // SW2/GPIO15 (Rolle CONFIG): aktiv HIGH über externen Teiler R2/R7 —
    // KEINE internen Pulls
    pinMode(Pins::BTN1, INPUT);
    // SW1/GPIO9 (Rolle OK): gegen GND, kein externer Pullup → interner Pullup
    pinMode(Pins::BTN2, INPUT_PULLUP);

    // Initiale Zustände lesen. Wer jetzt schon gedrückt ist, steht im
    // Boot-Lockout: CONFIG hält beim Einschalten den Latch, und im
    // Wartungsmodus werden beide Taster gehalten — ohne Lockout liefe dort
    // nach 3 s die Power-Off-Geste von OK los.
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        Button btn = static_cast<Button>(i);
        buttons[i].lastRawState = readRawState(btn);
        buttons[i].pressed = buttons[i].lastRawState;
        buttons[i].bootLockout = buttons[i].pressed;
    }
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
                lastActivity = now;

                // CONFIG hat keine Halte-Geste → Klick sofort beim Drücken
                if (btn == Button::CONFIG && !state.bootLockout) {
                    state.clickedFlag = true;
                }
            }
            // Button wurde losgelassen (Flanke)
            else if (!rawPressed && state.pressed) {
                state.pressed = false;
                lastActivity = now;

                if (state.bootLockout) {
                    // Einschalt-/Modus-Druck endet hier — ab jetzt zählt er
                    state.bootLockout = false;
                } else if (btn == Button::OK
                           && (now - state.pressTime) < Timing::ALARM_THRESHOLD_MS) {
                    // Kurz gehalten → Klick (lange Drücke sind Gesten)
                    state.clickedFlag = true;
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

    // Boot-Lockout: Einschalt-/Modus-Druck darf keine Geste auslösen
    if (state.bootLockout) return false;

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
    if (btn == Button::CONFIG) {
        // SW2/GPIO15, aktiv HIGH (Teiler R2/R7 an +BATT, ~2,5-3,5 V gedrückt).
        // Dieser Taster hält beim Einschalten den Power-Latch — das ist
        // Hardware und per Firmware nicht auf den anderen Taster verlegbar.
        return digitalRead(Pins::BTN1) == HIGH;
    }
    // Rolle OK → SW1/GPIO9, aktiv LOW (interner Pullup)
    return digitalRead(Pins::BTN2) == LOW;
}

bool ButtonManager::isBootLockoutActive(Button btn) const {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;
    return buttons[idx].bootLockout;
}
