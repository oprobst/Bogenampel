/**
 * @file SchiessBetriebMenu.h
 * @brief Menü für "Schießbetrieb" State
 *
 * Zeigt Schießbetrieb-UI mit:
 * - Vorbereitungsphase (10s, orange Countdown)
 * - Schießphase (120/240s, grüner Countdown)
 * - Gruppensequenz-Anzeige (bei 3-4 Schützen)
 * - "Passe beenden" Button
 */

#pragma once

#include <Adafruit_ST7789.h>
#include "Config.h"
#include "ButtonManager.h"

/**
 * @brief Menü für Schießbetrieb (aktive Schießphase)
 *
 * Verwaltet die UI-Logik für den Schießbetrieb-State mit:
 * - Vorbereitungsphase (10s, orange)
 * - Schießphase (120/240s, grün)
 * - Gruppenanzeige bei 3-4 Schützen
 * - Button "Passe beenden"
 *
 * Usage:
 * @code
 * schiessBetriebMenu.begin();
 * schiessBetriebMenu.setTournamentConfig(120, 4, Groups::GROUP_AB, Groups::POS_1);
 * schiessBetriebMenu.setPreparationPhase(true, 10000);
 *
 * // In loop():
 * schiessBetriebMenu.update();
 * if (schiessBetriebMenu.needsRedraw()) {
 *     schiessBetriebMenu.draw();
 * }
 * if (schiessBetriebMenu.isEndRequested()) {
 *     schiessBetriebMenu.resetEndRequest();
 *     // ... handle end request
 * }
 * @endcode
 */
class SchiessBetriebMenu {
public:
    /**
     * @brief Konstruktor
     * @param tft Display-Referenz
     * @param btnMgr ButtonManager-Referenz
     */
    SchiessBetriebMenu(Adafruit_ST7789& tft, ButtonManager& btnMgr);

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
     * @brief Setzt die Turnierkonfiguration
     * @param shootingTime Schießzeit (120 oder 240 Sekunden)
     * @param shooterCount Anzahl Schützen (2 oder 4)
     * @param group Aktuelle Gruppe (GROUP_AB oder GROUP_CD)
     * @param position Aktuelle Position (POS_1 oder POS_2)
     */
    void setTournamentConfig(uint8_t shootingTime, uint8_t shooterCount,
                            Groups::Type group, Groups::Position position);

    /**
     * @brief Setzt die Vorbereitungsphase
     * @param inPrep true wenn in Vorbereitung, false wenn in Schießphase
     * @param remainingMs Verbleibende Zeit in Millisekunden
     */
    void setPreparationPhase(bool inPrep, uint32_t remainingMs);

    /**
     * @brief Setzt die Schießphase
     * @param remainingMs Verbleibende Zeit in Millisekunden
     */
    void setShootingPhase(uint32_t remainingMs);

    /**
     * @brief Prüft ob "Passe beenden" gedrückt wurde
     * @return true wenn Button gedrückt wurde
     */
    bool isEndRequested() const { return endRequested; }

    /**
     * @brief Setzt das "Passe beenden" Flag zurück
     */
    void resetEndRequest() { endRequested = false; }

private:
    Adafruit_ST7789& display;
    ButtonManager& buttons;

    // Turnierkonfiguration
    uint8_t shootingTime;    // 120 oder 240 Sekunden
    uint8_t shooterCount;    // 2 (1-2 Schützen) oder 4 (3-4 Schützen)
    Groups::Type currentGroup;      // Aktuelle Gruppe (GROUP_AB oder GROUP_CD)
    Groups::Position currentPosition; // Aktuelle Position (POS_1 oder POS_2)

    // Timer-Zustand
    bool inPreparationPhase;  // true = Vorbereitung (10s), false = Schießphase (120/240s)
    uint16_t remainingSec;    // Verbleibende Sekunden
    uint16_t lastRemainingSec; // Letzte gezeichnete Zeit (für selective redraw)

    // UI-State
    bool needsUpdate;
    bool firstDraw;
    bool endRequested;  // "Passe beenden" Button gedrückt

    // Selective Drawing Helper
    void drawHeader();
    void drawGroupSequence();
    void drawTimer();
    void drawEndButton();
    void drawHelp();
    void updateTimer();  // Nur Timer aktualisieren (selective redraw)
};
