/**
 * @file AudioManager.h
 * @brief Bluetooth-Ausgabe der Brücke: A2DP-Source + Tonerzeugung
 *
 * Die Töne werden direkt im A2DP-Datencallback synthetisiert (Sinus aus einer
 * Tabelle, 44,1 kHz Stereo) — es gibt keine Audiodateien und keinen Puffer.
 * Die Folge entspricht BuzzerManager im Empfänger: N × (500 ms Ton, 500 ms
 * Pause); ein neues Signal ersetzt eine laufende Folge.
 *
 * Die Box wird über ESP32-A2DP (pschatzmann) angesprochen. Die Bibliothek merkt
 * sich die zuletzt verbundene Adresse selbst im NVS und verbindet beim nächsten
 * Start direkt neu. Eine neue Box wird im Suchmodus gewählt (begin(true)):
 * gefundene Geräte landen in einer Liste, die Auswahl wird beim nächsten
 * Treffer der laufenden Discovery verbunden.
 *
 * Zur Latenz: A2DP puffert in der Box, typisch 150-250 ms bis zum Ton.
 */

#pragma once

#include <Arduino.h>
#include "Config.h"

class AudioManager {
public:
    enum class State : uint8_t {
        STARTING,    // Bluetooth-Stack fährt hoch (die Bibliothek wartet ~10 s)
        SEARCHING,   // Discovery läuft (Suchmodus oder keine Box bekannt)
        CONNECTING,  // Verbindungsaufbau / Wiederverbinden zur bekannten Box
        CONNECTED
    };

    /**
     * @brief Startet Bluetooth als A2DP-Source
     * @param scanMode true = nicht automatisch neu verbinden, Geräte sammeln
     */
    void begin(bool scanMode);

    /// Muss in jedem loop() laufen (setzt die AFH-Kanalsperre, siehe Radio in Config.h)
    void update();

    /// Zustand für die Anzeige
    State state() const;

    /// Spielt N Töne (ersetzt eine laufende Folge)
    void playSignal(uint8_t count);

    /// Kurzer Vorhörton (nur wenn gerade keine Signalfolge läuft)
    void preview();

    /// Lautstärke in Stufen 0..Audio::VOLUME_STEPS
    void setVolume(uint8_t step);

    // --- Suchmodus -----------------------------------------------------------

    /// Ist eine Box-Adresse im NVS gespeichert? (Vor begin() aufrufbar, NVS muss laufen)
    bool hasStoredBox();

    /// Adresse der gewählten Box dauerhaft speichern (nach erfolgreicher Verbindung)
    void rememberSelectedBox();

    /// Anzahl bisher gefundener Geräte
    uint8_t deviceCount() const;

    /// Name eines gefundenen Geräts (in buf, max. len-1 Zeichen)
    void deviceName(uint8_t index, char* buf, size_t len) const;

    /// Gerät auswählen: wird beim nächsten Discovery-Treffer verbunden
    void selectDevice(uint8_t index);

    /// Wurde ein Gerät gewählt?
    bool hasSelection() const { return selectionValid; }

    // Aufrufe aus den Bibliotheks-Callbacks (BT-Task), nicht aus loop()
    bool handleDiscovered(const char* name, const uint8_t* address);
    bool takeRequest(uint8_t& count);
    int16_t currentAmplitude() const { return amplitude; }

private:
    struct Device {
        char name[33];
        uint8_t address[6];
    };

    bool scanning = false;
    Device devices[Ui::SCAN_MAX_DEVICES];
    volatile uint8_t devicesFound = 0;
    uint8_t selectedAddress[6] = {0};
    volatile bool selectionValid = false;
    mutable portMUX_TYPE deviceLock = portMUX_INITIALIZER_UNLOCKED;

    // Übergabe loop → BT-Task (loop und BT-Task laufen auf verschiedenen Kernen)
    portMUX_TYPE requestLock = portMUX_INITIALIZER_UNLOCKED;
    bool requestPending = false;
    uint8_t requestCount = 0;  // 0 = Vorhörton
    uint32_t requestMs = 0;
    volatile int16_t amplitude = 0;
};
