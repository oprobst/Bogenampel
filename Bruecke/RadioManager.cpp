/**
 * @file RadioManager.cpp
 * @brief Implementierung des ESP-NOW-Empfangs der Brücke
 */

#include "RadioManager.h"

#include <WiFi.h>
#include <esp_wifi.h>

RadioManager* RadioManager::instance = nullptr;

RadioManager::RadioManager()
    : lastSeq(0)
    , lastValid(false)
    , pendingCount(0)
    , signalCounter(0)
    , copyCounter(0)
    , lastSignalAt(0) {
    memset(lastMac, 0, sizeof(lastMac));
}

bool RadioManager::begin() {
    instance = this;

    // Wie beim Empfänger: reiner STA-Modus ohne AP-Verbindung, fester Kanal.
    // Modem-Sleep NICHT abschalten — mit aktivem Bluetooth verlangt der
    // Coexistence-Modus des IDF ein Power-Save ungleich NONE.
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(Radio::CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        DEBUG_PRINTLN("ESP-NOW init FAILED");
        return false;
    }
    esp_now_register_recv_cb(onRecvStatic);

    // MAC direkt vom Treiber: WiFi.macAddress() liefert unter arduino-esp32
    // 3.3.x an dieser Stelle noch 00:00:00:00:00:00
    uint8_t mac[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    DEBUG_PRINTF("ESP-NOW init OK (Kanal %u), MAC %02X:%02X:%02X:%02X:%02X:%02X\n",
                 Radio::CHANNEL, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return true;
}

uint8_t RadioManager::takeSignal() {
    uint8_t count = pendingCount;
    if (count != 0) {
        pendingCount = 0;
    }
    return count;
}

//=============================================================================
// Statischer ESP-NOW-Callback (läuft im WiFi-Task!)
//=============================================================================

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void RadioManager::onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
    const uint8_t* mac = info->src_addr;
#else
void RadioManager::onRecvStatic(const uint8_t* mac, const uint8_t* data, int len)
{
#endif
    if (!instance) return;

    if (len != (int)sizeof(RadioPacketV3)) return;
    const RadioPacketV3* packet = (const RadioPacketV3*)data;
    if (!validatePacket(packet)) return;
    if (packet->type != FT_SIGNAL || packet->command == 0) return;

    instance->copyCounter = instance->copyCounter + 1;

    // Wiederholung desselben Signals → verwerfen
    if (instance->lastValid && packet->seq == instance->lastSeq
        && memcmp(mac, instance->lastMac, 6) == 0) {
        return;
    }
    instance->lastValid = true;
    instance->lastSeq = packet->seq;
    memcpy(instance->lastMac, mac, 6);

    instance->pendingCount = packet->command;
    instance->signalCounter = instance->signalCounter + 1;
    instance->lastSignalAt = millis();
}
