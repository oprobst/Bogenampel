/**
 * @file EncoderManager.cpp
 * @brief Implementierung des KY-040-Drehgebers
 */

#include "EncoderManager.h"

volatile int32_t EncoderManager::steps = 0;
volatile uint8_t EncoderManager::lastAB = 0;

namespace {
    // Index = (alter Zustand << 2) | neuer Zustand, Zustand = (A << 1) | B.
    // ±1 für gültige Nachbarschritte, 0 für Stillstand und ungültige Sprünge.
    const int8_t TRANSITION[16] = {
         0, -1, +1,  0,
        +1,  0,  0, -1,
        -1,  0,  0, +1,
         0, +1, -1,  0
    };
}

void IRAM_ATTR EncoderManager::onEdge() {
    uint8_t ab = (digitalRead(Pins::ENC_CLK) << 1) | digitalRead(Pins::ENC_DT);
    steps = steps + TRANSITION[(lastAB << 2) | ab];
    lastAB = ab;
}

void EncoderManager::begin() {
    // CLK/DT: Pull-ups sitzen auf dem KY-040 (an dessen +, also an 3V3)
    pinMode(Pins::ENC_CLK, INPUT);
    pinMode(Pins::ENC_DT, INPUT);
    pinMode(Pins::ENC_SW, INPUT_PULLUP);

    lastAB = (digitalRead(Pins::ENC_CLK) << 1) | digitalRead(Pins::ENC_DT);
    attachInterrupt(digitalPinToInterrupt(Pins::ENC_CLK), onEdge, CHANGE);
    attachInterrupt(digitalPinToInterrupt(Pins::ENC_DT), onEdge, CHANGE);
}

int16_t EncoderManager::takeDetents() {
    noInterrupts();
    int32_t s = steps;
    steps = 0;
    interrupts();

    stepRemainder += s;
    int16_t detents = stepRemainder / Ui::ENC_STEPS_PER_DETENT;
    stepRemainder -= detents * Ui::ENC_STEPS_PER_DETENT;
    return detents;
}

void EncoderManager::update() {
    uint32_t now = millis();
    bool raw = digitalRead(Pins::ENC_SW) == LOW;

    if (raw != rawPressed) {
        rawPressed = raw;
        rawChangedMs = now;
    }

    if (raw != pressed && now - rawChangedMs >= Ui::BUTTON_DEBOUNCE_MS) {
        pressed = raw;
        if (pressed) {
            pressedSinceMs = now;
            longFired = false;
        } else if (!longFired) {
            pending = Event::SHORT_PRESS;
        }
    }

    if (pressed && !longFired && now - pressedSinceMs >= Ui::LONG_PRESS_MS) {
        longFired = true;
        pending = Event::LONG_PRESS;
    }
}

EncoderManager::Event EncoderManager::takeEvent() {
    Event e = pending;
    pending = Event::NONE;
    return e;
}
