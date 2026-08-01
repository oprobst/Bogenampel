/**
 * @file BuzzerManager.h
 * @brief Nicht-blockierender Buzzer-Manager (LEDC-PWM, Lautstärke regelbar)
 *
 * PORT aus V2 (Empfaenger/BuzzerManager.h): API unverändert (beep/update/stop),
 * intern ersetzt LEDC-PWM das AVR-digitalWrite — V3 treibt einen 12-V-Piezo-
 * Transducer über BC337 (R-9):
 * - Tonfrequenz = LEDC-Frequenz = Resonanzfrequenz des Transducers
 *   (Config::BUZZER_FREQUENCY_HZ; am lautesten genau bei Resonanz)
 * - Lautstärke = Duty-Cycle 0…50 % (setVolume, vom Lautstärke-Poti — FR-021)
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Manager-Klasse für nicht-blockierende Buzzer-Sequenzen
 *
 * Erzeugt Piepton-Sequenzen (z.B. 2x Piep für Vorbereitung, 3x Piep für Stop).
 * Jeder Piepton: 500ms Ton + 500ms Pause.
 */
class BuzzerManager {
public:
    // LEDC-Auflösung: 8 Bit → Duty 0-255; 50 % = 128 (Maximal-Lautstärke am Transducer)
    static constexpr uint8_t DUTY_MAX = 128;   // 50 % Duty (Lautstärke-Maximum)
    static constexpr uint8_t DUTY_MIN = 4;     // hörbares Minimum (FR-021)

    /**
     * @brief Konstruktor
     * @param pin GPIO-Pin für Buzzer (LEDC-fähig)
     * @param frequency Frequenz des Piezo-Tons in Hz = Transducer-Resonanz (Standard: 3250Hz)
     */
    BuzzerManager(uint8_t pin, uint16_t frequency = 3250);

    /**
     * @brief Initialisiert den Buzzer (LEDC-Kanal konfigurieren, stumm)
     */
    void begin();

    /**
     * @brief Startet eine Piepton-Sequenz
     * @param count Anzahl der Pieptöne (0 = keine Aktion)
     *
     * Jeder Piepton: 500ms Ton, 500ms Pause
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
     * @brief Setzt die Lautstärke (wirkt live, auch während eines Tons)
     * @param duty LEDC-Duty DUTY_MIN…DUTY_MAX (wird begrenzt)
     */
    void setVolume(uint8_t duty);

    /**
     * @brief Startet/verlängert den Lautstärke-Vorhörton (kontinuierlich)
     * @param holdMs Nachlaufzeit ab jetzt, bevor der Ton verstummt
     *
     * Erzeugt einen Dauerton, dessen Lautstärke live der eingestellten
     * Lautstärke (setVolume) folgt — gedacht zum Vorhören beim Drehen am
     * Lautstärke-Poti. Jeder Aufruf verschiebt das Verstummen um holdMs nach
     * hinten. Funkt NICHT in eine laufende Signalsequenz (beep) hinein.
     */
    void startPreview(uint16_t holdMs);

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

    uint8_t volumeDuty;          // Aktuelle Lautstärke (LEDC-Duty)
    bool toneOn;                 // Ton gerade aktiv (für Live-Lautstärke)

    // Lautstärke-Vorhörton (kontinuierlich beim Poti-Drehen)
    bool previewActive;          // Läuft gerade ein Vorhörton?
    uint32_t previewUntil;       // Zeitpunkt (millis), zu dem der Vorhörton verstummt

    /**
     * @brief Schaltet den Ton an/aus (LEDC-Duty setzen)
     */
    void setTone(bool on);

    // Konstanten
    static constexpr uint16_t BEEP_DURATION_MS = 500;   // Dauer eines Tons
    static constexpr uint16_t PAUSE_DURATION_MS = 500;  // Dauer einer Pause
};
