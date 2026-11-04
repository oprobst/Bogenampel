/**
 * @file OTAManager.cpp
 * @brief Implementierung des Wartungsmodus (Vorbild: Empfaenger/OTAManager.cpp)
 */

#include "OTAManager.h"
#include "Config.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

namespace OTAManager {

namespace {
    ProgressCallback progressCallback = nullptr;
    uint32_t lastWifiAttempt = 0;
    bool otaStarted = false;   // ArduinoOTA.begin() erst nach der ersten IP
    bool updateFailed = false;
    uint8_t lastPercent = 255;
}

void begin(ProgressCallback onProgress) {
    progressCallback = onProgress;

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    // Modem-Sleep aus: er verschluckt sonst Antworten an espota ("No response").
    // Im Wartungsmodus spielt der Stromverbrauch keine Rolle.
    WiFi.setSleep(false);

    if (hasCredentials()) {
        DEBUG_PRINTF("OTA: Verbinde mit '%s'...\n", OTA::WIFI_SSID);
        WiFi.begin(OTA::WIFI_SSID, OTA::WIFI_PASS);
    } else {
        DEBUG_PRINTLN("OTA: keine WLAN-Zugangsdaten einkompiliert");
    }
    lastWifiAttempt = millis();

    ArduinoOTA.setHostname(OTA::HOSTNAME);
    // Kein OTA-Passwort, wie bei Sender und Empfänger: die Auth ist mit der
    // espota.py von PlatformIO inkompatibel. Geschützt ist der Zugang durch den
    // Taster, der beim Einschalten gehalten werden muss.
    ArduinoOTA.onStart([]() {
        updateFailed = false;
        lastPercent = 255;
        DEBUG_PRINTLN("OTA: Start");
    });
    ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
        uint8_t percent = total > 0 ? (uint8_t)((uint64_t)done * 100 / total) : 0;
        if (percent != lastPercent) {
            lastPercent = percent;
            if (progressCallback) progressCallback(percent);
        }
    });
    ArduinoOTA.onEnd([]() { DEBUG_PRINTLN("OTA: Ende, Neustart"); });
    ArduinoOTA.onError([](ota_error_t e) {
        updateFailed = true;
        DEBUG_PRINTF("OTA: Fehler %u\n", (unsigned)e);
    });
}

void handle() {
    if (hasCredentials() && WiFi.status() != WL_CONNECTED
        && millis() - lastWifiAttempt > OTA::WIFI_TIMEOUT_MS) {
        DEBUG_PRINTLN("OTA: WLAN noch nicht verbunden, neuer Versuch");
        WiFi.disconnect();
        WiFi.begin(OTA::WIFI_SSID, OTA::WIFI_PASS);
        lastWifiAttempt = millis();
    }

    if (!otaStarted && isConnected()) {
        ArduinoOTA.begin();
        otaStarted = true;
        DEBUG_PRINTF("OTA bereit: IP %s\n", WiFi.localIP().toString().c_str());
    }

    if (otaStarted) {
        ArduinoOTA.handle();
    }
}

bool hasCredentials() {
    return strlen(OTA::WIFI_SSID) > 0;
}

bool isConnected() {
    return WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0;
}

String ipAddress() {
    return WiFi.localIP().toString();
}

bool lastUpdateFailed() {
    return updateFailed;
}

} // namespace OTAManager
