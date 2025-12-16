/**
 * @file StateMachine.h
 * @brief Haupt-State-Machine für Bogenampel Sender
 */

#pragma once

#include <Adafruit_ST7789.h>
#include "Config.h"
#include "ButtonManager.h"
#include "ConfigMenu.h"
#include "SplashScreen.h"
#include "PfeileHolenMenu.h"

/**
 * @brief System-Zustände (Tournament State Machine)
 */
enum class State : uint8_t {
    STATE_SPLASH,          // Zeigt Splash Screen (3 Sekunden, keine Buttons)
    STATE_CONFIG_MENU,     // Konfigurationsmenü (Zeit, Schützenanzahl)
    STATE_PFEILE_HOLEN,    // Turniermodus: Pfeile holen (Pause zwischen Passen)
    STATE_SCHIESS_BETRIEB  // Turniermodus: Schießbetrieb aktiv
};

/**
 * @brief State Machine für Sender-Logik (Tournament Control)
 *
 * Verwaltet Systemzustände und Übergänge:
 * STATE_SPLASH → STATE_CONFIG_MENU → STATE_PFEILE_HOLEN ⇄ STATE_SCHIESS_BETRIEB
 *                      ↑                      ↓
 *                      └──────────────────────┘ (Neustart)
 *
 * Features:
 * - Konfigurationsmenü (Zeit: 120/240s, Schützen: 1-2/3-4)
 * - Turniermodus-Steuerung (Pfeile holen ⇄ Schießbetrieb)
 * - Konfiguration wird bei jedem Start neu eingestellt
 * - Alarm-Detection (OK > 3s in jedem State)
 */
class StateMachine {
public:
    StateMachine(Adafruit_ST7789& tft, ButtonManager& btnMgr);

    /**
     * @brief Initialisiert die State Machine
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     */
    void update();

    /**
     * @brief Setzt den Radio-Initialisierungsstatus
     * @param initialized true wenn NRF24L01 Modul gefunden wurde
     */
    void setRadioInitialized(bool initialized);

    /**
     * @brief Aktuellen Zustand abfragen
     */
    State getCurrentState() const { return currentState; }

    /**
     * @brief Manueller Zustandswechsel (für Debugging)
     */
    void setState(State newState);

    /**
     * @brief Turnierkonfiguration abrufen (shootingTime)
     */
    uint8_t getShootingTime() const { return shootingTime; }

    /**
     * @brief Turnierkonfiguration abrufen (shooterCount)
     */
    uint8_t getShooterCount() const { return shooterCount; }

private:
    Adafruit_ST7789& display;
    ButtonManager& buttons;
    SplashScreen splashScreen;      // Splash Screen (nur UI)
    ConfigMenu configMenu;          // Konfigurationsmenü
    PfeileHolenMenu pfeileHolenMenu; // Pfeile-Holen-Menü
    State currentState;
    State previousState;
    uint32_t stateStartTime;  // Zeitstempel beim Zustandswechsel

    //-------------------------------------------------------------------------
    // Turnierkonfiguration (wird bei jedem Start im Menü eingestellt)
    //-------------------------------------------------------------------------
    uint8_t shootingTime;   // 120 oder 240 Sekunden
    uint8_t shooterCount;   // 2 (1-2 Schützen) oder 4 (3-4 Schützen)

    //-------------------------------------------------------------------------
    // State Variables: SPLASH
    //-------------------------------------------------------------------------
    bool radioInitialized;      // NRF24L01 Modul gefunden?
    bool connectionTested;      // Verbindungstest durchgeführt?
    bool connectionSuccessful;  // Empfänger gefunden?

    //-------------------------------------------------------------------------
    // State Variables: PFEILE_HOLEN
    //-------------------------------------------------------------------------
    uint32_t lastConnectionCheck;  // Zeitpunkt der letzten Verbindungsprüfung

    //-------------------------------------------------------------------------
    // Schützengruppen-Tracking (für 3-4 Schützen Modus)
    //-------------------------------------------------------------------------
    Groups::Type currentGroup;      // Aktuelle Gruppe (GROUP_AB oder GROUP_CD)
    Groups::Position currentPosition; // Aktuelle Position (POS_1 oder POS_2)

    //-------------------------------------------------------------------------
    // State Variables: SCHIESS_BETRIEB
    //-------------------------------------------------------------------------
    uint32_t shootingStartTime;     // Zeitpunkt des Schießbetrieb-Starts (millis)
    uint32_t shootingDurationMs;    // Dauer in Millisekunden (120000 oder 240000)

    //-------------------------------------------------------------------------
    // State Handlers
    //-------------------------------------------------------------------------
    void handleSplash();
    void handleConfigMenu();
    void handlePfeileHolen();
    void handleSchiessBetrieb();

    //-------------------------------------------------------------------------
    // State Entry/Exit Functions
    //-------------------------------------------------------------------------
    void enterSplash();
    void exitSplash();
    void enterConfigMenu();
    void exitConfigMenu();
    void enterPfeileHolen();
    void exitPfeileHolen();
    void enterSchiessBetrieb();
    void exitSchiessBetrieb();

    //-------------------------------------------------------------------------
    // Display Functions
    //-------------------------------------------------------------------------
    void drawSchiessBetrieb();
    void updateSchiessBetriebTimer();  // Nur Timer aktualisieren (kein Flackern)

    //-------------------------------------------------------------------------
    // Hilfsfunktionen
    //-------------------------------------------------------------------------

    /**
     * @brief Prüft ob genug Zeit im aktuellen State vergangen ist
     */
    bool timeInState(uint32_t milliseconds) const;

    /**
     * @brief Wechselt zur nächsten Schützengruppe im 4er-Zyklus
     * AB_POS1 -> CD_POS1 -> CD_POS2 -> AB_POS2 -> AB_POS1
     */
    void advanceToNextGroup();
};
