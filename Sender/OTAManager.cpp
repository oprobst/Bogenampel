/**
 * @file OTAManager.cpp
 * @brief Implementierung des OTA-Wartungsmodus (reine Station, kein SoftAP)
 */

#include "OTAManager.h"
#include "Config.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

namespace OTAManager {

namespace {
    uint32_t lastWifiAttempt = 0;    // millis() des letzten WiFi.begin()
    bool otaStarted = false;         // ArduinoOTA.begin() erst nach erster IP
    bool updating = false;           // zwischen onStart und onEnd/onError
    char ipBuffer[16] = {0};         // "255.255.255.255" + NUL
    void (*updateStartCb)() = nullptr;

    bool haveIp() {
        return WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0;
    }
}

void begin(void (*onUpdateStart)()) {
    updateStartCb = onUpdateStart;

    // Reiner Station-Modus — KEIN SoftAP
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    // WiFi-Power-Save AUS: der Modem-Sleep verschluckt sonst zeitkritische
    // OTA-Antworten. Im Wartungsmodus ist Stromsparen irrelevant.
    WiFi.setSleep(false);

    if (strlen(OTA::WIFI_SSID) > 0) {
        DEBUG_PRINTF("OTA: Verbinde mit '%s'...\n", OTA::WIFI_SSID);
        WiFi.begin(OTA::WIFI_SSID, OTA::WIFI_PASS);
    } else {
        DEBUG_PRINTLN("OTA: KEINE WLAN-Zugangsdaten hinterlegt");
    }
    lastWifiAttempt = millis();

    ArduinoOTA.setHostname(OTA::HOSTNAME);
    // HINWEIS: Das ArduinoOTA-Passwort (PBKDF2-HMAC-SHA256 in arduino-esp32
    // 3.3.7) ist mit der espota.py von PlatformIO inkompatibel — die Auth
    // scheitert stumm ("No response from device"). Denselben Befund gibt es
    // beim Empfänger. Der Zugang ist stattdessen physisch gesichert: der
    // Wartungsmodus ist nur mit CONFIG+OK beim Einschalten erreichbar.
    // ArduinoOTA.setPassword(OTA::PASSWORD);

    ArduinoOTA.onStart([]() {
        updating = true;
        DEBUG_PRINTLN("OTA: Start");
        if (updateStartCb) {
            updateStartCb();  // einmalige Anzeige — das Panel bleibt danach ruhig
        }
    });
    ArduinoOTA.onEnd([]() {
        updating = false;
        DEBUG_PRINTLN("OTA: Ende — Neustart");
    });
    ArduinoOTA.onError([](ota_error_t e) {
        updating = false;
        DEBUG_PRINTF("OTA Fehler [%u]\n", e);
    });

    DEBUG_PRINTF("OTA-Wartungsmodus: warte auf WLAN (%s)\n", OTA::HOSTNAME);
}

void handle() {
    // Reconnect-Retry: nie aufgeben, nie SoftAP
    if (strlen(OTA::WIFI_SSID) > 0 && WiFi.status() != WL_CONNECTED) {
        if (millis() - lastWifiAttempt > OTA::WIFI_TIMEOUT) {
            DEBUG_PRINTLN("OTA: WLAN noch nicht verbunden — neuer Versuch");
            WiFi.disconnect();
            WiFi.begin(OTA::WIFI_SSID, OTA::WIFI_PASS);
            lastWifiAttempt = millis();
        }
    }

    // ArduinoOTA erst starten, wenn eine IP existiert (UDP/mDNS auf live STA-Netif)
    if (!otaStarted && haveIp()) {
        ArduinoOTA.begin();
        otaStarted = true;
        DEBUG_PRINTF("OTA bereit: IP=%s\n", WiFi.localIP().toString().c_str());
    }

    if (otaStarted) {
        ArduinoOTA.handle();
    }
}

Phase phase() {
    if (updating) return Phase::UPDATING;
    if (haveIp()) return Phase::READY;
    return Phase::CONNECTING;
}

const char* ipString() {
    if (!haveIp()) {
        ipBuffer[0] = '\0';
        return ipBuffer;
    }
    IPAddress ip = WiFi.localIP();
    snprintf(ipBuffer, sizeof(ipBuffer), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    return ipBuffer;
}

} // namespace OTAManager
