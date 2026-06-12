/**
 * @file PowerManager.h
 * @brief Soft-Power (Latch), Akku-Messung und Lader-Status (NEU in V3)
 *
 * - Power-Latch GPIO16: HIGH = Gerät bleibt an (allererste Aktion in setup()!),
 *   LOW = Selbstabschaltung (R-13)
 * - Akku: analogReadMilliVolts(GPIO10) × 2,5 (Teiler R4/R8 = 150k/100k),
 *   Median-5-Filter, V2-Schwellen 3,0/3,3/4,2 V (R-11)
 * - Lader MCP73837: STAT1/STAT2/PG Open-Drain → INPUT_PULLUP, Decode laut
 *   Datenblatt (R-12); C_PRG bleibt hochohmig (Default-Ladestrom)
 * - USB-Erkennung: GPIO8 (VBUS-Teiler, aktiv HIGH)
 */

#pragma once

#include <Arduino.h>
#include "Config.h"

/**
 * @brief Lader-Zustand (MCP73837, decodiert aus STAT1/STAT2/PG)
 */
enum class ChargeState : uint8_t {
    NO_INPUT,   // keine gültige Eingangsspannung (PG inaktiv)
    CHARGING,   // Ladevorgang läuft
    COMPLETE,   // Ladung abgeschlossen (bzw. Standby mit Eingangsspannung)
    FAULT       // Lade-Fehler (Timer/Temperatur)
};

class PowerManager {
public:
    PowerManager();

    /**
     * @brief Initialisiert Mess- und Status-Pins (Latch ist bereits gesetzt!)
     *
     * Der LATCH-Pin wird NICHT hier, sondern als allererste Anweisung in
     * setup() gesetzt — sonst schaltet das Gerät beim Loslassen von BTN1 ab.
     */
    void begin();

    /**
     * @brief Zyklische Messung (alle 5 s, V2-Intervall) — in loop() aufrufen
     * @return true wenn neue Werte vorliegen (Anzeige aktualisieren)
     */
    bool update();

    uint16_t batteryMv() const { return filteredMv; }
    uint8_t batteryPercent() const;                       // linear 3,0-4,2 V
    bool isLowBattery() const { return filteredMv < Battery::VOLTAGE_LOW_MV; }
    bool isUsbConnected() const { return usbConnected; }
    ChargeState chargeState() const { return charge; }

    /**
     * @brief Latch loslassen — Gerät schaltet sich ab (letzter Schritt!)
     *
     * Reihenfolge davor (Aufrufer): Abschalt-Screen zeichnen →
     * display.hibernate() → LOAD-Rail aus → DANN latchOff().
     */
    void latchOff();

private:
    uint16_t filteredMv;       // Median-gefilterte Akkuspannung
    bool usbConnected;
    ChargeState charge;
    uint32_t lastUpdateMs;

    uint16_t readBatteryMv();  // Einzelmessung × Teilerfaktor
    void readChargerStatus();
};
