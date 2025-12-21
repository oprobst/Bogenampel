/**
 * @file ButtonManager.h
 * @brief Button-Verwaltung mit Debouncing und Event-System
 */

#pragma once

#include "Config.h"

/**
 * @brief Button-Enumeration
 */
enum class Button : uint8_t {
    LEFT = 0,   // J1: Links-Navigation
    OK = 1,     // J2: Bestätigen/Auswählen
    RIGHT = 2,  // J3: Rechts-Navigation
    COUNT = 3   // Anzahl der Buttons
};

/**
 * @brief Button Manager mit Debouncing und Event-Detection
 *
 * Features:
 * - Automatisches Debouncing (konfigurierbar)
 * - Event-Flags: wasPressed(), wasReleased()
 * - Long-Press Detection
 * - Polling von aktuellen Zuständen
 */
class ButtonManager {
public:
    ButtonManager();

    /**
     * @brief Initialisiert alle Button-Pins
     */
    void begin();

    /**
     * @brief Initialisiert den Buzzer-Pin
     */
    void initBuzzer();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     */
    void update();

    /**
     * @brief Prüft ob Button aktuell gedrückt ist (mit Debouncing)
     * @param btn Button-ID
     * @return true wenn gedrückt
     */
    bool isPressed(Button btn) const;

    /**
     * @brief Prüft ob Button seit letztem Abruf gedrückt wurde
     * @param btn Button-ID
     * @return true wenn gedrückt (Flag wird gelöscht!)
     */
    bool wasPressed(Button btn);

    /**
     * @brief Prüft ob Button seit letztem Abruf losgelassen wurde
     * @param btn Button-ID
     * @return true wenn losgelassen (Flag wird gelöscht!)
     */
    bool wasReleased(Button btn);

    /**
     * @brief Prüft ob Button länger als duration gedrückt ist
     * @param btn Button-ID
     * @param duration Mindestdauer in Millisekunden
     * @return true wenn Long Press erkannt
     */
    bool isLongPress(Button btn, uint32_t duration = 1000) const;

    /**
     * @brief Prüft ob irgendein Button gedrückt ist
     * @return true wenn mindestens ein Button gedrückt
     */
    bool isAnyPressed() const;

    /**
     * @brief Prüft ob Alarm ausgelöst wurde (OK > 3 Sekunden)
     * @return true wenn Alarm-Trigger erkannt (Flag wird gelöscht!)
     */
    bool isAlarmTriggered();

private:
    /**
     * @brief Zustand eines einzelnen Buttons
     */
    struct ButtonState {
        bool pressed;              // Aktuell gedrückt (nach Debouncing)
        bool lastRawState;         // Letzter roher Pin-Zustand
        uint32_t lastChangeTime;   // Zeitpunkt der letzten Zustandsänderung
        uint32_t pressTime;        // Zeitpunkt des Drückens (für Long Press)
        bool wasPressedFlag;       // Event-Flag: wurde gedrückt
        bool wasReleasedFlag;      // Event-Flag: wurde losgelassen
    };

    ButtonState buttons[static_cast<uint8_t>(Button::COUNT)];

    // Alarm-Detektion (OK-Button > 3 Sekunden)
    uint32_t okPressStartTime;    // Zeitpunkt, wann OK gedrückt wurde
    bool okPressActive;            // OK-Button aktuell gedrückt
    bool alarmTriggered;           // Alarm wurde ausgelöst (Flag)

    /**
     * @brief Gibt den Pin für einen Button zurück
     */
    uint8_t getPin(Button btn) const;

    /**
     * @brief Liest den rohen Button-Zustand (LOW = gedrückt)
     */
    bool readRawState(Button btn) const;

    /**
     * @brief Spielt einen kurzen Klick-Ton ab
     */
    void playClickSound();
};
