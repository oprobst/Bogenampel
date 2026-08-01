/**
 * @file OTAManager.cpp
 * @brief OTA-Wartungsmodus: reine WiFi-Station + ArduinoOTA + Status-LED-Signal.
 *
 * Wird NUR im OTA-Wartungsmodus benutzt (Taster D7 beim Boot). Es wird KEIN
 * SoftAP aufgespannt; scheitert der Verbindungsaufbau, bleibt das Gerät im
 * Wartungsmodus und versucht es endlos weiter (FR-009) — es fällt nie in den
 * Normalbetrieb oder auf einen Accesspoint zurück.
 *
 * Status-LED (D9, aktiv LOW), drei Muster (FR-004a-c):
 *   CONNECTING: schnelles kurzes Blitzen   (noch nicht verbunden / keine IP)
 *   READY:      langsames gleichm. Blinken (verbunden + IP, wartet auf Flash)
 *   UPDATING:   leuchtet, kurze Aussetzer  (Übertragung läuft)
 */

#include "OTAManager.h"
#include "Config.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

namespace OTAManager {

namespace {
    uint32_t lastWifiAttempt = 0;   // millis() des letzten WiFi.begin()
    bool otaStarted = false;        // ArduinoOTA.begin() erst nach erster IP
    bool updating = false;          // wird in onStart/onEnd/onError gesetzt
    uint32_t ledEpoch = 0;          // Zeitbasis fürs LED-Muster

    enum class Phase : uint8_t { CONNECTING, READY, UPDATING };

    bool haveIp() {
        return WiFi.status() == WL_CONNECTED && (uint32_t)WiFi.localIP() != 0;
    }

    Phase currentPhase() {
        if (updating) return Phase::UPDATING;   // Transfer hat Vorrang (FR-004c)
        if (haveIp()) return Phase::READY;       // verbunden + IP (FR-004b)
        return Phase::CONNECTING;                // sucht noch (FR-004a)
    }

    void setLed(bool on) {
        digitalWrite(Pins::STATUS_LED, on ? STATUS_LED_ON : STATUS_LED_OFF);
    }

    // Nicht-blockierendes LED-Muster je Phase (FR-004a-c) — D9 aktiv LOW
    void updateLed() {
        uint16_t period, onTime;
        switch (currentPhase()) {
            case Phase::READY:
                period = OtaSignal::READY_PERIOD_MS;      onTime = OtaSignal::READY_ON_MS;      break;
            case Phase::UPDATING:
                period = OtaSignal::UPDATING_PERIOD_MS;   onTime = OtaSignal::UPDATING_ON_MS;   break;
            case Phase::CONNECTING:
            default:
                period = OtaSignal::CONNECTING_PERIOD_MS; onTime = OtaSignal::CONNECTING_ON_MS; break;
        }
        uint32_t t = (millis() - ledEpoch) % period;
        setLed(t < onTime);
    }
}

void begin() {
    // Reiner Station-Modus — KEIN SoftAP (FR-005/FR-009)
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    // WiFi-Power-Save AUS: der Modem-Sleep verschluckt sonst die zeitkritische
    // ArduinoOTA-Auth-Antwort (PBKDF2) → "No response from device". Im
    // Wartungsmodus ist Stromsparen irrelevant, also Funk wachhalten.
    WiFi.setSleep(false);

    if (strlen(OTA::WIFI_SSID) > 0) {
        DEBUG_PRINTF("OTA: Verbinde mit '%s'...\n", OTA::WIFI_SSID);
        WiFi.begin(OTA::WIFI_SSID, OTA::WIFI_PASS);
    } else {
        DEBUG_PRINTLN("OTA: KEINE WLAN-Zugangsdaten — bleibe im CONNECTING-Blinken (FR-009)");
    }
    lastWifiAttempt = millis();

    // ArduinoOTA konfigurieren (begin() erst nach erster IP, siehe handle()).
    ArduinoOTA.setHostname(OTA::HOSTNAME);
    // HINWEIS (FR-012): Das ArduinoOTA-Passwort (PBKDF2-HMAC-SHA256 in
    // arduino-esp32 3.3.7) ist mit der espota.py von PlatformIO (tool-esptoolpy
    // 5.1.2) inkompatibel — die Auth scheitert stumm ("No response from device").
    // Der Zugang ist stattdessen durch den physischen Boot-Taster-Gate geschützt
    // (OTA-Modus nur mit gehaltenem Taster beim Einschalten erreichbar).
    // ArduinoOTA.setPassword(OTA::PASSWORD);
    ArduinoOTA.onStart([]() { updating = true;  DEBUG_PRINTLN("OTA: Start"); });
    ArduinoOTA.onEnd([]()   { updating = false; DEBUG_PRINTLN("OTA: Ende — Neustart"); });
    ArduinoOTA.onError([](ota_error_t e) { updating = false; DEBUG_PRINTF("OTA Fehler [%u]\n", e); });

    ledEpoch = millis();
    DEBUG_PRINTF("OTA-Wartungsmodus: warte auf WLAN (%s)\n", OTA::HOSTNAME);
}

void handle() {
    // WiFi-Reconnect-Retry: nie aufgeben, nie SoftAP (FR-009)
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

    updateLed();
}

bool isConnected() {
    return haveIp();
}

} // namespace OTAManager
