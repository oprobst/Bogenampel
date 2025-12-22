/**
 * @file BuzzerManager.cpp
 * @brief Implementierung des Buzzer-Managers
 */

#include "BuzzerManager.h"

BuzzerManager::BuzzerManager(uint8_t pin, uint16_t frequency)
    : buzzerPin(pin)
    , buzzerFrequency(frequency)
    , active(false)
    , state(0)
    , beepCount(0)
    , targetBeeps(0)
    , lastToggle(0) {
}

void BuzzerManager::begin() {
    pinMode(buzzerPin, OUTPUT);
    digitalWrite(buzzerPin, LOW);
}

void BuzzerManager::beep(uint8_t count) {
    if (count == 0) return;

    active = true;
    targetBeeps = count;
    beepCount = 0;
    state = 1;  // Starte mit Beeping
    lastToggle = millis();

    // Ersten Ton sofort starten (KY-012: Aktiver Buzzer)
    digitalWrite(buzzerPin, HIGH);
}

void BuzzerManager::update() {
    if (!active) return;

    uint32_t now = millis();
    uint32_t elapsed = now - lastToggle;

    if (state == 1) {
        // Beeping-Zustand: Nach 500ms in Pause wechseln
        if (elapsed >= BEEP_DURATION_MS) {
            // KY-012: Aktiver Buzzer - einfach LOW
            digitalWrite(buzzerPin, LOW);
            beepCount++;

            // Alle Pieptöne fertig?
            if (beepCount >= targetBeeps) {
                active = false;
                return;
            }

            // In Pause-Zustand wechseln
            state = 0;
            lastToggle = now;
        }
    } else {
        // Pause-Zustand: Nach 500ms nächsten Ton starten
        if (elapsed >= PAUSE_DURATION_MS) {
            // KY-012: Aktiver Buzzer - einfach HIGH
            digitalWrite(buzzerPin, HIGH);
            state = 1;
            lastToggle = now;
        }
    }
}

void BuzzerManager::stop() {
    if (active) {
        // KY-012: Aktiver Buzzer - einfach LOW
        digitalWrite(buzzerPin, LOW);
        active = false;
        state = 0;
        beepCount = 0;
        targetBeeps = 0;
    }
}
