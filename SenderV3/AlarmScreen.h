/**
 * @file AlarmScreen.h
 * @brief Alarm-Screen für Notfall-Abbruch (e-Paper)
 *
 * PORT aus V2 (Sender/AlarmScreen.h): statisches Alarm-UI (auf e-Paper kein
 * Blinken nötig/sinnvoll), erweitert um Zustellstatus (US3-Szenario 3) und
 * den Quittierungs-Hinweis (OK kurz = quittieren → CMD_STOP).
 */

#pragma once

#include "Config.h"
#include "EpaperDisplay.h"

class AlarmScreen {
public:
    AlarmScreen(EpaperDisplay& epd);

    /**
     * @brief Zeichnet den Alarm-Screen in den Puffer (ohne Refresh!)
     * @param delivered true wenn CMD_ALARM bestätigt zugestellt wurde
     */
    void draw(bool delivered);

private:
    EpaperDisplay& epd;
};
