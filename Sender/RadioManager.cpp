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
    , helloCount(0)
    , lastChannelCheckMs(0)
    , lastKnownChannel(0)
    , wakeWindowMs(0xFFFF)  // "noch nichts gesetzt" — erster setWakeWindow() greift
    , consecutiveFailures(0)
    , sendResultPending(false)
    , sendAcked(false)
    , helloAckPending(false) {
    memset(peerMac, 0, sizeof(peerMac));
    memset(helloAckMac, 0, sizeof(helloAckMac));
}

bool RadioManager::begin() {
    instance = this;

    // Reiner STA-Modus, fester Kanal (R-3). KEIN AP_STA/SoftAP: OTA läuft im
    // separaten Wartungsmodus (CONFIG+OK beim Boot), nie parallel zu ESP-NOW —
    // ein Radio, ein Kanal (identisch zum Empfänger, FR-006).
    WiFi.mode(WIFI_STA);

    // Auto-Reconnect aus und eine im NVS gespeicherte Verbindung trennen: sonst
    // zieht der STA-Teil den Kanal auf den eines fremden Routers und ESP-NOW
    // sendet ins Leere ("Peer channel is not equal to the home channel").
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);

    esp_wifi_set_channel(Radio::CHANNEL, WIFI_SECOND_CHAN_NONE);

    if (esp_now_init() != ESP_OK) {
        DEBUG_PRINTLN("ESP-NOW init FAILED");
        return false;
    }

    esp_now_register_send_cb(onSendStatic);
    esp_now_register_recv_cb(onRecvStatic);

    // Connectionless Power Save aufsetzen. Modem-Sleep explizit anfordern: der
    // Arduino-Core setzt WIFI_PS_MIN_MODEM zwar per Default, aber erst im
    // asynchronen STA_START-Event — darauf wollen wir uns hier nicht verlassen.
    // Ohne aktiven Power-Save-Modus sind Wake-Interval und -Window wirkungslos.
    esp_err_t psErr = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    esp_err_t ivErr = esp_wifi_connectionless_module_set_wake_interval(Radio::PS_WAKE_INTERVAL_MS);
    if (psErr != ESP_OK || ivErr != ESP_OK) {
        // Ohne diese beiden bleibt das Radio dauerhaft auf Empfang (~85 mA) —
        // das Gerät funktioniert, hält aber nur einen Bruchteil durch. Muss im
        // Log sichtbar sein, sonst sucht man den Verbrauch woanders.
        DEBUG_PRINTF("WARNUNG: Power Save nicht aktiv (set_ps=0x%x, wake_interval=0x%x)\n",
                     psErr, ivErr);
    }
    setWakeWindow(Radio::PS_WINDOW_OPEN_MS);  // bis der Empfänger antwortet

    // Broadcast-Peer für die Discovery registrieren
    if (!addOrModPeer(BROADCAST_MAC)) {
        DEBUG_PRINTLN("ESP-NOW broadcast peer FAILED");
        return false;
    }

    // Sequenznummer pro Boot zufällig initialisieren (R-4)
    seq = (uint8_t)esp_random();

    initialized = true;

    // Tatsächlich eingestellten Kanal zurücklesen — Soll und Ist können
    // auseinanderlaufen, und dann schweigt ESP-NOW ohne weitere Erklärung.
    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary, &second);
    lastKnownChannel = primary;
    DEBUG_PRINTF("ESP-NOW init OK (Kanal %u, Soll %u)\n", primary, Radio::CHANNEL);
    return true;
}

bool RadioManager::addOrModPeer(const uint8_t* mac) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = 0;  // 0 = aktueller Home-Channel (siehe Header)
    peer.encrypt = false;

    esp_err_t err = esp_now_is_peer_exist(mac) ? esp_now_mod_peer(&peer)
                                               : esp_now_add_peer(&peer);
    return err == ESP_OK;
}

void RadioManager::setWakeWindow(uint16_t windowMs) {
    if (wakeWindowMs == windowMs) return;
    wakeWindowMs = windowMs;

    esp_err_t err = esp_now_set_wake_window(windowMs);
    if (err != ESP_OK) {
        DEBUG_PRINTF("esp_now_set_wake_window(%u) fehlgeschlagen: 0x%x\n", windowMs, err);
        return;
    }
    DEBUG_PRINTF("Funk-Empfangsfenster: %u ms je %u ms%s\n", windowMs,
                 Radio::PS_WAKE_INTERVAL_MS,
                 windowMs == 0 ? " (nur noch Senden)" : "");
}

void RadioManager::dropPeer() {
    if (!peerDiscovered) return;

    DEBUG_PRINTF("Empfaenger antwortet nicht (%u Kommandos in Folge) — suche neu\n",
                 consecutiveFailures);

    esp_now_del_peer(peerMac);
    memset(peerMac, 0, sizeof(peerMac));
    peerDiscovered = false;
    consecutiveFailures = 0;

    // Discovery von vorn: schneller Raster, sofortiger erster Broadcast
    helloCount = 0;
    lastHelloMs = 0;

    // Ohne offenes Empfangsfenster käme das HELLO_ACK nie an. update() zieht es
    // nach HELLO_LISTEN_MS wieder zu.
    setWakeWindow(Radio::PS_WINDOW_OPEN_MS);
}

void RadioManager::enforceChannel() {
    uint32_t now = millis();
    if (now - lastChannelCheckMs < Radio::CHANNEL_CHECK_MS) return;
    lastChannelCheckMs = now;

    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return;
    if (primary == Radio::CHANNEL) {
        lastKnownChannel = primary;
        return;
    }

    // Nur bei echter Änderung loggen — sonst hätten wir den Spam nur ersetzt.
    if (primary != lastKnownChannel) {
        DEBUG_PRINTF("WLAN-Kanal war %u statt %u — korrigiere\n", primary, Radio::CHANNEL);
    }
    lastKnownChannel = primary;
    esp_wifi_set_channel(Radio::CHANNEL, WIFI_SECOND_CHAN_NONE);
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

    // Kanal-Wächter nur solange kein Empfänger gefunden ist: läuft die
    // Discovery, ist ein abgewanderter Kanal die wahrscheinlichste Ursache.
    enforceChannel();

    // FT_HELLO-Broadcast bis FT_HELLO_ACK (Regel 4); 1 Hz in der Anfangsphase,
    // danach im langsamen Raster (Empfänger vermutlich aus).
    uint32_t interval = (helloCount < Radio::HELLO_FAST_COUNT)
                            ? Radio::HELLO_INTERVAL_MS
                            : Radio::HELLO_SLOW_INTERVAL_MS;

    uint32_t now = millis();

    // Antwortfenster abgelaufen → Radio bis zum nächsten Broadcast schlafen
    // legen. Im langsamen Raster sind das 500 ms wach je 5 s statt dauerhaft.
    if (lastHelloMs != 0 && (now - lastHelloMs) >= Radio::HELLO_LISTEN_MS) {
        setWakeWindow(Radio::PS_WINDOW_CLOSED_MS);
    }

    if (now - lastHelloMs >= interval || lastHelloMs == 0) {
        // Fenster VOR dem Senden öffnen — sonst käme das HELLO_ACK, das der
        // Empfänger unmittelbar zurückschickt, ins geschlossene Radio.
        setWakeWindow(Radio::PS_WINDOW_OPEN_MS);
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

    // Nur die ersten Versuche protokollieren; danach bleibt die Suche still,
    // bis ein Empfänger antwortet (handleHelloAck loggt den Fund).
    if (helloCount < Radio::HELLO_LOG_COUNT) {
        DEBUG_PRINTLN("HELLO broadcast");
    } else if (helloCount == Radio::HELLO_LOG_COUNT) {
        DEBUG_PRINTF("Kein Empfaenger — Suche laeuft weiter alle %u ms (still)\n",
                     Radio::HELLO_SLOW_INTERVAL_MS);
    }
    if (helloCount < 0xFFFF) {
        helloCount++;
    }
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
    addOrModPeer(peerMac);

    peerDiscovered = true;
    helloCount = 0;  // Backoff für eine spätere Neusuche zurücksetzen
    consecutiveFailures = 0;
    DEBUG_PRINTF("Empfaenger gefunden: %02X:%02X:%02X:%02X:%02X:%02X\n",
                 peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5]);

    // Ab hier ist der Sender ein reiner Sender — Empfangsfenster zu.
    // Das 802.11-ACK auf eigene Frames bleibt davon unberührt, es kommt im
    // Sendefenster zurück. Merkt der Sender an ausbleibenden ACKs, dass der
    // Empfänger weg ist, macht dropPeer() das Fenster wieder auf.
    setWakeWindow(Radio::PS_WINDOW_CLOSED_MS);
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

void RadioManager::relinkIfExhausted() {
    if (consecutiveFailures >= Radio::RELINK_AFTER_FAILURES) {
        dropPeer();
    }
}

TransmissionResult RadioManager::sendCommand(RadioCommand cmd, bool allowRelink) {
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

    // Ausbleibende Link-ACKs sind das einzige Lebenszeichen, das dem Sender bei
    // geschlossenem Empfangsfenster bleibt. Bleiben sie mehrfach aus, ist der
    // Empfänger aus, außer Reichweite oder ausgetauscht → Discovery neu.
    if (result == TX_SUCCESS) {
        consecutiveFailures = 0;
    } else if (consecutiveFailures < 0xFF) {
        consecutiveFailures++;
        // Bei allowRelink=false bleibt der Peer stehen, damit die laufende
        // Sequenz ihre restlichen Versuche noch wirklich senden kann. Der
        // Aufrufer holt den Relink danach per relinkIfExhausted() nach.
        if (allowRelink && consecutiveFailures >= Radio::RELINK_AFTER_FAILURES) {
            dropPeer();
        }
    }

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
