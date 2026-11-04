/**
 * @file SignalLink.h
 * @brief Empfang der Signaltöne vom Empfänger über die UART-Leitung
 *
 * Eine Leitung, nur Empfänger → Brücke: D5/GPIO7 des XIAO an Pins::SIGNAL_RX,
 * dazu gemeinsame Masse. Telegramm siehe namespace SignalLink in Config.h.
 * Ungültige Bytes werden verworfen; der Zähler zeigt, ob die Leitung gestört ist.
 */

#pragma once

#include <Arduino.h>
#include "Config.h"

class SignalReceiver {
public:
    /// UART2 auf Pins::SIGNAL_RX starten
    void begin();

    /// Muss in jedem loop() laufen: liest die angekommenen Bytes aus
    void update();

    /**
     * @brief Liegt ein neues Signal vor? Holt es ab (Anzahl Töne, sonst 0)
     *
     * Kommen zwischen zwei Abfragen mehrere Signale an, gilt das letzte — wie
     * beim Piezo, dessen beep() eine laufende Folge ebenfalls ersetzt.
     */
    uint8_t takeSignal();

    /// Anzahl gültiger Telegramme seit dem Start
    uint32_t signalsReceived() const { return signalCounter; }

    /// Anzahl verworfener Bytes bzw. Telegramme seit dem Start
    uint32_t rejected() const { return rejectCounter; }

    /// millis() des letzten gültigen Telegramms
    uint32_t lastSignalMs() const { return lastSignalAt; }

private:
    void parse(uint8_t byte);

    uint8_t position = 0;    // 0 = wartet auf START, 1 = Anzahl, 2 = Prüfbyte
    uint8_t count = 0;
    uint32_t lastByteMs = 0;

    uint8_t pendingCount = 0;
    uint32_t signalCounter = 0;
    uint32_t rejectCounter = 0;
    uint32_t lastSignalAt = 0;
};
