/**
 * @file SchiessBetriebMenu.h
 * @brief Menü für "Schießbetrieb" State (e-Paper, Countdown per Partial-Refresh)
 *
 * PORT aus V2 (Sender/SchiessBetriebMenu.h), erweitert um den 1-Hz-Countdown
 * im Partial-Fenster (SC-007 — kein Vollbild-Blitzen):
 * - Vorbereitungsphase (10 s, Countdown in Sekunden)
 * - Schießphase (120/240 s, Countdown als Sekundensumme "240s")
 * - Gruppenanzeige bei 3-4 Schützen
 * - OK kurz = "Passe beenden"; 3x Klick = Alarm und Halten = Aus wertet die
 *   StateMachine aus (Feature 006)
 */

#pragma once

#include "Config.h"
#include "ButtonManager.h"
#include "EpaperDisplay.h"

class SchiessBetriebMenu {
public:
    SchiessBetriebMenu(EpaperDisplay& epd, ButtonManager& btnMgr);

    /**
     * @brief Initialisiert das Menü
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen) — OK kurz = Passe beenden
     */
    void update();

    /**
     * @brief Zeichnet den kompletten Screen in den Puffer (ohne Refresh!)
     */
    void draw();

    bool needsRedraw() const { return needsUpdate; }

    /**
     * @brief Setzt die Turnierkonfiguration
     */
    void setTournamentConfig(uint16_t shootingTime, uint8_t shooterCount,
                             Groups::Type group, Groups::Position position);

    /**
     * @brief Setzt die Vorbereitungsphase (orange Phase in V2)
     */
    void setPreparationPhase(bool inPrep, uint32_t remainingMs);

    /**
     * @brief Wechselt in die Schießphase
     */
    void setShootingPhase(uint32_t remainingMs);

    /**
     * @brief 1-Hz-Countdown: zeichnet die Restzeit ins Countdown-Fenster und
     *        stößt den Partial-Refresh an (R-2, ~300-400 ms)
     * @param remainingSec verbleibende Sekunden der aktuellen Phase
     */
    void updateCountdown(uint32_t remainingSec);

    bool isEndRequested() const { return endRequested; }
    void resetEndRequest() { endRequested = false; }

private:
    EpaperDisplay& epd;
    ButtonManager& buttons;

    // Turnierkonfiguration
    uint16_t shootingTime;             // 120 oder 240 Sekunden
    uint8_t shooterCount;              // 2 oder 4
    Groups::Type currentGroup;
    Groups::Position currentPosition;

    // Timer-Zustand
    bool inPreparationPhase;           // true = Vorbereitung, false = Schießphase
    uint32_t lastDrawnSec;             // zuletzt gezeichnete Restzeit

    // UI-State
    bool needsUpdate;
    bool endRequested;                 // "Passe beenden" (OK kurz)

    /**
     * @brief Restzeit ins Countdown-Fenster zeichnen (nur Puffer)
     */
    void drawCountdownValue(uint32_t remainingSec);
};
