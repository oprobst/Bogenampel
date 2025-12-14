/**
 * @file StateMachine.cpp
 * @brief State Machine Implementierung (Tournament Control)
 */

#include "StateMachine.h"
#include "Commands.h"

// Forward-Deklarationen für Radio-Funktionen (implementiert in Sender.ino)
extern TransmissionResult sendCommand(RadioCommand cmd);
extern bool testReceiverConnection();

StateMachine::StateMachine(Adafruit_ST7789& tft, ButtonManager& btnMgr)
    : display(tft)
    , buttons(btnMgr)
    , splashScreen(tft)
    , configMenu(tft, btnMgr)
    , pfeileHolenMenu(tft, btnMgr)
    , currentState(State::STATE_SPLASH)
    , previousState(State::STATE_SPLASH)
    , stateStartTime(0)
    , shootingTime(EEPROM_Config::DEFAULT_TIME)
    , shooterCount(EEPROM_Config::DEFAULT_COUNT)
    , connectionTested(false)
    , connectionSuccessful(false) {
}

void StateMachine::begin() {
    DEBUG_PRINTLN(F("State Machine initialisiert"));

    // Starte mit Splash Screen
    enterSplash();
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

    DEBUG_PRINT(F("State: "));
    DEBUG_PRINT(static_cast<int>(currentState));
    DEBUG_PRINT(F(" → "));
    DEBUG_PRINTLN(static_cast<int>(newState));

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
    DEBUG_PRINTLN(F("→ STATE_SPLASH"));

    // Verbindungstest-Variablen zurücksetzen
    connectionTested = false;
    connectionSuccessful = false;

    // Splash Screen zeichnen (zeigt "Suche Empfaengermodul..." an)
    splashScreen.draw();
}

void StateMachine::handleSplash() {
    // Verbindungstest durchführen (nur einmal, nach ~500ms)
    if (!connectionTested && timeInState(500)) {
        connectionSuccessful = testReceiverConnection();
        connectionTested = true;

        // Status auf Display aktualisieren
        if (connectionSuccessful) {
            splashScreen.updateConnectionStatus("Empfaenger verbunden");
        } else {
            splashScreen.updateConnectionStatus("Kein Empfaenger gefunden");
        }
    }

    // Bedingungen zum Verlassen des Splash Screens:
    // 1. Mindestens 3 Sekunden vergangen UND Verbindungstest abgeschlossen
    // 2. ODER: Taste gedrückt (Skip)

    bool minTimeElapsed = timeInState(Timing::SPLASH_DURATION_MS);
    bool canExit = minTimeElapsed && connectionTested;
    bool skipPressed = buttons.isAnyPressed();

    if (canExit || skipPressed) {
        if (skipPressed) {
            DEBUG_PRINTLN(F("Splash Screen uebersprungen (Taste gedrueckt)"));
        }
        setState(State::STATE_CONFIG_MENU);
    }
}

void StateMachine::exitSplash() {
    DEBUG_PRINTLN(F("← STATE_SPLASH"));
}

//=============================================================================
// STATE_CONFIG_MENU
//=============================================================================

void StateMachine::enterConfigMenu() {
    DEBUG_PRINTLN(F("→ STATE_CONFIG_MENU"));

    // ConfigMenu initialisieren
    configMenu.begin();
    configMenu.draw();
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

        DEBUG_PRINTLN(F("Config bestaetigt, sende CMD_INIT..."));

        // Sende CMD_INIT an Empfänger (wenn bereits verbunden, sonst überspringen)
        if (connectionSuccessful) {
            TransmissionResult result = sendCommand(CMD_INIT);
            if (result != TX_SUCCESS) {
                DEBUG_PRINTLN(F("Warnung: CMD_INIT fehlgeschlagen, fahre trotzdem fort"));
            }
        } else {
            DEBUG_PRINTLN(F("Kein Empfaenger verbunden, ueberspringe CMD_INIT"));
        }

        // Gehe zu PFEILE_HOLEN (auch ohne Empfänger, für Entwicklung)
        setState(State::STATE_PFEILE_HOLEN);
    }
}

void StateMachine::exitConfigMenu() {
    DEBUG_PRINTLN(F("← STATE_CONFIG_MENU"));
}

//=============================================================================
// STATE_PFEILE_HOLEN
//=============================================================================

void StateMachine::enterPfeileHolen() {
    DEBUG_PRINTLN(F("→ STATE_PFEILE_HOLEN"));

    // PfeileHolenMenu initialisieren
    pfeileHolenMenu.begin();
    pfeileHolenMenu.draw();

    // Verbindungstest-Timer zurücksetzen (sofort testen)
    lastConnectionCheck = 0;
}

void StateMachine::handlePfeileHolen() {
    // Verbindungstest alle 5 Sekunden durchführen
    if (millis() - lastConnectionCheck >= 5000) {
        bool connected = testReceiverConnection();
        pfeileHolenMenu.updateConnectionStatus(connected);
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
                // Sende CMD_START_120 oder CMD_START_240
                RadioCommand cmd = (shootingTime == 120) ? CMD_START_120 : CMD_START_240;
                TransmissionResult result = sendCommand(cmd);

                if (result == TX_SUCCESS) {
                    setState(State::STATE_SCHIESS_BETRIEB);
                } else {
                    DEBUG_PRINTLN(F("FEHLER: CMD_START konnte nicht gesendet werden"));
                    // TODO: Fehleranzeige auf Display
                    // Menü muss neu initialisiert werden
                    pfeileHolenMenu.begin();
                    pfeileHolenMenu.draw();
                }
                break;
            }

            case PfeileHolenAction::REIHENFOLGE:
                // TODO: Reihenfolge-Editor implementieren
                DEBUG_PRINTLN(F("TODO: Shooter Order Edit"));
                break;

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
    DEBUG_PRINTLN(F("← STATE_PFEILE_HOLEN"));
}

//=============================================================================
// STATE_SCHIESS_BETRIEB
//=============================================================================

void StateMachine::enterSchiessBetrieb() {
    DEBUG_PRINTLN(F("→ STATE_SCHIESS_BETRIEB"));
    drawSchiessBetrieb();
}

void StateMachine::handleSchiessBetrieb() {
    // Nur ein Button: "Passe beenden"
    if (buttons.wasPressed(Button::OK)) {
        // Sende CMD_STOP
        TransmissionResult result = sendCommand(CMD_STOP);

        if (result == TX_SUCCESS) {
            setState(State::STATE_PFEILE_HOLEN);
        } else {
            // Fehler: Zeige Fehlermeldung, bleibe im State
            DEBUG_PRINTLN(F("Fehler beim Senden von CMD_STOP"));
            // TODO: Fehleranzeige auf Display
        }
    }
}

void StateMachine::exitSchiessBetrieb() {
    DEBUG_PRINTLN(F("← STATE_SCHIESS_BETRIEB"));
}

void StateMachine::drawSchiessBetrieb() {
    display.fillScreen(ST77XX_BLACK);

    // Titel
    display.setTextSize(2);
    display.setTextColor(ST77XX_WHITE);
    display.setCursor(10, 10);
    display.print("Schiessbetrieb");

    // Button
    display.setTextSize(2);
    display.setTextColor(ST77XX_RED);
    display.setCursor(10, 80);
    display.print("[Passe beenden]");

    // Hinweis unten
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 30);
    display.print("OK: Passe beenden");
}

//=============================================================================
// Hilfsfunktionen
//=============================================================================

bool StateMachine::timeInState(uint32_t milliseconds) const {
    return (millis() - stateStartTime) >= milliseconds;
}
