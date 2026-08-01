/**
 * @file OtaScreen.h
 * @brief Anzeige des OTA-Wartungsmodus auf dem e-Paper
 *
 * Zeigt WLAN-Name, Verbindungszustand und die eigene IP-Adresse, damit der
 * upload_port ohne Router-Suche ablesbar ist.
 *
 * e-Paper-Ökonomie: Der statische Teil (Titel, Beschriftungen, Hostname) wird
 * einmal mit einem Voll-Refresh geschrieben. Danach aktualisiert update() nur
 * das Statusfenster, und auch das nur bei echter Zustandsänderung — ein
 * zyklisches Neuzeichnen würde das Panel unnötig altern lassen.
 */

#pragma once

#include "EpaperDisplay.h"
#include "OTAManager.h"

class OtaScreen {
public:
    explicit OtaScreen(EpaperDisplay& epdRef);

    /**
     * @brief Zeichnet den kompletten Screen (Voll-Refresh, einmal beim Start)
     */
    void draw();

    /**
     * @brief Aktualisiert das Statusfenster, wenn sich etwas geändert hat
     *
     * In loop() aufrufen. Ohne Änderung passiert nichts (kein Panel-Zugriff).
     */
    void update();

    /**
     * @brief Zeigt sofort „Update laeuft" — Callback für OTAManager::begin()
     *
     * Wird aus dem ArduinoOTA-onStart aufgerufen, weil die loop() während des
     * Transfers nicht mehr drankommt.
     */
    void showUpdating();

private:
    /**
     * @brief Übernimmt den aktuellen OTA-Zustand in den Anzeige-Cache
     *
     * Muss VOR dem Zeichnen laufen: Ein Voll-Refresh dauert ~2,6 s, und in
     * dieser Zeit kann die WLAN-Verbindung zustande kommen. Würde der Cache
     * erst danach gefüllt, stünde dort READY, während auf dem Panel noch
     * "Verbinde mit WLAN" steht — update() sähe keinen Unterschied mehr und
     * die Anzeige bliebe dauerhaft falsch.
     */
    void captureState();

    /**
     * @brief Zeichnet den dynamischen Bereich aus dem Cache in den Puffer
     *
     * Bewusst aus lastPhase/lastIp statt live aus dem OTAManager — nur so ist
     * garantiert, dass das Panel zeigt, was der Cache behauptet.
     */
    void drawStatus();

    EpaperDisplay& epd;

    // Anzeige-Cache: Zustand, der aktuell auf dem Panel steht
    OTAManager::Phase lastPhase;
    char lastIp[16];
    bool drawn;          // draw() bereits gelaufen?

    // Dynamisches Fenster (unterhalb der Beschriftungen, oberhalb des Hostnamens)
    static constexpr uint16_t STATUS_Y = 62;
    static constexpr uint16_t STATUS_H = 108;
};
