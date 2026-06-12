/**
 * @file RadioManager.h
 * @brief ESP-NOW-Transport für den Sender (Discovery, Kommandos, Qualitätstest)
 *
 * Ersetzt das NRF24-Modul aus V2 durch ESP-NOW (integriertes 2,4-GHz-Funk):
 * - Discovery: FT_HELLO-Broadcast (max. 1 Hz) bis FT_HELLO_ACK vom Empfänger,
 *   dann ausschließlich Unicast an die gelernte Empfänger-MAC (R-3)
 * - sendCommand(): V2-kompatible API mit Link-Layer-ACK über den Send-Callback,
 *   bis zu 3 Transport-Retries à 50 ms (Gesamtbudget < 500 ms, FR-007)
 * - pingQualityTest(): 10 Pings im 250-ms-Raster für den Splash (FR-009, R-5)
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
     * @brief Treibt die Discovery voran (FT_HELLO-Broadcast, max. 1 Hz bis ACK)
     *
     * Muss regelmäßig in loop() aufgerufen werden, solange kein Empfänger
     * gefunden wurde. Nach Empfang des FT_HELLO_ACK ist die Empfänger-MAC
     * als Peer registriert; weitere Aufrufe sind no-ops.
     */
    void update();

    /**
     * @brief Wurde ein Empfänger per Discovery gefunden?
     */
    bool isPeerDiscovered() const { return peerDiscovered; }

    /**
     * @brief Sendet ein Kommando an den Empfänger (V2-kompatible API)
     * @param cmd RadioCommand (11 Kommandos)
     * @return TX_SUCCESS (Link-ACK), TX_TIMEOUT (alle Retries NACK) oder TX_ERROR
     *
     * Transport-Retries (max. 3 Versuche, 50 ms Abstand) verwenden dieselbe
     * Sequenznummer — der Empfänger dedupliziert per seq (FR-008).
     */
    TransmissionResult sendCommand(RadioCommand cmd);

    /**
     * @brief Verbindungsqualitätstest für den Splash (FR-009)
     * @param pings Anzahl der FT_PING-Frames (Default: 10)
     * @param intervalMs Raster zwischen den Pings (Default: 250 ms)
     * @param progress optionaler Callback pro Ping (done = gesendet, ok = bestätigt)
     * @return Qualität in Prozent (Anteil per Send-Callback bestätigter Frames)
     *
     * Blockiert für pings × intervalMs (2,5 s bei Defaults). Ohne Discovery
     * (kein Empfänger) wird 0 zurückgegeben, ohne zu senden.
     */
    uint8_t pingQualityTest(uint8_t pings = Radio::QUALITY_TEST_PINGS,
                            uint16_t intervalMs = Radio::QUALITY_TEST_INTERVAL_MS,
                            void (*progress)(uint8_t done, uint8_t ok) = nullptr);

private:
    /**
     * @brief Sendet ein Paket und wartet auf den Send-Callback
     * @return TX_SUCCESS / TX_TIMEOUT / TX_ERROR (einzelner Versuch, kein Retry)
     */
    TransmissionResult transmitOnce(const RadioPacketV3& packet, const uint8_t* mac);

    void sendHello();
    void handleHelloAck(const uint8_t* mac);

    // ESP-NOW-Callbacks (Signaturen abhängig von der IDF-Version — Guards halten
    // den Code über arduino-esp32-Core-Updates der PIO-Platform hinweg baubar)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    static void onSendStatic(const wifi_tx_info_t* info, esp_now_send_status_t status);
#else
    static void onSendStatic(const uint8_t* mac, esp_now_send_status_t status);
#endif
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    static void onRecvStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);
#else
    static void onRecvStatic(const uint8_t* mac, const uint8_t* data, int len);
#endif

    static RadioManager* instance;  // für statische Callbacks (eine Instanz)

    uint8_t peerMac[6];             // gelernte Empfänger-MAC (nach HELLO_ACK)
    bool peerDiscovered;            // FT_HELLO_ACK empfangen, Peer registriert
    bool initialized;               // esp_now_init() erfolgreich

    uint8_t seq;                    // Sequenznummer (pro Boot zufällig, ++ pro Kommando)
    uint32_t lastHelloMs;           // Zeitpunkt des letzten HELLO-Broadcasts

    // Send-Callback-Synchronisation (Callback läuft im WiFi-Task)
    volatile bool sendResultPending;
    volatile bool sendAcked;

    // Vom Recv-Callback gemeldetes HELLO_ACK (Verarbeitung in update()/Wartepfad)
    volatile bool helloAckPending;
    uint8_t helloAckMac[6];
};
