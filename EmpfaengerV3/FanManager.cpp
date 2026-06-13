/**
 * @file FanManager.cpp
 * @brief Implementierung der Lüfter-Steuerung
 */

#include "FanManager.h"
#include "Config.h"

#include <esp_arduino_version.h>

namespace {
#if ESP_ARDUINO_VERSION_MAJOR < 3
    // arduino-esp32 2.x: LEDC-Kanal muss manuell vergeben werden
    // (Kanal 0 nutzt der Buzzer in BuzzerManager.cpp)
    constexpr uint8_t LEDC_CHANNEL_FAN = 2;
#endif
}

FanManager::FanManager(uint8_t pwm, uint8_t poti)
    : pwmPin(pwm)
    , potiPin(poti)
    , duty(0)
    , lastUpdateMs(0) {
}

void FanManager::begin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(pwmPin, Fan::PWM_FREQUENCY_HZ, Fan::PWM_RESOLUTION_BITS);
#else
    ledcSetup(LEDC_CHANNEL_FAN, Fan::PWM_FREQUENCY_HZ, Fan::PWM_RESOLUTION_BITS);
    ledcAttachPin(pwmPin, LEDC_CHANNEL_FAN);
#endif

    // Sofort definierten Duty vom Poti setzen (Befund 6: Gate-Pull-up R5
    // hielt die PWM-Leitung bis hierhin LOW → Lüfter auf Minimaldrehzahl)
    applyPotiValue();
    lastUpdateMs = millis();
}

void FanManager::update() {
    if (millis() - lastUpdateMs < Timing::POTI_UPDATE_INTERVAL_MS) return;
    lastUpdateMs = millis();
    applyPotiValue();
}

void FanManager::applyPotiValue() {
    // Poti verpolt → invertiert auf den Gate-Duty (Q2 Open-Drain: Gate HIGH zieht
    // die PWM-Leitung LOW = Lüfter langsam). Poti am ADC-Minimum (0) = volle
    // Drehzahl (Gate-Duty 0 → Leitung HIGH). Ab Fan::OFF_THRESHOLD (nahe Maximum)
    // Lüfter aus (Gate-Duty 255 → Leitung dauerhaft LOW; bei 4-Draht-Lüftern nur
    // Minimaldrehzahl, da PWM 0 % nicht vollständig stoppt).
    uint16_t raw = analogRead(potiPin);
    uint16_t clamped = (raw > Fan::OFF_THRESHOLD) ? Fan::OFF_THRESHOLD : raw;
    uint8_t newDuty = (uint8_t)map(clamped, 0, Fan::OFF_THRESHOLD, 0, 255);

    // Kleine Hysterese gegen ADC-Rauschen
    if (newDuty != duty && (newDuty > duty ? newDuty - duty : duty - newDuty) >= 2) {
        duty = newDuty;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
        ledcWrite(pwmPin, duty);
#else
        ledcWrite(LEDC_CHANNEL_FAN, duty);
#endif
    }
}
