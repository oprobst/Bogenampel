/**
 * @file EncoderManager.h
 * @brief KY-040-Drehgeber: Drehung (Interrupt, Gray-Code-Tabelle) + Taster
 *
 * Drehung: beide Spuren lösen bei jeder Flanke einen Interrupt aus, eine
 * Übergangstabelle wertet nur gültige Gray-Code-Schritte — Prellen hebt sich
 * dabei selbst auf (vor/zurück zwischen zwei Zuständen ergibt netto 0).
 * Läuft die Richtung verkehrt herum: CLK und DT am Steckbrett tauschen.
 *
 * Taster: entprellt im loop(); kurzer Druck meldet sich beim Loslassen, langer
 * Druck sobald die Haltezeit erreicht ist (dann kein kurzer Druck mehr).
 */

#pragma once

#include <Arduino.h>
#include "Config.h"

class EncoderManager {
public:
    enum class Event : uint8_t { NONE, SHORT_PRESS, LONG_PRESS };

    void begin();

    /// Muss in jedem loop() laufen (Taster-Entprellung)
    void update();

    /// Rastungen seit dem letzten Aufruf (+ rechts, − links)
    int16_t takeDetents();

    /// Taster-Ereignis seit dem letzten Aufruf
    Event takeEvent();

private:
    static void IRAM_ATTR onEdge();

    static volatile int32_t steps;   // Gray-Schritte (vom ISR)
    static volatile uint8_t lastAB;  // letzter Spurzustand (vom ISR)

    int32_t stepRemainder = 0;

    bool rawPressed = false;
    bool pressed = false;
    uint32_t rawChangedMs = 0;
    uint32_t pressedSinceMs = 0;
    bool longFired = false;
    Event pending = Event::NONE;
};
