/**
 * @file ButtonManager.cpp
 * @brief Button Manager Implementierung (2 Taster, V3)
 */

#include "ButtonManager.h"

ButtonManager::ButtonManager()
    : lastActivity(0) {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        buttons[i].pressed = false;
        buttons[i].lastChangeTime = 0;
        buttons[i].pressTime = 0;
        buttons[i].clickedFlag = false;
        buttons[i].reportedHoldMs = 0;
        buttons[i].bootLockout = false;
        buttons[i].clickCount = 0;
        buttons[i].lastPressTime = 0;
        buttons[i].multiClickFlag = false;
        buttons[i].multiClickArmed = false;
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
        buttons[i].pressed = readRawState(btn);
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

        // Entprellung auf der VORDERFLANKE: die erste Änderung zählt sofort,
        // danach ist der Taster für DEBOUNCE_MS gesperrt.
        //
        // Vorher wurde umgekehrt gewartet, bis der rohe Pegel 80 ms stabil war,
        // BEVOR die Flanke zählte. Damit war jede Betätigung unter 80 ms
        // komplett unsichtbar — für langsame Menübedienung nie aufgefallen, aber
        // tödlich für den Dreifachklick der Alarm-Geste: unter Stress liegt die
        // Kontaktzeit bei 50-80 ms. Prellen dauert < 10 ms und wird von der
        // Sperre weiterhin sicher geschluckt.
        if (rawPressed != state.pressed
            && (now - state.lastChangeTime) >= Timing::DEBOUNCE_MS) {
            state.lastChangeTime = now;
            // Button wurde gedrückt (Flanke)
            if (rawPressed && !state.pressed) {
                state.pressed = true;
                state.pressTime = now;
                state.reportedHoldMs = 0;
                lastActivity = now;

                if (!state.bootLockout) {
                    // Mehrfachklick zählen — NUR auf der Rolle OK (FR-009).
                    // CONFIG blättert durch Menüs und zählt Werte hoch; dort
                    // wäre zügiges Tippen von einer Alarmfolge nicht zu
                    // unterscheiden. Der Einschalt-/Modus-Druck zählt ebenfalls
                    // nicht mit (Boot-Lockout), sonst wären zwei Klicks nach dem
                    // Booten schon eine halbe Folge.
                    if (btn == Button::OK) {
                        if (state.clickCount > 0
                            && (now - state.lastPressTime) <= Timing::MULTI_CLICK_GAP_MS) {
                            state.clickCount++;
                        } else {
                            state.clickCount = 1;
                        }
                        state.lastPressTime = now;

                        // Diese Betätigung WÜRDE die Folge vervollständigen.
                        // Entschieden wird erst beim Loslassen: wer die Taste
                        // jetzt liegen lässt, will ausschalten (FR-011).
                        state.multiClickArmed =
                            (state.clickCount >= Timing::MULTI_CLICK_COUNT);

                        DEBUG_PRINTF("BTN OK: Klick #%u%s\n",
                                     (unsigned)state.clickCount,
                                     state.multiClickArmed ? " (Folge scharf)" : "");
                    } else {
                        // CONFIG hat weder Halte- noch Klickfolgen-Geste →
                        // Klick sofort beim Drücken
                        state.clickedFlag = true;
                    }
                }
            }
            // Button wurde losgelassen (Flanke)
            else if (!rawPressed && state.pressed) {
                state.pressed = false;
                lastActivity = now;

                if (state.bootLockout) {
                    // Einschalt-/Modus-Druck endet hier — ab jetzt zählt er
                    state.bootLockout = false;
                } else {
                    const uint32_t heldMs = now - state.pressTime;
                    // Länger als das Klickfenster = kein Klick mehr, sondern der
                    // Beginn eines Haltens — bricht die Folge ab (FR-011)
                    const bool wasHold = (heldMs >= Timing::MULTI_CLICK_GAP_MS);

                    if (state.multiClickArmed && !wasHold) {
                        // Folge vollständig → Alarm melden. Der auslösende Klick
                        // verfällt, sonst löste er im Zielzustand sofort die
                        // nächste Aktion aus (im Alarm: quittieren).
                        state.multiClickFlag = true;
                        state.clickCount = 0;
                        // Klick 1 und 2 der Folge verwerfen: liegen sie noch
                        // unabgeholt herum (Bildaufbau blockiert), quittieren
                        // sie den Alarm sofort wieder.
                        discardPendingClicks();
                        DEBUG_PRINTLN("BTN: Mehrfachklick erkannt -> Alarm-Geste");
                    } else {
                        if (wasHold) state.clickCount = 0;

                        // OK klickt beim Loslassen — kürzer als die
                        // Ausschalt-Schwelle gilt als Klick, damit auch eine
                        // abgebrochene Halte-Geste wirkt (Contract G-8).
                        // CONFIG hat beim Drücken bereits geklickt.
                        if (btn == Button::OK && heldMs < Timing::POWER_OFF_HOLD_MS) {
                            state.clickedFlag = true;
                        }
                    }
                    state.multiClickArmed = false;
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

bool ButtonManager::wasMultiClicked(Button btn) {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;

    // Read-once: Flag wird beim Lesen gelöscht
    if (buttons[idx].multiClickFlag) {
        buttons[idx].multiClickFlag = false;
        return true;
    }
    return false;
}

void ButtonManager::resetMultiClick() {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        buttons[i].clickCount = 0;
        buttons[i].lastPressTime = 0;
        buttons[i].multiClickFlag = false;
        // Auch die scharfe Betätigung entschärfen, sonst meldete ihr Loslassen
        // noch einen Alarm, obwohl der Zustandswechsel die Folge gerade
        // verworfen hat. Ein dabei zurückgehaltener CONFIG-Klick verfällt —
        // er stammt aus dem alten Zustand und wäre ohnehin veraltet.
        buttons[i].multiClickArmed = false;
    }
}

void ButtonManager::discardPendingClicks() {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        buttons[i].clickedFlag = false;
    }
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
