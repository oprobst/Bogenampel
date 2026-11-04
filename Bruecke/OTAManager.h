/**
 * @file OTAManager.h
 * @brief Wartungsmodus der Brücke: WLAN-Station + ArduinoOTA
 *
 * Wird NUR benutzt, wenn der Drehgeber-Taster beim Einschalten gehalten wird. In
 * diesem Modus läuft kein Bluetooth und im Normalbetrieb kein WLAN — beide teilen
 * sich ein Funkteil, und so kommen sie sich nicht in die Quere. Es gibt keinen
 * SoftAP; ohne WLAN bleibt die Brücke im Wartungsmodus und versucht es weiter.
 *
 * Flash-Workflow:
 *   1. Taster halten und einschalten; das LCD zeigt die IP
 *   2. pio run -t upload -e bruecke-ota   (weicht die IP ab: --upload-port <IP>)
 */

#pragma once

#include <Arduino.h>

namespace OTAManager {

    /// Während der Übertragung blockiert handle(); der Callback hält das LCD aktuell
    using ProgressCallback = void (*)(uint8_t percent);

    /// WLAN-Station starten und ArduinoOTA vorbereiten
    void begin(ProgressCallback onProgress);

    /// In jedem loop()-Durchlauf aufrufen: WLAN-Neuversuch und ArduinoOTA
    void handle();

    /// Sind WLAN-Zugangsdaten einkompiliert (wifi_credentials.h)?
    bool hasCredentials();

    /// Verbunden und IP zugewiesen?
    bool isConnected();

    /// Eigene IP als Text
    String ipAddress();

    /// Ist die letzte Übertragung fehlgeschlagen?
    bool lastUpdateFailed();

} // namespace OTAManager
