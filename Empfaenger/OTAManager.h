/**
 * @file OTAManager.h
 * @brief Over-The-Air Update via WiFi-Station + ArduinoOTA (OTA-Wartungsmodus)
 *
 * Wird NUR im OTA-Wartungsmodus benutzt (Debug-Taster D7 beim Boot gehalten,
 * FR-002). Das Gerät verbindet sich als reine WiFi-Station mit dem Heimnetz
 * (Zugangsdaten aus Config.h::OTA / wifi_credentials.h) — KEIN SoftAP. ESP-NOW
 * läuft in diesem Modus nicht (Single-Radio-Kanalkonflikt, FR-006).
 *
 * Flash-Workflow:
 *   1. Empfänger mit gehaltenem Taster einschalten (Status-LED blinkt, FR-004)
 *   2. pio run -t upload -e empfaenger-ota   (upload_port = IP des Empfängers)
 */

#pragma once

#include <Arduino.h>

namespace OTAManager {

    /**
     * @brief WiFi-Station starten (KEIN SoftAP) und ArduinoOTA vorbereiten.
     *        Wird nur im OTA-Wartungsmodus aufgerufen (setupOta()); im
     *        Normalbetrieb läuft stattdessen ESP-NOW (RadioManager).
     */
    void begin();

    /**
     * @brief OTA bedienen: WiFi-Reconnect-Retry, ArduinoOTA und Status-LED-Signal.
     *        Muss in jedem loopOta()-Durchlauf laufen (nicht-blockierend, FR-004).
     */
    void handle();

    /**
     * @brief true, sobald die Station verbunden ist UND eine IP zugewiesen wurde.
     */
    bool isConnected();

} // namespace OTAManager
