/**
 * @file BuzzerManager.h
 * @brief Nicht-blockierender Buzzer-Manager
 *
 * Verwaltet Buzzer-Pieptöne ohne die Hauptschleife zu blockieren.
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Manager-Klasse für nicht-blockierende Buzzer-Sequenzen
 *
 * Erzeugt Piepton-Sequenzen (z.B. 2x Piep für Vorbereitung, 3x Piep für Stop).
 * Jeder Pieppton: 500ms Ton + 500ms Pause.
 */
class BuzzerManager {
public:
    /**
     * @brief Konstruktor
     * @param pin GPIO-Pin für Buzzer
     * @param frequency Frequenz des Piezo-Tons in Hz (Standard: 2700Hz)
     */
    BuzzerManager(uint8_t pin, uint16_t frequency = 2700);

    /**
     * @brief Initialisiert den Buzzer (Pin-Mode setzen)
     */
    void begin();

    /**
     * @brief Startet eine Piepton-Sequenz
     * @param count Anzahl der Pieptöne (0 = keine Aktion)
     *
     * Jeder Pieppton: 500ms Ton, 500ms Pause
     */
    void beep(uint8_t count);

    /**
     * @brief Aktualisiert den Buzzer-Zustand (nicht-blockierend)
     *
     * WICHTIG: Muss regelmäßig in loop() aufgerufen werden!
     */
    void update();

    /**
     * @brief Stoppt aktuelle Buzzer-Sequenz sofort
     */
    void stop();

    /**
     * @brief Prüft ob Buzzer aktiv ist
     * @return true wenn Sequenz läuft, false sonst
     */
    bool isActive() const { return active; }

private:
    uint8_t buzzerPin;           // GPIO-Pin
    uint16_t buzzerFrequency;    // Frequenz in Hz

    // State-Machine Variablen
    bool active;                 // Ist Buzzer-Sequenz aktiv?
    uint8_t state;               // 0 = Pause, 1 = Beeping
    uint8_t beepCount;           // Aktueller Piepston (0-basiert)
    uint8_t targetBeeps;         // Anzahl Pieptöne gesamt
    uint32_t lastToggle;         // Zeitpunkt der letzten Zustandsänderung

    // Konstanten
    static constexpr uint16_t BEEP_DURATION_MS = 500;   // Dauer eines Tons
    static constexpr uint16_t PAUSE_DURATION_MS = 500;  // Dauer einer Pause
};
