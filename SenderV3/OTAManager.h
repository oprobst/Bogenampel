/**
 * @file OTAManager.h
 * @brief OTA-Wartungsmodus des Senders: reine WiFi-Station + ArduinoOTA
 *
 * Wird NUR im Wartungsmodus benutzt (CONFIG + OK beim Einschalten gehalten).
 * Im Normalbetrieb läuft KEIN WiFi-Join und KEIN SoftAP — der ESP32 hat nur
 * ein Radio und damit genau einen Kanal; ein parallel betriebener Accesspoint
 * oder eine STA-Verbindung zieht den Kanal weg und legt ESP-NOW still.
 *
 * Wie beim Empfänger gibt es bewusst KEINEN SoftAP-Fallback: scheitert der
 * Verbindungsaufbau, bleibt das Gerät im Wartungsmodus und versucht es endlos
 * weiter. Es fällt nie in den Normalbetrieb zurück.
 *
 * Flash-Workflow:
 *   1. Gerät mit CONFIG + OK gleichzeitig einschalten
 *   2. IP vom Display ablesen, in platformio.ini als upload_port eintragen
 *   3. pio run -t upload -e sender-ota
 */

#pragma once

#include <Arduino.h>

namespace OTAManager {

    /**
     * @brief Anzeigephase des Wartungsmodus
     */
    enum class Phase : uint8_t {
        CONNECTING,  // sucht das WLAN / noch keine IP
        READY,       // verbunden + IP, wartet auf den Upload
        UPDATING     // Übertragung läuft
    };

    /**
     * @brief WiFi-Station starten und ArduinoOTA konfigurieren
     * @param onUpdateStart optionaler Callback, sobald der Upload beginnt
     *
     * Der Callback läuft im ArduinoOTA-Kontext — er darf kurz zeichnen, aber
     * nicht dauerhaft blockieren.
     */
    void begin(void (*onUpdateStart)() = nullptr);

    /**
     * @brief WLAN-Retry + OTA bedienen. In jedem loop()-Durchlauf aufrufen.
     *
     * ArduinoOTA.handle() wickelt einen laufenden Upload vollständig ab und
     * kehrt erst danach zurück — während des Transfers läuft die loop() nicht.
     */
    void handle();

    /**
     * @brief Aktuelle Phase für die Anzeige
     */
    Phase phase();

    /**
     * @brief IP-Adresse als Text ("" solange keine IP zugeteilt ist)
     */
    const char* ipString();

} // namespace OTAManager
