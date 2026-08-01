/**
 * @file ConfigMenu.h
 * @brief Konfigurationsmenü für Turniereinstellungen (e-Paper, 2 Tasten)
 *
 * PORT aus V2 (Sender/ConfigMenu.h) — Logik des gelebten V2-Stands
 * ("Ändern"-los, Wrap-around):
 * - Zeile 0: Zeit (120s / 240s)        — CONFIG wechselt, OK bestätigt
 * - Zeile 1: Schützen (1-2 / 3-4)      — CONFIG wechselt, OK bestätigt
 * - Zeile 2: Start                     — CONFIG zurück zu Zeile 0 (Wrap),
 *                                        OK startet das Turnier
 *
 * Rendering ausschließlich über die EpaperDisplay-API (kein direkter
 * Panel-Zugriff): draw() zeichnet in den Puffer, die StateMachine entscheidet
 * über Voll-/Partial-Refresh.
 */

#pragma once

#include "Config.h"
#include "ButtonManager.h"
#include "EpaperDisplay.h"

class ConfigMenu {
public:
    /**
     * @brief Konstruktor
     * @param epd e-Paper-Wrapper
     * @param btnMgr ButtonManager-Referenz
     */
    ConfigMenu(EpaperDisplay& epd, ButtonManager& btnMgr);

    /**
     * @brief Initialisiert das Menü (Cursor auf Zeile 0, Flags zurück)
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen) — verarbeitet Tasten
     */
    void update();

    /**
     * @brief Zeichnet das komplette Menü in den Puffer (ohne Refresh!)
     */
    void draw();

    /**
     * @brief Prüft ob das Menü abgeschlossen wurde ("Start" bestätigt)
     */
    bool isComplete() const { return complete; }

    /**
     * @brief Prüft ob Display neu gezeichnet werden muss
     */
    bool needsRedraw() const { return needsUpdate; }

    uint16_t getShootingTime() const { return shootingTime; }
    uint8_t getShooterCount() const { return shooterCount; }

    /**
     * @brief Setzt Konfigurationswerte (aus NVS via ConfigStore)
     */
    void setConfig(uint16_t time, uint8_t count);

private:
    EpaperDisplay& epd;
    ButtonManager& buttons;

    // Konfigurationswerte
    uint16_t shootingTime;  // 120 oder 240 Sekunden
    uint8_t shooterCount;   // 2 (1-2 Schützen) oder 4 (3-4 Schützen)

    // UI-State
    uint8_t cursorLine;     // 0 = Zeit, 1 = Schützen, 2 = Start

    // Flags
    bool complete;          // true wenn "Start" bestätigt
    bool needsUpdate;       // true wenn Display neu gezeichnet werden muss
};
