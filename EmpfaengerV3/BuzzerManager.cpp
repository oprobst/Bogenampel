/**
 * @file BuzzerManager.cpp
 * @brief Implementierung des Buzzer-Managers (LEDC-PWM)
 */

#include "BuzzerManager.h"

#include <esp_arduino_version.h>

namespace {
    constexpr uint8_t LEDC_RESOLUTION_BITS = 8;  // Duty 0-255

#if ESP_ARDUINO_VERSION_MAJOR < 3
    // arduino-esp32 2.x: LEDC-Kanal muss manuell vergeben werden
    constexpr uint8_t LEDC_CHANNEL_BUZZER = 0;
#endif
}

BuzzerManager::BuzzerManager(uint8_t pin, uint16_t frequency)
    : buzzerPin(pin)
    , buzzerFrequency(frequency)
    , active(false)
    , state(0)
    , beepCount(0)
    , targetBeeps(0)
    , lastToggle(0)
    , volumeDuty(DUTY_MAX)
    , toneOn(false) {
}

void BuzzerManager::begin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(buzzerPin, buzzerFrequency, LEDC_RESOLUTION_BITS);
    ledcWrite(buzzerPin, 0);  // stumm
#else
    ledcSetup(LEDC_CHANNEL_BUZZER, buzzerFrequency, LEDC_RESOLUTION_BITS);
    ledcAttachPin(buzzerPin, LEDC_CHANNEL_BUZZER);
    ledcWrite(LEDC_CHANNEL_BUZZER, 0);  // stumm
#endif
}

void BuzzerManager::setTone(bool on) {
    toneOn = on;
    uint8_t duty = on ? volumeDuty : 0;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(buzzerPin, duty);
#else
    ledcWrite(LEDC_CHANNEL_BUZZER, duty);
#endif
}

void BuzzerManager::setVolume(uint8_t duty) {
    // Begrenzen auf hörbares Minimum bis 50 % Duty (FR-021)
    if (duty < DUTY_MIN) duty = DUTY_MIN;
    if (duty > DUTY_MAX) duty = DUTY_MAX;
    volumeDuty = duty;

    // Lautstärke wirkt live, auch während eines laufenden Tons
    if (toneOn) {
        setTone(true);
    }
}

void BuzzerManager::beep(uint8_t count) {
    if (count == 0) return;

    active = true;
    targetBeeps = count;
    beepCount = 0;
    state = 1;  // Starte mit Beeping
    lastToggle = millis();

    // Ersten Ton sofort starten
    setTone(true);
}

void BuzzerManager::update() {
    if (!active) return;

    uint32_t now = millis();
    uint32_t elapsed = now - lastToggle;

    if (state == 1) {
        // Beeping-Zustand: Nach 500ms in Pause wechseln
        if (elapsed >= BEEP_DURATION_MS) {
            setTone(false);
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
            setTone(true);
            state = 1;
            lastToggle = now;
        }
    }
}

void BuzzerManager::stop() {
    if (active) {
        setTone(false);
        active = false;
        state = 0;
        beepCount = 0;
        targetBeeps = 0;
    }
}
