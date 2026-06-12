/**
 * @file RadioManager.cpp
 * @brief Implementierung des ESP-NOW-Transports für den Sender
 */

#include "RadioManager.h"

#include <WiFi.h>
#include <esp_wifi.h>

RadioManager* RadioManager::instance = nullptr;

namespace {
    const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    // Wartezeit auf den Send-Callback (Link-ACK kommt typ. < 10 ms)
    constexpr uint32_t SEND_CALLBACK_TIMEOUT_MS = 100;
}

RadioManager::RadioManager()
    : peerDiscovered(false)
    , initialized(false)
    , seq(0)
    , lastHelloMs(0)
    , sendResultPending(false)
    , sendAcked(false)
    , helloAckPending(false) {
    memset(peerMac, 0, sizeof(peerMac));
    memset(helloAckMac, 0, sizeof(helloAckMac));
}

bool RadioManager::begin() {
    instance = this;

    // WLAN in AP_STA-Modus, fester Kanal (R-3); AP_STA erlaubt SoftAP (OTA) und
    // ESP-NOW gleichzeitig auf Kanal 1
    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_channel(Radio::CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        DEBUG_PRINTLN("ESP-NOW init FAILED");
        return false;
    }

    esp_now_register_send_cb(onSendStatic);
    esp_now_register_recv_cb(onRecvStatic);

    // Broadcast-Peer für die Discovery registrieren
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = Radio::CHANNEL;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        DEBUG_PRINTLN("ESP-NOW broadcast peer FAILED");
        return false;
    }

    // Sequenznummer pro Boot zufällig initialisieren (R-4)
    seq = (uint8_t)esp_random();

    initialized = true;
    DEBUG_PRINTF("ESP-NOW init OK (Kanal %u)\n", Radio::CHANNEL);
    return true;
}

void RadioManager::update() {
    if (!initialized) return;

    // Vom Recv-Callback gemeldetes HELLO_ACK verarbeiten (Peer-Registrierung
    // im Loop-Kontext, nicht im WiFi-Task)
    if (helloAckPending) {
        helloAckPending = false;
        handleHelloAck(helloAckMac);
    }

    if (peerDiscovered) return;

    // FT_HELLO-Broadcast, max. 1 Hz bis FT_HELLO_ACK (Regel 4)
    uint32_t now = millis();
    if (now - lastHelloMs >= Radio::HELLO_INTERVAL_MS || lastHelloMs == 0) {
        lastHelloMs = now;
        sendHello();
    }
}

void RadioManager::sendHello() {
    RadioPacketV3 packet;
    packet.magic0 = PACKET_MAGIC0;
    packet.magic1 = PACKET_MAGIC1;
    packet.type = FT_HELLO;
    packet.seq = ++seq;
    packet.command = 0x00;
    packet.checksum = calculateChecksum(packet.command);

    esp_now_send(BROADCAST_MAC, (const uint8_t*)&packet, sizeof(packet));
    DEBUG_PRINTLN("HELLO broadcast");
}

void RadioManager::handleHelloAck(const uint8_t* mac) {
    if (peerDiscovered && memcmp(peerMac, mac, 6) == 0) {
        return;  // bereits registriert
    }

    // Alte Peer-Registrierung ersetzen (z. B. nach Empfänger-Tausch)
    if (peerDiscovered) {
        esp_now_del_peer(peerMac);
    }

    memcpy(peerMac, mac, 6);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, peerMac, 6);
    peer.channel = Radio::CHANNEL;
    peer.encrypt = false;
    if (esp_now_is_peer_exist(peerMac)) {
        esp_now_mod_peer(&peer);
    } else {
        esp_now_add_peer(&peer);
    }

    peerDiscovered = true;
    DEBUG_PRINTF("Empfaenger gefunden: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5]);
}

TransmissionResult RadioManager::transmitOnce(const RadioPacketV3& packet, const uint8_t* mac) {
    sendAcked = false;
    sendResultPending = true;

    esp_err_t err = esp_now_send(mac, (const uint8_t*)&packet, sizeof(packet));
    if (err != ESP_OK) {
        sendResultPending = false;
        return TX_ERROR;
    }

    // Auf den Send-Callback warten (läuft im WiFi-Task, setzt die Flags)
    uint32_t start = millis();
    while (sendResultPending && (millis() - start) < SEND_CALLBACK_TIMEOUT_MS) {
        delay(1);
    }

    if (sendResultPending) {
        sendResultPending = false;
        return TX_TIMEOUT;  // Callback kam nicht — wie NACK behandeln
    }
    return sendAcked ? TX_SUCCESS : TX_TIMEOUT;
}

TransmissionResult RadioManager::sendCommand(RadioCommand cmd) {
    if (!initialized) return TX_ERROR;
    if (!peerDiscovered) return TX_TIMEOUT;

    RadioPacketV3 packet;
    packet.magic0 = PACKET_MAGIC0;
    packet.magic1 = PACKET_MAGIC1;
    packet.type = FT_CMD;
    packet.seq = ++seq;  // neue seq pro Kommando; Retries behalten sie (Dedup, FR-008)
    packet.command = (uint8_t)cmd;
    packet.checksum = calculateChecksum(packet.command);

    TransmissionResult result = TX_TIMEOUT;
    for (uint8_t attempt = 0; attempt < Radio::MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            delay(Radio::RETRY_DELAY_MS);
        }
        result = transmitOnce(packet, peerMac);
        if (result == TX_SUCCESS) break;
    }

    DEBUG_PRINTF("TX %s: %s\n", commandToString(cmd),
                 result == TX_SUCCESS ? "OK" : (result == TX_TIMEOUT ? "TIMEOUT" : "ERROR"));
    return result;
}

uint8_t RadioManager::pingQualityTest(uint8_t pings, uint16_t intervalMs,
                                      void (*progress)(uint8_t done, uint8_t ok)) {
    if (!initialized || !peerDiscovered || pings == 0) return 0;

    uint8_t acked = 0;
    for (uint8_t i = 0; i < pings; i++) {
        uint32_t slotStart = millis();

        RadioPacketV3 packet;
        packet.magic0 = PACKET_MAGIC0;
        packet.magic1 = PACKET_MAGIC1;
        packet.type = FT_PING;
        packet.seq = ++seq;
        packet.command = 0x00;
        packet.checksum = calculateChecksum(packet.command);

        if (transmitOnce(packet, peerMac) == TX_SUCCESS) {
            acked++;
        }
        if (progress) {
            progress(i + 1, acked);
        }

        // Rest des 250-ms-Slots abwarten (R-5)
        uint32_t elapsed = millis() - slotStart;
        if (i + 1 < pings && elapsed < intervalMs) {
            delay(intervalMs - elapsed);
        }
    }

    uint8_t quality = (uint8_t)(((uint16_t)acked * 100) / pings);
    DEBUG_PRINTF("Qualitaetstest: %u/%u = %u%%\n", acked, pings, quality);
    return quality;
}

//=============================================================================
// Statische ESP-NOW-Callbacks (laufen im WiFi-Task!)
//=============================================================================

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
void RadioManager::onSendStatic(const wifi_tx_info_t* /*info*/, esp_now_send_status_t status)
#else
void RadioManager::onSendStatic(const uint8_t* /*mac*/, esp_now_send_status_t status)
#endif
{
    if (!instance) return;
    instance->sendAcked = (status == ESP_NOW_SEND_SUCCESS);
    instance->sendResultPending = false;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
void RadioManager::onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len)
{
    const uint8_t* mac = info->src_addr;
#else
void RadioManager::onRecvStatic(const uint8_t* mac, const uint8_t* data, int len)
{
#endif
    if (!instance) return;

    // Validierung: Länge + magic + Checksumme (Regel 1, Constitution I)
    if (len != (int)sizeof(RadioPacketV3)) return;
    const RadioPacketV3* packet = (const RadioPacketV3*)data;
    if (!validatePacket(packet)) return;

    if (packet->type == FT_HELLO_ACK) {
        // MAC merken, Peer-Registrierung erfolgt in update() (Loop-Kontext)
        memcpy(instance->helloAckMac, mac, 6);
        instance->helloAckPending = true;
    }
    // FT_PING_ACK u. a.: keine Aktion nötig (Link-ACK genügt für die Qualität)
}
