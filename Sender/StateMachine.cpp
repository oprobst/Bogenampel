/**
 * @file StateMachine.cpp
 * @brief State Machine Implementierung (Tournament Control)
 */

#include "StateMachine.h"
#include "Commands.h"

// Forward-Deklarationen für Radio-Funktionen (implementiert in Sender.ino)
extern TransmissionResult sendCommand(RadioCommand cmd);
extern bool testReceiverConnection();
extern uint8_t testConnectionQuality();
extern bool initializeRadio();

// Forward-Deklarationen für Batterie-Funktionen (implementiert in Sender.ino)
extern uint16_t readBatteryVoltage();
extern bool isUsbPowered();

StateMachine::StateMachine(Adafruit_ST7789& tft, ButtonManager& btnMgr)
    : display(tft)
    , buttons(btnMgr)
    , splashScreen(tft)
    , configMenu(tft, btnMgr)
    , pfeileHolenMenu(tft, btnMgr)
    , schiessBetriebMenu(tft, btnMgr)
    , currentState(State::STATE_SPLASH)
    , previousState(State::STATE_SPLASH)
    , stateStartTime(0)
    , shootingTime(EEPROM_Config::DEFAULT_TIME)
    , shooterCount(EEPROM_Config::DEFAULT_COUNT)
    , radioInitialized(false)
    , connectionTested(false)
    , connectionSuccessful(false)
    , qualityTestDone(false)
    , connectionQuality(0)
    , qualityDisplayStartTime(0)
    , lastConnectionCheck(0)
    , initialPingsDone(false)
    , currentGroup(Groups::Type::GROUP_AB)     // Start mit A/B
    , currentPosition(Groups::Position::POS_1) { // Start mit Position 1
}

void StateMachine::begin() {
    // Starte mit Splash Screen
    enterSplash();
}

void StateMachine::setRadioInitialized(bool initialized) {
    radioInitialized = initialized;
}

void StateMachine::update() {
    // State Handler aufrufen
    switch (currentState) {
        case State::STATE_SPLASH:
            handleSplash();
            break;

        case State::STATE_CONFIG_MENU:
            handleConfigMenu();
            break;

        case State::STATE_PFEILE_HOLEN:
            handlePfeileHolen();
            break;

        case State::STATE_SCHIESS_BETRIEB:
            handleSchiessBetrieb();
            break;
    }
}

void StateMachine::setState(State newState) {
    if (newState == currentState) return;

    // Exit aktueller State
    switch (currentState) {
        case State::STATE_SPLASH: exitSplash(); break;
        case State::STATE_CONFIG_MENU: exitConfigMenu(); break;
        case State::STATE_PFEILE_HOLEN: exitPfeileHolen(); break;
        case State::STATE_SCHIESS_BETRIEB: exitSchiessBetrieb(); break;
    }

    // Zustand wechseln
    previousState = currentState;
    currentState = newState;
    stateStartTime = millis();

    // Enter neuer State
    switch (currentState) {
        case State::STATE_SPLASH: enterSplash(); break;
        case State::STATE_CONFIG_MENU: enterConfigMenu(); break;
        case State::STATE_PFEILE_HOLEN: enterPfeileHolen(); break;
        case State::STATE_SCHIESS_BETRIEB: enterSchiessBetrieb(); break;
    }
}

//=============================================================================
// STATE_SPLASH
//=============================================================================

void StateMachine::enterSplash() {
    // Verbindungstest-Variablen zurücksetzen
    connectionTested = false;
    connectionSuccessful = false;
    qualityTestDone = false;
    connectionQuality = 0;
    qualityDisplayStartTime = 0;
    lastConnectionCheck = 0;  // Sofort testen

    // Splash Screen zeichnen (zeigt initial "Suche Empfaengermodul..." an)
    splashScreen.draw();

    // Status je nach Radio-Initialisierung aktualisieren
    if (!radioInitialized) {
        splashScreen.updateConnectionStatus("Suche Funkmodul");
    }
}

void StateMachine::handleSplash() {
    // Taste zum Überspringen prüfen (jederzeit möglich)
    if (buttons.isAnyPressed()) {
        setState(State::STATE_CONFIG_MENU);
        return;
    }

    // Fall 1: Radio-Modul nicht initialisiert
    // -> Versuche alle Sekunde das Modul zu initialisieren
    if (!radioInitialized) {
        if (millis() - lastConnectionCheck >= 1000) {
            radioInitialized = initializeRadio();
            lastConnectionCheck = millis();

            if (!radioInitialized) {
                splashScreen.updateConnectionStatus("Suche Funkmodul");
            }
        }
        // Splash Screen bleibt solange bestehen, bis Modul gefunden wird
        return;
    }

    // Fall 2: Radio initialisiert, aber Quality Test noch nicht durchgeführt
    if (!qualityTestDone) {
        // Status anzeigen
        splashScreen.updateConnectionStatus("Teste Verbindung");

        // Connection Quality Test durchführen (blockierend, ~5 Sekunden)
        connectionQuality = testConnectionQuality();
        qualityTestDone = true;
        connectionTested = true;
        connectionSuccessful = (connectionQuality > 0);
        qualityDisplayStartTime = millis();

        // Qualität anzeigen
        splashScreen.showConnectionQuality(connectionQuality);
        return;
    }

    // Fall 3: Quality Test durchgeführt - zeige Qualität für 5 Sekunden
    uint32_t qualityDisplayTime = millis() - qualityDisplayStartTime;
    if (qualityDisplayTime >= Timing::QUALITY_DISPLAY_DURATION_MS) {
        // 5 Sekunden sind vorbei -> zum Config Menu
        setState(State::STATE_CONFIG_MENU);
    }
}

void StateMachine::exitSplash() {
}

//=============================================================================
// STATE_CONFIG_MENU
//=============================================================================

void StateMachine::enterConfigMenu() {
    // ConfigMenu initialisieren
    configMenu.begin();
    configMenu.draw();

    // Initiale Pings zurücksetzen für nächsten Pfeile-Holen State
    initialPingsDone = false;
}

void StateMachine::handleConfigMenu() {
    // ConfigMenu aktualisieren
    configMenu.update();

    // Display neu zeichnen wenn nötig
    if (configMenu.needsRedraw()) {
        configMenu.draw();
    }

    // Prüfen ob "Start" bestätigt wurde
    if (configMenu.isComplete()) {
        // Konfiguration übernehmen
        shootingTime = configMenu.getShootingTime();
        shooterCount = configMenu.getShooterCount();

        // Sende CMD_INIT an Empfänger
        sendCommand(CMD_INIT);

        // Gehe zu PFEILE_HOLEN
        setState(State::STATE_PFEILE_HOLEN);
    }
}

void StateMachine::exitConfigMenu() {
}

//=============================================================================
// STATE_PFEILE_HOLEN
//=============================================================================

void StateMachine::enterPfeileHolen() {
    // PfeileHolenMenu initialisieren
    pfeileHolenMenu.begin();

    // Turnierkonfiguration setzen (inkl. Schützengruppen)
    pfeileHolenMenu.setTournamentConfig(shooterCount, currentGroup, currentPosition);

    pfeileHolenMenu.draw();

    // Gruppen-Signal sofort senden (damit Empfänger die richtige Gruppe anzeigt)
    // Bei 1-2 Schützen: CMD_GROUP_NONE (beide Gruppen aus)
    // Bei 3-4 Schützen: Berücksichtige Gruppe UND Position
    RadioCommand groupCmd;
    if (shooterCount <= 2) {
        groupCmd = CMD_GROUP_NONE;  // Keine Gruppen bei 1-2 Schützen
    } else {
        // Bei 3-4 Schützen: Prüfe ob erste oder zweite Hälfte der Passe
        if (currentPosition == Groups::Position::POS_1) {
            // Erste Hälfte (ganze Passe)
            groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_AB : CMD_GROUP_CD;
        } else {
            // Zweite Hälfte (halbe Passe)
            groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_FINISH_AB : CMD_GROUP_FINISH_CD;
        }
    }
    sendCommand(groupCmd);

    // Verbindungstest-Timer zurücksetzen (sofort testen)
    lastConnectionCheck = 0;
}

void StateMachine::handlePfeileHolen() {
    // Beim ersten Aufruf: 4 schnelle Pings durchführen um die Ping-Historie zu füllen
    if (!initialPingsDone) {
        // 4 schnelle Pings im Abstand von 200ms
        for (uint8_t i = 0; i < 4; i++) {
            bool connected = testReceiverConnection();
            pfeileHolenMenu.updateConnectionStatus(connected);

            // Warte 200ms bis zum nächsten Ping (außer beim letzten)
            if (i < 3) {
                delay(200);
            }
        }

        // Initiale Batteriemessung durchführen
        uint16_t voltage = readBatteryVoltage();
        bool usbPowered = isUsbPowered();
        pfeileHolenMenu.updateBatteryStatus(voltage, usbPowered);

        // Flags setzen
        initialPingsDone = true;
        lastConnectionCheck = millis();
    }

    // Verbindungstest alle 5 Sekunden durchführen (nach den initialen Pings)
    if (millis() - lastConnectionCheck >= 5000) {
        bool connected = testReceiverConnection();
        pfeileHolenMenu.updateConnectionStatus(connected);

        // Batteriemessung durchführen
        uint16_t voltage = readBatteryVoltage();
        bool usbPowered = isUsbPowered();
        pfeileHolenMenu.updateBatteryStatus(voltage, usbPowered);

        lastConnectionCheck = millis();
    }

    // PfeileHolenMenu aktualisieren
    pfeileHolenMenu.update();

    // Display neu zeichnen wenn nötig
    if (pfeileHolenMenu.needsRedraw()) {
        pfeileHolenMenu.draw();
    }

    // Prüfen ob eine Aktion gewählt wurde
    PfeileHolenAction action = pfeileHolenMenu.getSelectedAction();
    if (action != PfeileHolenAction::NONE) {
        pfeileHolenMenu.resetAction();

        switch (action) {
            case PfeileHolenAction::NAECHSTE_PASSE: {
                // Auto-Erkennung: Ganze oder halbe Passe basierend auf Position
                // POS_1: Ganze Passe (beide Gruppen)
                // POS_2: Halbe Passe (nur zweite Gruppe)

                if (currentPosition == Groups::Position::POS_1) {
                    // === GANZE PASSE ===
                    // Wechsel in Schießbetrieb (CMD_START wird in enterSchiessBetrieb() gesendet)
                    setState(State::STATE_SCHIESS_BETRIEB);
                } else {
                    // === HALBE PASSE ===
                    // Starte zweite Hälfte der Passe (nur die aktuelle Gruppe)
                    // Gruppenwechsel erfolgt erst NACH der Schießphase in handleShootingPhaseEnd()

                    // Wechsel in Schießbetrieb (CMD_START wird in enterSchiessBetrieb() gesendet)
                    setState(State::STATE_SCHIESS_BETRIEB);
                }
                break;
            }

            case PfeileHolenAction::REIHENFOLGE: {
                // Schützengruppen-Abfolge einen Schritt weiterschalten
                advanceToNextGroup();

                // Sende GROUP-Kommando an Empfänger (damit Anzeige sofort aktualisiert wird)
                RadioCommand groupCmd;
                if (shooterCount <= 2) {
                    groupCmd = CMD_GROUP_NONE;  // Keine Gruppen bei 1-2 Schützen
                } else {
                    // Bei 3-4 Schützen: Prüfe ob erste oder zweite Gruppe in der Passe
                    if (currentPosition == Groups::Position::POS_1) {
                        // Erste Gruppe (ganze Passe)
                        groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_AB : CMD_GROUP_CD;
                    } else {
                        // Zweite Gruppe (halbe Passe)
                        groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_FINISH_AB : CMD_GROUP_FINISH_CD;
                    }
                }
                sendCommand(groupCmd);

                // Neue Gruppe/Position an PfeileHolenMenu übergeben
                pfeileHolenMenu.setTournamentConfig(shooterCount, currentGroup, currentPosition);
                break;
            }

            case PfeileHolenAction::NEUSTART:
                // Zurück zur Konfiguration
                setState(State::STATE_CONFIG_MENU);
                break;

            default:
                break;
        }
    }
}

void StateMachine::exitPfeileHolen() {
}

//=============================================================================
// STATE_SCHIESS_BETRIEB
//=============================================================================

void StateMachine::enterSchiessBetrieb() {
    // Starte mit Vorbereitungsphase (10 Sekunden, oder 5s im DEBUG)
    inPreparationPhase = true;
    preparationStartTime = millis();

    // Schießzeit setzen (normal oder verkürzt für DEBUG)
    #if DEBUG_SHORT_TIMES
        // DEBUG: 15s für beide Modi
        shootingDurationMs = 15000UL;
    #else
        shootingDurationMs = shootingTime * 1000UL;  // Sekunden → Millisekunden
    #endif

    // Sende START-Kommando sofort (Empfänger startet eigene 10s Vorbereitungsphase)
    RadioCommand cmd = (shootingTime == 120) ? CMD_START_120 : CMD_START_240;
    sendCommand(cmd);

    // Menü initialisieren
    schiessBetriebMenu.begin();
    schiessBetriebMenu.setTournamentConfig(shootingTime, shooterCount, currentGroup, currentPosition);

    // Vorbereitungsphase setzen
    schiessBetriebMenu.setPreparationPhase(true, Timing::PREPARATION_TIME_MS);

    // Initial zeichnen
    schiessBetriebMenu.draw();
}

void StateMachine::handleSchiessBetrieb() {
    // Fall 1: Vorbereitungsphase (10 Sekunden, orange Countdown)
    if (inPreparationPhase) {
        uint32_t prepElapsed = millis() - preparationStartTime;

        // Prüfe ob Vorbereitungsphase vorbei
        if (prepElapsed >= Timing::PREPARATION_TIME_MS) {
            // Beende Vorbereitungsphase (Empfänger macht das automatisch auch)
            inPreparationPhase = false;
            shootingStartTime = millis();
        }

        // Timer alle 1000ms aktualisieren
        static uint32_t lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate >= 1000) {
            // Timer aktualisieren
            uint32_t prepRemaining = (prepElapsed < Timing::PREPARATION_TIME_MS)
                ? (Timing::PREPARATION_TIME_MS - prepElapsed) : 0;
            schiessBetriebMenu.setPreparationPhase(true, prepRemaining);
            lastDisplayUpdate = millis();
        }

        // Menu aktualisieren
        schiessBetriebMenu.update();
        if (schiessBetriebMenu.needsRedraw()) {
            schiessBetriebMenu.draw();
        }

        // Prüfe ob "Passe beenden" gedrückt wurde
        if (schiessBetriebMenu.isEndRequested()) {
            schiessBetriebMenu.resetEndRequest();

            // Wechsle zur nächsten Gruppe ZUERST
            advanceToNextGroup();

            // Sende STOP
            sendCommand(CMD_STOP);

            // Wechsle zu STATE_PFEILE_HOLEN (sendet GROUP-Kommando in enterPfeileHolen)
            setState(State::STATE_PFEILE_HOLEN);
        }
        return;
    }

    // Fall 2: Eigentliche Schießphase (120/240 Sekunden, grün)
    uint32_t elapsed = millis() - shootingStartTime;
    bool timeExpired = (elapsed >= shootingDurationMs);

    // Nur Timer alle 1000ms aktualisieren (1x pro Sekunde)
    static uint32_t lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate >= 1000) {
        // Timer aktualisieren
        uint32_t remainingMs = (elapsed < shootingDurationMs) ? (shootingDurationMs - elapsed) : 0;
        schiessBetriebMenu.setShootingPhase(remainingMs);
        lastDisplayUpdate = millis();
    }

    // Menu aktualisieren
    schiessBetriebMenu.update();
    if (schiessBetriebMenu.needsRedraw()) {
        schiessBetriebMenu.draw();
    }

    // Automatisches Ende bei Zeitablauf
    if (timeExpired) {
        handleShootingPhaseEnd();
        return;
    }

    // Manuelles Ende durch Button
    if (schiessBetriebMenu.isEndRequested()) {
        schiessBetriebMenu.resetEndRequest();
        handleShootingPhaseEnd();
    }
}

/**
 * @brief Behandelt das Ende der Schießphase (automatisch oder manuell)
 *
 * Für 1-2 Schützen: Sende STOP, gehe zu PFEILE_HOLEN
 * Für 3-4 Schützen:
 *   - Nach erster Gruppe (A/B): Starte zweite Gruppe (C/D)
 *   - Nach zweiter Gruppe (C/D): Sende STOP, gehe zu PFEILE_HOLEN
 */
void StateMachine::handleShootingPhaseEnd() {
    if (shooterCount <= 2) {
        // 1-2 Schützen: Nur eine Gruppe
        // Sende STOP (3 Pieptöne auf Empfänger)
        sendCommand(CMD_STOP);

        // Wechsle zur nächsten Gruppe (für nächste Passe)
        advanceToNextGroup();

        // Gehe zu PFEILE_HOLEN
        setState(State::STATE_PFEILE_HOLEN);
    } else {
        // 3-4 Schützen: Zwei Gruppen pro Passe
        // Prüfe welche Gruppe gerade fertig ist (BEFORE advanceToNextGroup!)
        if (currentGroup == Groups::Type::GROUP_AB) {
            // Erste Gruppe (A/B) fertig → Starte zweite Gruppe (C/D)
            advanceToNextGroup();  // Wechsle zu GROUP_CD

            // Vorbereitung für zweite Gruppe manuell neu starten
            // (setState würde nicht funktionieren da wir bereits in STATE_SCHIESS_BETRIEB sind)
            inPreparationPhase = true;
            preparationStartTime = millis();

            // Sende START-Kommando (Empfänger startet eigene 10s Vorbereitungsphase)
            RadioCommand startCmd = (shootingTime == 120) ? CMD_START_120 : CMD_START_240;
            sendCommand(startCmd);

            // Menü für zweite Gruppe aktualisieren
            schiessBetriebMenu.setTournamentConfig(shootingTime, shooterCount, currentGroup, currentPosition);
            schiessBetriebMenu.setPreparationPhase(true, Timing::PREPARATION_TIME_MS);
            schiessBetriebMenu.draw();
        } else {
            // Zweite Gruppe (C/D) fertig → Ende der Passe
            // Sende STOP (3 Pieptöne auf Empfänger)
            sendCommand(CMD_STOP);

            // Wechsle zur nächsten Gruppe (zurück zu A/B für nächste Passe)
            advanceToNextGroup();

            // Gehe zu PFEILE_HOLEN
            setState(State::STATE_PFEILE_HOLEN);
        }
    }
}

void StateMachine::exitSchiessBetrieb() {
}

//=============================================================================
// Hilfsfunktionen
//=============================================================================

bool StateMachine::timeInState(uint32_t milliseconds) const {
    return (millis() - stateStartTime) >= milliseconds;
}

void StateMachine::advanceToNextGroup() {
    // 4-Zyklus: AB_POS1 -> CD_POS1 -> CD_POS2 -> AB_POS2 -> AB_POS1
    if (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_1) {
        // State 1 -> State 2
        currentGroup = Groups::Type::GROUP_CD;
        currentPosition = Groups::Position::POS_2;
    }
    else if (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_2) {
        // State 2 -> State 3
        currentGroup = Groups::Type::GROUP_CD;
        currentPosition = Groups::Position::POS_1;
    }
    else if (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_1) {
        // State 3 -> State 4
        currentGroup = Groups::Type::GROUP_AB;
        currentPosition = Groups::Position::POS_2;
    }
    else { // AB_POS2
        // State 4 -> State 1
        currentGroup = Groups::Type::GROUP_AB;
        currentPosition = Groups::Position::POS_1;
    }
}
