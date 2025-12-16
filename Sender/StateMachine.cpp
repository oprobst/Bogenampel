/**
 * @file StateMachine.cpp
 * @brief State Machine Implementierung (Tournament Control)
 */

#include "StateMachine.h"
#include "Commands.h"

// Forward-Deklarationen für Radio-Funktionen (implementiert in Sender.ino)
extern TransmissionResult sendCommand(RadioCommand cmd);
extern bool testReceiverConnection();
extern bool initializeRadio();

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
    , radioInitialized(false)
    , connectionTested(false)
    , connectionSuccessful(false)
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
    lastConnectionCheck = 0;  // Sofort testen

    // Splash Screen zeichnen (zeigt initial "Suche Empfaengermodul..." an)
    splashScreen.draw();

    // Status je nach Radio-Initialisierung aktualisieren
    if (!radioInitialized) {
        splashScreen.updateConnectionStatus("Funkmodul nicht installiert");
    }
}

void StateMachine::handleSplash() {
    // Fall 1: Radio-Modul nicht initialisiert
    // -> Versuche alle Sekunde das Modul zu initialisieren
    if (!radioInitialized) {
        if (millis() - lastConnectionCheck >= 1000) {
            radioInitialized = initializeRadio();
            lastConnectionCheck = millis();

            if (radioInitialized) {
                splashScreen.updateConnectionStatus("Suche Empfaenger...");
                // Jetzt mit Empfängersuche fortfahren
                connectionTested = false;
                connectionSuccessful = false;
            } else {
                splashScreen.updateConnectionStatus("Funkmodul nicht installiert");
            }
        }
        // Splash Screen bleibt solange bestehen, bis Modul gefunden wird
        return;
    }

    // Fall 2: Radio-Modul initialisiert
    // -> Sende kontinuierlich PINGs zum Testen (alle 2 Sekunden)
    if (millis() - lastConnectionCheck >= 2000) {
        // PING senden
        sendCommand(CMD_PING);

        // Zähler für Display
        static uint8_t pingCount = 0;
        pingCount++;

        // Status auf Display aktualisieren
        char statusMsg[32];
        sprintf(statusMsg, "Sende PING #%d...", pingCount);
        splashScreen.updateConnectionStatus(statusMsg);

        lastConnectionCheck = millis();
        connectionTested = true;
    }

    // Bedingungen zum Verlassen des Splash Screens:
    // 1. Mindestens 3 Sekunden vergangen UND mindestens 1 Test durchgeführt
    // 2. ODER: Taste gedrückt (Skip)

    bool minTimeElapsed = timeInState(Timing::SPLASH_DURATION_MS);
    bool canExit = minTimeElapsed && connectionTested;
    bool skipPressed = buttons.isAnyPressed();

    if (canExit || skipPressed) {
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

        // Sende CMD_INIT an Empfänger (wenn bereits verbunden, sonst überspringen)
        if (connectionSuccessful) {
            sendCommand(CMD_INIT);
        }

        // Gehe zu PFEILE_HOLEN (auch ohne Empfänger, für Entwicklung)
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
                    // Menü muss neu initialisiert werden
                    pfeileHolenMenu.begin();
                    pfeileHolenMenu.draw();
                }
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
    // Timer initialisieren
    shootingStartTime = millis();
    shootingDurationMs = shootingTime * 1000UL;  // Sekunden → Millisekunden

    drawSchiessBetrieb();
}

void StateMachine::handleSchiessBetrieb() {
    // Verbleibende Zeit berechnen
    uint32_t elapsed = millis() - shootingStartTime;
    bool timeExpired = (elapsed >= shootingDurationMs);

    // Nur Timer alle 1000ms aktualisieren (1x pro Sekunde)
    static uint32_t lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate >= 1000) {
        updateSchiessBetriebTimer();
        lastDisplayUpdate = millis();
    }

    // Automatisches Ende bei Zeitablauf
    if (timeExpired) {
        sendCommand(CMD_STOP);
        advanceToNextGroup();
        setState(State::STATE_PFEILE_HOLEN);
        return;
    }

    // Manuelles Ende durch Button
    if (buttons.wasPressed(Button::OK)) {
        sendCommand(CMD_STOP);
        advanceToNextGroup();
        setState(State::STATE_PFEILE_HOLEN);
    }
}

void StateMachine::exitSchiessBetrieb() {
}

void StateMachine::drawSchiessBetrieb() {
    display.fillScreen(ST77XX_BLACK);

    // Überschrift: "Schiessbetrieb" in Orange
    display.setTextSize(3);
    display.setTextColor(ST77XX_ORANGE);
    display.setCursor(10, 10);
    display.print(F("Schiessbetrieb"));

    // Trennlinie
    display.drawFastHLine(10, 50, display.width() - 20, Display::COLOR_GRAY);

    // Gruppensequenz anzeigen (nur bei 3-4 Schützen)
    if (shooterCount == 4) {
        // Zeile 1: "Aktuell: A/B" oder "Aktuell: C/D"
        display.setCursor(10, 58);
        display.setTextSize(2);
        display.setTextColor(ST77XX_WHITE);
        display.print(F("Aktuell: "));
        display.setTextColor(ST77XX_YELLOW);
        display.println(currentGroup == Groups::Type::GROUP_AB ? F("A/B") : F("C/D"));

        // Zeile 2: Statischer String mit Hervorhebung
        display.setCursor(10, 82);
        display.setTextSize(2);

        // "{A/B -> C/D} {C/D -> A/B}"
        // Teile: "{A/B", " -> ", "C/D}", " ", "{C/D", " -> ", "A/B}"

        // Bestimme welcher Teil gelb sein soll
        bool highlightAB1 = (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_1);
        bool highlightCD1 = (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_1);
        bool highlightCD2 = (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_2);
        bool highlightAB2 = (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_2);

        // "{A/B"
        display.setTextColor(highlightAB1 ? ST77XX_YELLOW : Display::COLOR_GRAY);
        display.print(F("{A/B"));

        // " -> "
        display.setTextColor(Display::COLOR_GRAY);
        display.print(F(" -> "));

        // "C/D}"
        display.setTextColor(highlightCD1 ? ST77XX_YELLOW : Display::COLOR_GRAY);
        display.print(F("C/D}"));

        // " "
        display.setTextColor(Display::COLOR_GRAY);
        display.print(F(" "));

        // "{C/D"
        display.setTextColor(highlightCD2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
        display.print(F("{C/D"));

        // " -> "
        display.setTextColor(Display::COLOR_GRAY);
        display.print(F(" -> "));

        // "A/B}"
        display.setTextColor(highlightAB2 ? ST77XX_YELLOW : Display::COLOR_GRAY);
        display.print(F("A/B}"));
    }
    // Bei 1-2 Schützen: Keine Gruppenanzeige (nur A/B, keine Rotation)

    // Verbleibende Zeit anzeigen (große Zahlen) - MITTIG ZENTRIERT
    uint32_t elapsed = millis() - shootingStartTime;
    uint32_t remainingMs = (elapsed < shootingDurationMs) ? (shootingDurationMs - elapsed) : 0;
    uint16_t remainingSec = (remainingMs + 999) / 1000;  // Aufrunden

    // Timer-Position: Bei 3-4 Schützen tiefer setzen (wegen Gruppenanzeige)
    uint16_t timerY = (shooterCount == 4) ? 112 : 65;

    display.setTextSize(4);
    display.setTextColor(ST77XX_GREEN);

    // Timer horizontal zentrieren
    char timerText[8];
    sprintf(timerText, "%ds", remainingSec);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timerText, 0, 0, &x1, &y1, &w, &h);
    uint16_t timerX = (display.width() - w) / 2;

    display.setCursor(timerX, timerY);
    display.print(remainingSec);
    display.print(F("s"));

    // Button "Passe beenden" - knapp über dem grauen Text
    const uint16_t btnY = 184;  // 8 Pixel höher als vorher
    const uint16_t btnH = 30;
    const uint16_t margin = 10;
    uint16_t btnW = display.width() - 2 * margin;

    // Grauer Hintergrund (wie bei anderen Buttons)
    display.fillRect(margin, btnY, btnW, btnH, Display::COLOR_DARKGRAY);

    // Roter Rahmen
    display.drawRect(margin, btnY, btnW, btnH, ST77XX_RED);

    display.setTextSize(2);
    display.setTextColor(ST77XX_RED);

    // Variablen x1, y1, w, h bereits oben deklariert - wiederverwenden
    display.getTextBounds("Passe beenden", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(margin + (btnW - w) / 2, btnY + (btnH - h) / 2);
    display.print(F("Passe beenden"));

    // Hinweis unten
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    display.setCursor(10, display.height() - 15);
    display.print(F("OK: Passe beenden"));
}

void StateMachine::updateSchiessBetriebTimer() {
    // Timer-Position: Bei 3-4 Schützen tiefer setzen (wegen Gruppenanzeige)
    const uint16_t timerY = (shooterCount == 4) ? 112 : 65;
    const uint16_t timerH = 32;   // Höhe für Textgröße 4

    // Verbleibende Zeit berechnen
    uint32_t elapsed = millis() - shootingStartTime;
    uint32_t remainingMs = (elapsed < shootingDurationMs) ? (shootingDurationMs - elapsed) : 0;
    uint16_t remainingSec = (remainingMs + 999) / 1000;  // Aufrunden

    // Timer horizontal zentrieren
    display.setTextSize(4);
    char timerText[8];
    sprintf(timerText, "%ds", remainingSec);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timerText, 0, 0, &x1, &y1, &w, &h);
    uint16_t timerX = (display.width() - w) / 2;

    // Timer-Bereich löschen (volle Breite um Flackern zu vermeiden)
    display.fillRect(0, timerY, display.width(), timerH, ST77XX_BLACK);

    // Timer neu zeichnen
    display.setTextColor(ST77XX_GREEN);
    display.setCursor(timerX, timerY);
    display.print(remainingSec);
    display.print(F("s"));
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
        currentPosition = Groups::Position::POS_1;
    }
    else if (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_1) {
        // State 2 -> State 3
        currentGroup = Groups::Type::GROUP_CD;
        currentPosition = Groups::Position::POS_2;
    }
    else if (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_2) {
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
