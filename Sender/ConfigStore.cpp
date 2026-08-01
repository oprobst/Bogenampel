/**
 * @file ConfigStore.cpp
 * @brief Implementierung der NVS-Konfiguration
 */

#include "ConfigStore.h"

#include <Preferences.h>

ConfigStore::ConfigStore()
    : shootingTime(TournamentDefaults::DEFAULT_TIME)
    , shooterCount(TournamentDefaults::DEFAULT_COUNT) {
}

uint16_t ConfigStore::validateTime(uint16_t time) {
    if (time == TournamentDefaults::TIME_120_SEC || time == TournamentDefaults::TIME_240_SEC) {
        return time;
    }
    return TournamentDefaults::DEFAULT_TIME;
}

uint8_t ConfigStore::validateCount(uint8_t count) {
    if (count == TournamentDefaults::SHOOTERS_1_2 || count == TournamentDefaults::SHOOTERS_3_4) {
        return count;
    }
    return TournamentDefaults::DEFAULT_COUNT;
}

void ConfigStore::load() {
    Preferences prefs;
    prefs.begin(NVS::NAMESPACE_NAME, /*readOnly=*/true);
    uint16_t time = prefs.getUShort(NVS::KEY_SHOOTING_TIME, TournamentDefaults::DEFAULT_TIME);
    uint8_t count = prefs.getUChar(NVS::KEY_SHOOTER_COUNT, TournamentDefaults::DEFAULT_COUNT);
    prefs.end();

    shootingTime = validateTime(time);
    shooterCount = validateCount(count);

    DEBUG_PRINTF("Config geladen: %us, %u Schuetzen-Modus\n", shootingTime, shooterCount);
}

void ConfigStore::save() {
    Preferences prefs;
    prefs.begin(NVS::NAMESPACE_NAME, /*readOnly=*/false);
    prefs.putUShort(NVS::KEY_SHOOTING_TIME, shootingTime);
    prefs.putUChar(NVS::KEY_SHOOTER_COUNT, shooterCount);
    prefs.end();

    DEBUG_PRINTF("Config gespeichert: %us, %u\n", shootingTime, shooterCount);
}

void ConfigStore::set(uint16_t time, uint8_t count) {
    shootingTime = validateTime(time);
    shooterCount = validateCount(count);
}
