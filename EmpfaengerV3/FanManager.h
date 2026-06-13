/**
 * @file FanManager.h
 * @brief Lüfter-Steuerung (LEDC-PWM, Drehzahl vom Poti — FR-023, R-10)
 *
 * NEU in V3: Der Gehäuselüfter hängt über einen 2N7002 (Q2, Low-Side) an D6;
 * die Drehzahl folgt dem Lüfter-Poti an D2 (GPIO4, ADC1_CH4).
 * - PWM-Frequenz 1 kHz: 25 kHz kam über den Open-Drain-Treiber am 4-Draht-
 *   Lüfter nicht sauber an (keine Drehzahländerung). Der PWM-Pin ist nur ein
 *   Logik-Eingang → kein Pfeifen trotz hörbarer Frequenz.
 * - Kein Tacho: bewusst nicht angeschlossen (Low-Side-PWM zerhackt das
 *   Open-Collector-Signal; D8/GPIO8 ist Strapping-Pin)
 * - R5 ist Gate-Pull-up an 3V3 → Lüfter läuft hardware-default voll, bis die
 *   Firmware übernimmt: begin() FRÜH in setup() aufrufen!
 */

#pragma once

#include <Arduino.h>

class FanManager {
public:
    /**
     * @brief Konstruktor
     * @param pwmPin GPIO für die Lüfter-PWM (Gate des 2N7002)
     * @param potiPin ADC1-GPIO des Drehzahl-Potis
     */
    FanManager(uint8_t pwmPin, uint8_t potiPin);

    /**
     * @brief Initialisiert die PWM und setzt sofort den Duty vom Poti
     *
     * Früh in setup() aufrufen (Gate-Pull-up R5 → Lüfter läuft sonst voll).
     */
    void begin();

    /**
     * @brief Liest das Poti und stellt die Drehzahl nach (≥ 10 Hz aufrufen)
     */
    void update();

    /**
     * @brief Aktueller PWM-Duty (0-255, für Debug)
     */
    uint8_t currentDuty() const { return duty; }

private:
    uint8_t pwmPin;
    uint8_t potiPin;
    uint8_t duty;             // aktueller PWM-Duty (0-255)
    uint32_t lastUpdateMs;    // Zeitpunkt der letzten Poti-Messung

    /**
     * @brief Liest das Poti und schreibt den (gegen Zappeln gefilterten) Duty
     */
    void applyPotiValue();
};
