/**
 * @file OTAManager.h
 * @brief Over-The-Air Update via WiFi SoftAP + ArduinoOTA
 *
 * Das Gerät spannt einen SoftAP auf (SSID/Passwort aus Config.h::OTA).
 * Der AP läuft auf Kanal 1 (identisch mit ESP-NOW) — beide koexistieren in
 * WIFI_AP_STA-Modus ohne Kanalwechsel.
 *
 * Flash-Workflow:
 *   1. Laptop mit "Bogenampel-Sender" (PW: bogenampel) verbinden
 *   2. pio run -t upload -e sender-ota
 *      (oder: --upload-port 192.168.4.1)
 */

#pragma once

#include <Arduino.h>

namespace OTAManager {

    /**
     * @brief SoftAP starten und ArduinoOTA initialisieren.
     *        Muss NACH radio.begin() aufgerufen werden (WiFi bereits im AP_STA-Modus).
     */
    void begin();

    /**
     * @brief Eingehende OTA-Verbindungen bedienen.
     *        Muss in jedem loop()-Durchlauf aufgerufen werden.
     */
    void handle();

} // namespace OTAManager
