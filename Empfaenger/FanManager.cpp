/**
 * @file FanManager.cpp
 * @brief Implementierung der Lüfter-Steuerung
 */

#include "FanManager.h"
#include "Config.h"

#include <esp_arduino_version.h>
#include <math.h>

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
    // Poti verpolt → invertiert (Q2 Open-Drain: Gate HIGH zieht die PWM-Leitung
    // LOW = Lüfter langsam). ADC-Minimum = volle Drehzahl; ab Fan::OFF_THRESHOLD
    // aus. Gamma-Kennlinie (Fan::CURVE_GAMMA < 1) spreizt die gefühlte Drehzahl-
    // änderung über den Drehweg — sonst beschleunigt der Lüfter fast nur im
    // obersten PWM-Bereich.
    uint16_t raw = Adc::readAveraged(potiPin);  // gemittelt (Rauschunterdrückung)
    uint16_t clamped = (raw > Fan::OFF_THRESHOLD) ? Fan::OFF_THRESHOLD : raw;
    // q = Anteil Richtung volle Drehzahl (1 = ADC-Min/voll, 0 = Aus-Anschlag)
    float q = (float)(Fan::OFF_THRESHOLD - clamped) / (float)Fan::OFF_THRESHOLD;
    uint8_t fanDuty = (uint8_t)(powf(q, Fan::CURVE_GAMMA) * 255.0f + 0.5f);  // 255 = voll
    uint8_t newDuty = (uint8_t)(255 - fanDuty);  // Gate-Duty (invertiert: 0 = voll)

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
