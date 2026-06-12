/**
 * @file OTAManager.cpp
 * @brief Implementierung des OTA-Updates
 *
 * Verbindungsstrategie:
 *   1. Wenn OTA::WIFI_SSID gesetzt: STA-Verbindung ins Heimnetz (DHCP-IP).
 *      mDNS-Hostname: bogenampel-empfaenger.local
 *   2. Fallback: SoftAP auf Kanal 1 (gleich wie ESP-NOW, IP immer 192.168.4.1).
 */

#include "OTAManager.h"
#include "Config.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

namespace OTAManager {

void begin() {
    bool staConnected = false;

    if (strlen(OTA::WIFI_SSID) > 0) {
        DEBUG_PRINTF("OTA: Verbinde mit '%s'...\n", OTA::WIFI_SSID);
        WiFi.begin(OTA::WIFI_SSID, OTA::WIFI_PASS);

        uint32_t start = millis();
        while (WiFi.status() != WL_CONNECTED
               && millis() - start < OTA::WIFI_TIMEOUT) {
            delay(100);
        }

        staConnected = (WiFi.status() == WL_CONNECTED);
        if (staConnected) {
            DEBUG_PRINTF("OTA WiFi OK: IP=%s\n",
                         WiFi.localIP().toString().c_str());
        } else {
            DEBUG_PRINTLN("OTA WiFi TIMEOUT — Fallback auf SoftAP");
        }
    }

    if (!staConnected) {
        // SoftAP auf Kanal 1 (gleicher Kanal wie ESP-NOW → kein Konflikt)
        WiFi.softAP(OTA::AP_SSID, OTA::PASSWORD, Radio::CHANNEL);
        DEBUG_PRINTF("OTA SoftAP: SSID=%s  IP=%s\n",
                     OTA::AP_SSID,
                     WiFi.softAPIP().toString().c_str());
    }

    ArduinoOTA.setHostname(OTA::HOSTNAME);
    ArduinoOTA.setPassword(OTA::PASSWORD);

    ArduinoOTA.onStart([]() {
        DEBUG_PRINTLN("OTA: Start");
    });
    ArduinoOTA.onEnd([]() {
        DEBUG_PRINTLN("OTA: Ende — Neustart");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        DEBUG_PRINTF("OTA Fehler [%u]\n", error);
    });

    ArduinoOTA.begin();
    DEBUG_PRINTF("OTA bereit: %s.local\n", OTA::HOSTNAME);
}

void handle() {
    ArduinoOTA.handle();
}

} // namespace OTAManager
