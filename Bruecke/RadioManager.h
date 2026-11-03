/**
 * @file RadioManager.h
 * @brief ESP-NOW-Empfang der Signal-Frames (FT_SIGNAL) für die Bluetooth-Brücke
 *
 * Reiner Zuhörer: Die Brücke registriert keinen Peer und sendet nichts. Der
 * Empfänger schickt jedes Signal als Broadcast mehrfach mit derselben seq
 * (Radio::SIGNAL_REPEATS in Empfaenger/Config.h) — hier wird dedupliziert.
 *
 * Läuft parallel zu Classic Bluetooth auf demselben Funkteil (Coexistence).
 * Ob dabei Frames verloren gehen, soll der Steckbrett-Aufbau zeigen: der
 * Zähler der empfangenen Kopien gegenüber den Signalen gibt das an.
 */

#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <esp_idf_version.h>
#include "Config.h"
#include "Commands.h"

class RadioManager {
public:
    RadioManager();

    /**
     * @brief WLAN (STA, Kanal 1) und ESP-NOW starten, Empfangs-Callback setzen
     * @return true wenn erfolgreich
     */
    bool begin();

    /**
     * @brief Liegt ein neues Signal vor? Holt es ab (Anzahl Töne, sonst 0)
     *
     * Kommen zwischen zwei Abfragen mehrere Signale an, gilt das letzte — wie
     * beim Piezo, dessen beep() eine laufende Folge ebenfalls ersetzt.
     */
    uint8_t takeSignal();

    /// Anzahl unterschiedlicher Signale seit dem Start
    uint32_t signalsReceived() const { return signalCounter; }

    /// Anzahl gültiger FT_SIGNAL-Kopien seit dem Start (inkl. Duplikate)
    uint32_t copiesReceived() const { return copyCounter; }

    /// millis() des letzten neuen Signals (0 = noch keins)
    uint32_t lastSignalMs() const { return lastSignalAt; }

private:
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
#endif

    static RadioManager* instance;

    // Dedup: seq + Absender des letzten Signals
    uint8_t lastMac[6];
    uint8_t lastSeq;
    bool lastValid;

    // Übergabe WiFi-Task → loop
    volatile uint8_t pendingCount;
    volatile uint32_t signalCounter;
    volatile uint32_t copyCounter;
    volatile uint32_t lastSignalAt;
};
