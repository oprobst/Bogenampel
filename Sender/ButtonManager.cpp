/**
 * @file ButtonManager.cpp
 * @brief Button Manager Implementierung
 */

#include "ButtonManager.h"

ButtonManager::ButtonManager()
    : okPressStartTime(0), okPressActive(false), alarmTriggered(false) {
    // Alle Button-States initialisieren
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        buttons[i].pressed = false;
        buttons[i].lastRawState = true;  // HIGH = nicht gedrückt (Pull-Up)
        buttons[i].lastChangeTime = 0;
        buttons[i].pressTime = 0;
        buttons[i].wasPressedFlag = false;
        buttons[i].wasReleasedFlag = false;
    }
}

void ButtonManager::begin() {
    // Pins als Eingänge mit Pull-Up konfigurieren
    pinMode(Pins::BTN_LEFT, INPUT_PULLUP);
    pinMode(Pins::BTN_OK, INPUT_PULLUP);
    pinMode(Pins::BTN_RIGHT, INPUT_PULLUP);

    // Initiale Zustände lesen
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        Button btn = static_cast<Button>(i);
        buttons[i].lastRawState = readRawState(btn);
    }
}

void ButtonManager::update() {
    uint32_t now = millis();

    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        Button btn = static_cast<Button>(i);
        ButtonState& state = buttons[i];

        // Aktuellen rohen Zustand lesen (LOW = gedrückt)
        bool rawPressed = readRawState(btn);

        // Debouncing: Hat sich der rohe Zustand geändert?
        if (rawPressed != state.lastRawState) {
            state.lastRawState = rawPressed;
            state.lastChangeTime = now;
        }

        // Prüfe ob genug Zeit vergangen ist (Debounce)
        if ((now - state.lastChangeTime) >= Timing::DEBOUNCE_MS) {
            // Zustand ist stabil, prüfe ob sich der entprellte Zustand ändert

            // Button wurde gedrückt (Flanke HIGH → LOW)
            if (rawPressed && !state.pressed) {
                state.pressed = true;
                state.pressTime = now;
                state.wasPressedFlag = true;               
            }
            // Button wurde losgelassen (Flanke LOW → HIGH)
            else if (!rawPressed && state.pressed) {
                state.pressed = false;
                state.wasReleasedFlag = true;

                #if DEBUG_ENABLED
                DEBUG_PRINT(i);
                DEBUG_PRINT(F(" ("));
                DEBUG_PRINT(now - state.pressTime);
                DEBUG_PRINTLN(F("ms)"));
                #endif
            }
        }
    }

    // Alarm-Detektion: OK-Button > 3 Sekunden gehalten
    bool okPressed = buttons[static_cast<uint8_t>(Button::OK)].pressed;

    if (okPressed && !okPressActive) {
        // OK-Button wurde gerade gedrückt
        okPressStartTime = now;
        okPressActive = true;
        alarmTriggered = false;
    }
    else if (okPressed && okPressActive) {
        // OK-Button wird gehalten - prüfe Dauer
        uint32_t duration = now - okPressStartTime;
        if (duration >= Timing::ALARM_THRESHOLD_MS && !alarmTriggered) {
            alarmTriggered = true;  // Alarm-Flag setzen (wird mit isAlarmTriggered() abgerufen)
            DEBUG_PRINTLN(F("ALARM"));
        }
    }
    else if (!okPressed && okPressActive) {
        // OK-Button wurde losgelassen
        okPressActive = false;
    }
}

bool ButtonManager::isPressed(Button btn) const {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;
    return buttons[idx].pressed;
}

bool ButtonManager::wasPressed(Button btn) {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;

    // Read-once: Flag wird beim Lesen gelöscht
    if (buttons[idx].wasPressedFlag) {
        buttons[idx].wasPressedFlag = false;
        return true;
    }
    return false;
}

bool ButtonManager::wasReleased(Button btn) {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;

    // Read-once: Flag wird beim Lesen gelöscht
    if (buttons[idx].wasReleasedFlag) {
        buttons[idx].wasReleasedFlag = false;
        return true;
    }
    return false;
}

bool ButtonManager::isLongPress(Button btn, uint32_t duration) const {
    uint8_t idx = static_cast<uint8_t>(btn);
    if (idx >= static_cast<uint8_t>(Button::COUNT)) return false;

    const ButtonState& state = buttons[idx];
    if (!state.pressed) return false;

    return (millis() - state.pressTime) >= duration;
}

bool ButtonManager::isAnyPressed() const {
    for (uint8_t i = 0; i < static_cast<uint8_t>(Button::COUNT); i++) {
        if (buttons[i].pressed) return true;
    }
    return false;
}

bool ButtonManager::isAlarmTriggered() {
    // Read-once: Flag wird beim Lesen gelöscht
    if (alarmTriggered) {
        alarmTriggered = false;
        return true;
    }
    return false;
}

//=============================================================================
// Private Hilfsfunktionen
//=============================================================================

uint8_t ButtonManager::getPin(Button btn) const {
    switch (btn) {
        case Button::LEFT:  return Pins::BTN_LEFT;
        case Button::OK:    return Pins::BTN_OK;
        case Button::RIGHT: return Pins::BTN_RIGHT;
        default: return 0;
    }
}

bool ButtonManager::readRawState(Button btn) const {
    // LOW = gedrückt (Pull-Up aktiv), also invertieren
    return digitalRead(getPin(btn)) == LOW;
}
