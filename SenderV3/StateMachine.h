/**
 * @file StateMachine.h
 * @brief Haupt-State-Machine für Bogenampel V3 Sender
 *
 * PORT aus V2 (Sender/StateMachine.h): Zustände und Übergänge 1:1
 * (data-model.md §3), umgestellt auf:
 * - e-Paper-Rendering über die Menü-Klassen (EpaperDisplay statt ST7789)
 * - ESP-NOW (RadioManager statt globaler RF24-Funktionen)
 * - NVS (ConfigStore statt EEPROM)
 * - 2-Tasten-Gesten: OK kurz = bestätigen, OK ≥2s = Alarm (Schießbetrieb),
 *   beide Taster ≥3s = Power-Off (außerhalb Schießbetrieb, FR-015)
 *
 * REGRESSION-GUARD (FR-004a): handleShootingPhaseEnd(false) bei Zeitablauf
 * sendet KEIN CMD_STOP — der Empfänger beendet die Passe autonom; das
 * CMD_START für die zweite Gruppe ist nur ein Sync-Signal.
 */

#pragma once

#include "Config.h"
#include "ButtonManager.h"
#include "RadioManager.h"
#include "PowerManager.h"
#include "ConfigStore.h"
#include "EpaperDisplay.h"
#include "ConfigMenu.h"
#include "SplashScreen.h"
#include "PfeileHolenMenu.h"
#include "SchiessBetriebMenu.h"
#include "AlarmScreen.h"

/**
 * @brief System-Zustände (Tournament State Machine)
 */
enum class State : uint8_t {
    STATE_SPLASH,          // Splash + Verbindungstest (überspringbar)
    STATE_CONFIG_MENU,     // Konfigurationsmenü (Zeit, Schützenanzahl)
    STATE_PFEILE_HOLEN,    // Turniermodus: Pfeile holen (Pause zwischen Passen)
    STATE_SCHIESS_BETRIEB, // Turniermodus: Schießbetrieb aktiv
    STATE_ALARM            // Alarm: Notfall-Abbruch des Schießbetriebs
};

class StateMachine {
public:
    StateMachine(EpaperDisplay& epd, ButtonManager& btnMgr, RadioManager& radioMgr,
                 PowerManager& powerMgr, ConfigStore& store);

    /**
     * @brief Initialisiert die State Machine (startet im Splash)
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     */
    void update();

    /**
     * @brief Aktuellen Zustand abfragen
     */
    State getCurrentState() const { return currentState; }

    /**
     * @brief Manueller Zustandswechsel
     */
    void setState(State newState);

private:
    EpaperDisplay& epd;
    ButtonManager& buttons;
    RadioManager& radio;
    PowerManager& power;
    ConfigStore& configStore;

    SplashScreen splashScreen;
    ConfigMenu configMenu;
    PfeileHolenMenu pfeileHolenMenu;
    SchiessBetriebMenu schiessBetriebMenu;
    AlarmScreen alarmScreen;

    State currentState;
    uint32_t stateStartTime;  // Zeitstempel beim Zustandswechsel

    //-------------------------------------------------------------------------
    // Turnierkonfiguration (aus NVS geladen, im Menü geändert)
    //-------------------------------------------------------------------------
    uint16_t shootingTime;   // 120 oder 240 Sekunden
    uint8_t shooterCount;    // 2 (1-2 Schützen) oder 4 (3-4 Schützen)

    //-------------------------------------------------------------------------
    // State Variables: SPLASH
    //-------------------------------------------------------------------------
    bool qualityTestDone;       // Connection Quality Test durchgeführt?
    uint8_t connectionQuality;  // Verbindungsqualität in Prozent (0-100)
    uint32_t qualityDisplayStartTime;
    bool searchStatusShown;     // "Suche Empfaenger" bereits angezeigt

    //-------------------------------------------------------------------------
    // State Variables: PFEILE_HOLEN
    //-------------------------------------------------------------------------
    uint32_t lastConnectionCheck;  // Zeitpunkt der letzten Verbindungsprüfung
    bool lastPingOk;               // Ergebnis des letzten Pings (Statuszeile)

    //-------------------------------------------------------------------------
    // Schützengruppen-Tracking (für 3-4 Schützen Modus)
    //-------------------------------------------------------------------------
    Groups::Type currentGroup;
    Groups::Position currentPosition;

    //-------------------------------------------------------------------------
    // State Variables: SCHIESS_BETRIEB (1-Hz-Flag aus SenderV3.ino)
    //-------------------------------------------------------------------------
    bool inPreparationPhase;
    uint32_t preparationSecondsRemaining;
    uint32_t shootingSecondsRemaining;
    uint32_t shootingDurationMs;

    //-------------------------------------------------------------------------
    // State Handlers
    //-------------------------------------------------------------------------
    void handleSplash();
    void handleConfigMenu();
    void handlePfeileHolen();
    void handleSchiessBetrieb();
    void handleAlarm();

    //-------------------------------------------------------------------------
    // State Entry Functions
    //-------------------------------------------------------------------------
    void enterSplash();
    void enterConfigMenu();
    void enterPfeileHolen();
    void enterSchiessBetrieb();
    void enterAlarm();

    /**
     * @brief Behandelt Ende der Schießphase (FR-004a-Regression-Guard!)
     * @param manualStop true = Bediener hat abgebrochen (CMD_STOP senden),
     *        false = Zeitablauf (KEIN Funk — Empfänger endet autonom)
     */
    void handleShootingPhaseEnd(bool manualStop = false);

    //-------------------------------------------------------------------------
    // Hilfsfunktionen
    //-------------------------------------------------------------------------
    bool timeInState(uint32_t milliseconds) const;

    /**
     * @brief Wechselt zur nächsten Schützengruppe im 4er-Zyklus
     * AB_POS1 -> CD_POS2 -> CD_POS1 -> AB_POS2 -> AB_POS1
     */
    void advanceToNextGroup();

    /**
     * @brief Sendet das zur aktuellen Gruppe/Position passende GROUP-Kommando
     */
    void sendGroupCommand();

    /**
     * @brief Alarm mit App-Level-Retries senden (3 Versuche à 200 ms, V2)
     * @return TX_SUCCESS sobald ein Versuch bestätigt wurde
     */
    TransmissionResult sendAlarmWithRetry();

    /**
     * @brief Statuszeile neu zeichnen (Akku/USB/Lader/Funk) + Partial-Refresh
     */
    void refreshStatusLine();

    /**
     * @brief Power-Off-Geste prüfen (beide Taster ≥ 3 s) und ggf. abschalten (FR-015)
     */
    void checkPowerOffGesture();

    /**
     * @brief Geordnetes Ausschalten: Screen → hibernate → LOAD aus → LATCH LOW
     */
    void doPowerOff();
};
