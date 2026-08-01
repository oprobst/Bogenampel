/**
 * @file RadioManager.h
 * @brief ESP-NOW-Empfang für den Empfänger (Validierung, Dedup, Discovery-Antwort)
 *
 * Ersetzt das NRF24-Modul aus V2 durch ESP-NOW:
 * - Empfang mit Validierung (Länge, magic, Checksumme — Constitution I)
 * - Duplikat-Unterdrückung per Sequenznummer (FR-008, Retries idempotent)
 * - FT_HELLO → FT_HELLO_ACK unicast + Peer-Registrierung (Discovery, R-3)
 * - Kommandos werden in eine kleine Queue gelegt und im loop() abgeholt
 *   (der Recv-Callback läuft im WiFi-Task → volatile Ring-Puffer)
 *
 * Protokoll-Contract: specs/004-v3-esp32-port/contracts/espnow-protocol.md
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
     * @brief Initialisiert WLAN (STA, Kanal 1) und ESP-NOW, registriert Callbacks
     * @return true wenn erfolgreich, false bei esp_now-Fehler
     */
    bool begin();

    /**
     * @brief Beantwortet anstehende FT_HELLOs (muss regelmäßig in loop() laufen)
     *
     * Die Discovery-Antwort (Peer-Registrierung + FT_HELLO_ACK unicast) wird
     * bewusst im Loop-Kontext ausgeführt, nicht im WiFi-Task-Callback.
     */
    void update();

    /**
     * @brief Liegt ein empfangenes Kommando in der Queue?
     */
    bool commandAvailable() const { return queueHead != queueTail; }

    /**
     * @brief Holt das nächste Kommando aus der Queue
     * @return RadioCommand (nur gültig wenn commandAvailable() true war)
     */
    RadioCommand nextCommand();

    /**
     * @brief Zähler aller akzeptierten Frames (für Status-LED-Blink bei Empfang)
     */
    uint32_t framesReceived() const { return frameCounter; }

private:
    void sendHelloAck(const uint8_t* mac);
    void registerPeer(const uint8_t* mac);

    // ESP-NOW-Callbacks (Signaturen abhängig von der IDF-Version — Guards halten
    // den Code über arduino-esp32-Core-Updates der PIO-Platform hinweg baubar)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
#endif

    static RadioManager* instance;  // für statischen Callback (eine Instanz)

    bool initialized;

    // Kommando-Queue (Ring-Puffer, Schreiber = WiFi-Task, Leser = loop)
    static constexpr uint8_t QUEUE_SIZE = 8;  // Potenz von 2
    volatile uint8_t queue[QUEUE_SIZE];
    volatile uint8_t queueHead;  // Schreibposition (WiFi-Task)
    volatile uint8_t queueTail;  // Leseposition (loop)

    // Dedup: letzte akzeptierte Sequenznummer (ein Sender, Regel 2)
    volatile uint8_t lastSeq;
    volatile bool lastSeqValid;

    // Anstehende Discovery-Antwort (vom Callback gemeldet, in update() bedient)
    volatile bool helloPending;
    uint8_t helloMac[6];

    volatile uint32_t frameCounter;  // akzeptierte Frames (LED-Feedback)
};
