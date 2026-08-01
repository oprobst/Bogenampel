/**
 * @file PfeileHolenMenu.h
 * @brief Menü für "Pfeile holen" State (e-Paper, 2 Tasten)
 *
 * PORT aus V2 (Sender/PfeileHolenMenu.h):
 * - Optionen: Nächste Passe / Abfolge (nur 3-4 Schützen) / Neustart
 * - CONFIG: Cursor weiter (Wrap-around), OK: Aktion ausführen
 * - Verbindungs-/Batteriestatus wandert in V3 in die Statuszeile (T023)
 */

#pragma once

#include "Config.h"
#include "ButtonManager.h"
#include "EpaperDisplay.h"

/**
 * @brief Aktionen die im Pfeile-Holen-Menü gewählt werden können
 */
enum class PfeileHolenAction : uint8_t {
    NONE = 0xFF,          // Keine Aktion gewählt
    NAECHSTE_PASSE = 0,   // Nächste Passe starten (ganz/halb je nach Position)
    REIHENFOLGE = 1,      // Reihenfolge ändern
    NEUSTART = 2          // Zurück zur Konfiguration
};

class PfeileHolenMenu {
public:
    PfeileHolenMenu(EpaperDisplay& epd, ButtonManager& btnMgr);

    /**
     * @brief Initialisiert das Menü (inkl. MENU_LOCKOUT gegen Tastenprellen)
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     */
    void update();

    /**
     * @brief Zeichnet das komplette Menü in den Puffer (ohne Refresh!)
     */
    void draw();

    bool needsRedraw() const { return needsUpdate; }

    PfeileHolenAction getSelectedAction() const { return selectedAction; }
    void resetAction() { selectedAction = PfeileHolenAction::NONE; }

    /**
     * @brief Setzt die Turnierkonfiguration (Schützenzahl, Gruppe, Position)
     */
    void setTournamentConfig(uint8_t shooters, Groups::Type group, Groups::Position position);

private:
    EpaperDisplay& epd;
    ButtonManager& buttons;

    // UI-State
    uint8_t cursorPosition;            // 0 = Nächste Passe, 1 = Abfolge, 2 = Neustart
    PfeileHolenAction selectedAction;

    // Flags
    bool needsUpdate;
    uint32_t buttonLockoutUntil;  // Eingaben ignorieren bis zu diesem Zeitpunkt

    // Turnierkonfiguration
    uint8_t shooterCount;              // 2 oder 4
    Groups::Type currentGroup;
    Groups::Position currentPosition;

    void drawOptionButton(const char* label, int16_t y, bool selected);
    void drawShooterGroupInfo();
};
