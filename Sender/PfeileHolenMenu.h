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

    // Hilfsfunktionen für selective drawing
    void drawHeader();
    void drawOptions();
    void drawHelp();
    void drawConnectionIcon();
};
