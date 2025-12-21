/**
 * @file PfeileHolenMenu.h
 * @brief Menü für "Pfeile holen" State
 *
 * Zeigt Optionen zwischen Passen:
 * - Nächste Passe starten
 * - Reihenfolge ändern
 * - Neustart
 */

#pragma once

#include <Adafruit_ST7789.h>
#include "Config.h"
#include "ButtonManager.h"

/**
 * @brief Aktionen die im Pfeile-Holen-Menü gewählt werden können
 */
enum class PfeileHolenAction : uint8_t {
    NONE = 0xFF,          // Keine Aktion gewählt
    NAECHSTE_PASSE = 0,   // Nächste Passe starten
    REIHENFOLGE = 1,      // Reihenfolge ändern
    NEUSTART = 2          // Zurück zur Konfiguration
};

/**
 * @brief Menü für "Pfeile holen" Pause zwischen Passen
 *
 * Verwaltet die UI-Logik für das Pause-Menü mit 3 Optionen.
 *
 * Usage:
 * @code
 * pfeileHolenMenu.begin();
 *
 * // In loop():
 * pfeileHolenMenu.update();
 * if (pfeileHolenMenu.needsRedraw()) {
 *     pfeileHolenMenu.draw();
 * }
 * if (pfeileHolenMenu.getSelectedAction() != PfeileHolenAction::NONE) {
 *     PfeileHolenAction action = pfeileHolenMenu.getSelectedAction();
 *     pfeileHolenMenu.resetAction();
 *     // ... handle action
 * }
 * @endcode
 */
class PfeileHolenMenu {
public:
    /**
     * @brief Konstruktor
     * @param tft Display-Referenz
     * @param btnMgr ButtonManager-Referenz
     */
    PfeileHolenMenu(Adafruit_ST7789& tft, ButtonManager& btnMgr);

    /**
     * @brief Initialisiert das Menü
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     */
    void update();

    /**
     * @brief Zeichnet das komplette Menü
     */
    void draw();

    /**
     * @brief Prüft ob Display neu gezeichnet werden muss
     * @return true wenn draw() aufgerufen werden sollte
     */
    bool needsRedraw() const { return needsUpdate; }

    /**
     * @brief Holt die gewählte Aktion
     * @return Gewählte Aktion oder NONE
     */
    PfeileHolenAction getSelectedAction() const { return selectedAction; }

    /**
     * @brief Setzt die gewählte Aktion zurück
     */
    void resetAction() { selectedAction = PfeileHolenAction::NONE; }

    /**
     * @brief Aktualisiert den Verbindungsstatus zum Empfänger
     * @param isConnected true wenn Empfänger erreichbar, false sonst
     */
    void updateConnectionStatus(bool isConnected);

    /**
     * @brief Aktualisiert den Batteriestatus
     * @param voltageMillivolts Batteriespannung in Millivolt
     * @param usbPowered true wenn USB angeschlossen, false wenn Batteriebetrieb
     */
    void updateBatteryStatus(uint16_t voltageMillivolts, bool usbPowered);

    /**
     * @brief Setzt die Turnierkonfiguration
     * @param shooters Anzahl Schützen (2 oder 4)
     * @param group Aktuelle Gruppe (GROUP_AB oder GROUP_CD)
     * @param position Aktuelle Position (POS_1 oder POS_2)
     */
    void setTournamentConfig(uint8_t shooters, Groups::Type group, Groups::Position position);

private:
    Adafruit_ST7789& display;
    ButtonManager& buttons;

    // UI-State
    uint8_t cursorPosition;   // 0 = Nächste Passe, 1 = Reihenfolge, 2 = Neustart
    PfeileHolenAction selectedAction;  // Gewählte Aktion

    // Flags
    bool needsUpdate;
    bool firstDraw;

    // Vorherige Werte für selective redraw
    uint8_t lastCursorPosition;

    // Verbindungsstatus
    bool connectionOk;
    bool lastConnectionOk;

    // Ping-Historie für Empfangsstärke-Anzeige (letzte 4 Pings)
    bool pingHistory[4];      // true = ACK empfangen, false = kein ACK
    uint8_t pingHistoryIndex; // Ring-Buffer Index (0-3)
    bool pingHistoryUpdated;  // Flag: Ping-Historie wurde aktualisiert

    // Batteriestatus
    uint16_t batteryVoltage;  // Spannung in Millivolt
    bool isUsbPowered;        // true wenn USB, false wenn Batterie
    bool batteryUpdated;      // Flag: Batteriestatus wurde aktualisiert

    // Turnierkonfiguration
    uint8_t shooterCount;           // 2 (1-2 Schützen) oder 4 (3-4 Schützen)
    Groups::Type currentGroup;      // Aktuelle Gruppe (GROUP_AB oder GROUP_CD)
    Groups::Position currentPosition; // Aktuelle Position (POS_1 oder POS_2)
    bool groupConfigChanged;        // Flag: Gruppe/Position wurde geändert

    // Hilfsfunktionen für selective drawing
    void drawHeader();
    void drawOptions();
    void drawHelp();
    void drawConnectionIcon();
    void drawBatteryIcon();       // Zeigt Batteriestatus
    void drawShooterGroupInfo();  // Zeigt Schützengruppen bei 3-4 Schützen
};
