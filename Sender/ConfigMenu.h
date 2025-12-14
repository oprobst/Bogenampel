/**
 * @file ConfigMenu.h
 * @brief Konfigurationsmenü für Turniereinstellungen
 *
 * Ermöglicht die Auswahl von:
 * - Schießzeit (120s oder 240s)
 * - Schützenanzahl (1-2 oder 3-4)
 *
 * Navigation:
 * - LEFT/RIGHT: Werte ändern, zwischen Buttons wechseln
 * - OK: Bestätigen und zur nächsten Zeile / Menü beenden
 */

#pragma once

#include <Adafruit_ST7789.h>
#include "Config.h"
#include "ButtonManager.h"

/**
 * @brief Konfigurationsmenü für Turniereinstellungen
 *
 * Verwaltet die UI-Logik für das Konfigurationsmenü mit 3 Zeilen:
 * - Zeile 0: Zeit (120s / 240s)
 * - Zeile 1: Schützenanzahl (1-2 / 3-4)
 * - Zeile 2: Buttons (Ändern / Start)
 *
 * Usage:
 * @code
 * configMenu.begin();
 *
 * // In loop():
 * configMenu.update();
 * if (configMenu.needsRedraw()) {
 *     configMenu.draw();
 * }
 * if (configMenu.isComplete()) {
 *     uint8_t time = configMenu.getShootingTime();
 *     uint8_t count = configMenu.getShooterCount();
 *     // ... starte Turnier
 * }
 * @endcode
 */
class ConfigMenu {
public:
    /**
     * @brief Konstruktor
     * @param tft Display-Referenz
     * @param btnMgr ButtonManager-Referenz
     */
    ConfigMenu(Adafruit_ST7789& tft, ButtonManager& btnMgr);

    /**
     * @brief Initialisiert das Menü
     *
     * Setzt alle Werte auf Default und markiert Redraw als nötig.
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     *
     * Verarbeitet Button-Inputs und aktualisiert den Menü-State.
     * Setzt needsUpdate-Flag wenn Display neu gezeichnet werden muss.
     */
    void update();

    /**
     * @brief Zeichnet das komplette Menü
     *
     * Sollte aufgerufen werden wenn needsRedraw() == true.
     * Setzt needsUpdate-Flag zurück.
     */
    void draw();

    /**
     * @brief Prüft ob das Menü abgeschlossen wurde
     * @return true wenn "Start" Button bestätigt wurde
     */
    bool isComplete() const { return complete; }

    /**
     * @brief Prüft ob "Ändern" gewählt wurde (zurück zu Zeile 0)
     * @return true wenn zurück navigiert werden soll
     */
    bool needsChange() const { return changeRequested; }

    /**
     * @brief Prüft ob Display neu gezeichnet werden muss
     * @return true wenn draw() aufgerufen werden sollte
     */
    bool needsRedraw() const { return needsUpdate; }

    /**
     * @brief Holt die konfigurierte Schießzeit
     * @return 120 oder 240 (Sekunden)
     */
    uint8_t getShootingTime() const { return shootingTime; }

    /**
     * @brief Holt die konfigurierte Schützenanzahl
     * @return 2 (1-2 Schützen) oder 4 (3-4 Schützen)
     */
    uint8_t getShooterCount() const { return shooterCount; }

    /**
     * @brief Setzt Konfigurationswerte (z.B. aus EEPROM)
     * @param time Schießzeit (120 oder 240)
     * @param count Schützenanzahl (2 oder 4)
     */
    void setConfig(uint8_t time, uint8_t count);

private:
    Adafruit_ST7789& display;
    ButtonManager& buttons;

    // Konfigurationswerte
    uint8_t shootingTime;   // 120 oder 240 Sekunden
    uint8_t shooterCount;   // 2 (1-2 Schützen) oder 4 (3-4 Schützen)

    // UI-State
    uint8_t cursorLine;       // 0 = Zeit, 1 = Schützen, 2 = Buttons
    uint8_t selectedButton;   // 0 = "Ändern", 1 = "Start"

    // Flags
    bool complete;            // true wenn "Start" bestätigt
    bool changeRequested;     // true wenn "Ändern" bestätigt
    bool needsUpdate;         // true wenn Display neu gezeichnet werden muss
    bool firstDraw;           // true beim ersten Zeichnen

    // Vorherige Werte für selective redraw
    uint8_t lastShootingTime;
    uint8_t lastShooterCount;
    uint8_t lastCursorLine;
    uint8_t lastSelectedButton;

    // Hilfsfunktionen für selective drawing
    void drawHeader();
    void drawTimeOption();
    void drawShooterOption();
    void drawButtonOption();
    void drawHelp();
};
