/**
 * @file RadioManager.cpp
 * @brief Implementierung des ESP-NOW-Empfangs für den Empfänger
 */

#include "RadioManager.h"

#include <WiFi.h>
#include <esp_wifi.h>

RadioManager* RadioManager::instance = nullptr;

RadioManager::RadioManager()
    : initialized(false)
    , queueHead(0)
    , queueTail(0)
    , lastSeq(0)
    , lastSeqValid(false)
    , helloPending(false)
    , frameCounter(0) {
    memset(helloMac, 0, sizeof(helloMac));
}

bool RadioManager::begin() {
    instance = this;

    // WLAN reiner STA-Modus, fester Kanal 1 für ESP-NOW (R-3). Kein AP_STA/SoftAP
    // mehr: OTA läuft im separaten Wartungsmodus (Taster beim Boot), nie parallel
    // zu ESP-NOW (Single-Radio-Kanalkonflikt, FR-006).
    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(Radio::CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        DEBUG_PRINTLN("ESP-NOW init FAILED");
        return false;
    }

    esp_now_register_recv_cb(onRecvStatic);

    initialized = true;
    DEBUG_PRINTF("ESP-NOW init OK (Kanal %u)\n", Radio::CHANNEL);
    return true;
}

void RadioManager::update() {
    if (!initialized || !helloPending) return;

    helloPending = false;
    registerPeer(helloMac);
    sendHelloAck(helloMac);
}

RadioCommand RadioManager::nextCommand() {
    RadioCommand cmd = (RadioCommand)queue[queueTail];
    queueTail = (queueTail + 1) % QUEUE_SIZE;
    return cmd;
}

void RadioManager::registerPeer(const uint8_t* mac) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = Radio::CHANNEL;
    peer.encrypt = false;

    if (esp_now_is_peer_exist(mac)) {
        esp_now_mod_peer(&peer);
    } else {
        esp_now_add_peer(&peer);
    }
}

void RadioManager::sendHelloAck(const uint8_t* mac) {
    RadioPacketV3 packet;
    packet.magic0 = PACKET_MAGIC0;
    packet.magic1 = PACKET_MAGIC1;
    packet.type = FT_HELLO_ACK;
    packet.seq = 0;  // Sender dedupliziert nicht — seq hier ohne Bedeutung
    packet.command = 0x00;
    packet.checksum = calculateChecksum(packet.command);

    esp_now_send(mac, (const uint8_t*)&packet, sizeof(packet));
    DEBUG_PRINTF("HELLO_ACK an %02X:%02X:%02X:%02X:%02X:%02X\n",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

    // Validierung: Länge + magic + Checksumme (Regel 1, Constitution I)
    if (len != (int)sizeof(RadioPacketV3)) return;
    const RadioPacketV3* packet = (const RadioPacketV3*)data;
    if (!validatePacket(packet)) return;

    switch (packet->type) {
        case FT_HELLO:
            // Discovery: MAC merken, Antwort erfolgt in update() (Loop-Kontext).
            // Sender-Reboot setzt die Dedup-Historie zurück (neue zufällige seq).
            memcpy(instance->helloMac, mac, 6);
            instance->helloPending = true;
            instance->lastSeqValid = false;
            instance->frameCounter = instance->frameCounter + 1;
            break;

        case FT_CMD: {
            // Dedup: gleiche seq wie der letzte akzeptierte Frame → Retry, verwerfen
            if (instance->lastSeqValid && packet->seq == instance->lastSeq) {
                return;
            }
            instance->lastSeq = packet->seq;
            instance->lastSeqValid = true;
            instance->frameCounter = instance->frameCounter + 1;

            // Kommando in die Queue (voll → ältestes Kommando geht verloren;
            // bei 8 Plätzen und manueller Bedienung praktisch unmöglich)
            uint8_t nextHead = (instance->queueHead + 1) % QUEUE_SIZE;
            if (nextHead != instance->queueTail) {
                instance->queue[instance->queueHead] = packet->command;
                instance->queueHead = nextHead;
            }
            break;
        }

        case FT_PING:
            // Qualitätstest: keine Aktion, das Link-Layer-ACK genügt (Regel 5)
            instance->frameCounter = instance->frameCounter + 1;
            break;

        default:
            break;
    }
}
