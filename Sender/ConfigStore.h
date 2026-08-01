/**
 * @file ConfigStore.h
 * @brief Persistente Turnier-Konfiguration in NVS (ersetzt AVR-EEPROM, R-6)
 *
 * NEU in V3: `Preferences` (NVS, Namespace "bogenampel") statt EEPROM+CRC8.
 * Die CRC entfällt (NVS sichert Integrität selbst), die
 * Wertebereichs-Validierung (Whitelist) bleibt erhalten (FR-005).
 *
 * Schreibzeitpunkt: sofort bei Bestätigung im ConfigMenu — nicht erst beim
 * Ausschalten (Edge Case "Empty battery").
 */

#pragma once

#include <Arduino.h>
#include "Config.h"

class ConfigStore {
public:
    ConfigStore();

    /**
     * @brief Lädt die Konfiguration aus NVS
     *
     * Fehlende oder ungültige Werte (nicht in der Whitelist) werden durch
     * Defaults ersetzt (120 s / 1-2 Schützen, FR-005).
     */
    void load();

    /**
     * @brief Speichert die aktuelle Konfiguration sofort in NVS
     */
    void save();

    /**
     * @brief Schießzeit in Sekunden (120 oder 240)
     */
    uint16_t getShootingTime() const { return shootingTime; }

    /**
     * @brief Schützenanzahl (2 = "1-2", 4 = "3-4")
     */
    uint8_t getShooterCount() const { return shooterCount; }

    /**
     * @brief Setzt die Konfiguration (mit Whitelist-Validierung)
     * @param time 120 oder 240 (sonst Default)
     * @param count 2 oder 4 (sonst Default)
     */
    void set(uint16_t time, uint8_t count);

private:
    uint16_t shootingTime;  // 120 oder 240 Sekunden
    uint8_t shooterCount;   // 2 oder 4

    static uint16_t validateTime(uint16_t time);
    static uint8_t validateCount(uint8_t count);
};
