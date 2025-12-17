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
    , qualityTestDone(false)
    , connectionQuality(0)
    , qualityDisplayStartTime(0)
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
                // Wechsel in Schießbetrieb (CMD_START wird in enterSchiessBetrieb() gesendet)
                setState(State::STATE_SCHIESS_BETRIEB);
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
        // DEBUG: 6s statt 120s, 12s statt 240s
        shootingDurationMs = (shootingTime == 120) ? 6000UL : 12000UL;
    #else
        shootingDurationMs = shootingTime * 1000UL;  // Sekunden → Millisekunden
    #endif

    // Sende START-Kommando sofort (Empfänger startet eigene 10s Vorbereitungsphase)
    RadioCommand cmd = (shootingTime == 120) ? CMD_START_120 : CMD_START_240;
    sendCommand(cmd);

    drawSchiessBetrieb();
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
            updateSchiessBetriebTimer();
            lastDisplayUpdate = millis();
        }

        // Button zum Abbrechen
        if (buttons.wasPressed(Button::OK)) {
            // Sende STOP
            sendCommand(CMD_STOP);
            // Sende Gruppeninformation
            RadioCommand groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_AB : CMD_GROUP_CD;
            sendCommand(groupCmd);

            advanceToNextGroup();
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
        updateSchiessBetriebTimer();
        lastDisplayUpdate = millis();
    }

    // Automatisches Ende bei Zeitablauf
    if (timeExpired) {
        // Sende STOP
        sendCommand(CMD_STOP);
        // Sende Gruppeninformation
        RadioCommand groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_AB : CMD_GROUP_CD;
        sendCommand(groupCmd);

        advanceToNextGroup();
        setState(State::STATE_PFEILE_HOLEN);
        return;
    }

    // Manuelles Ende durch Button
    if (buttons.wasPressed(Button::OK)) {
        // Sende STOP
        sendCommand(CMD_STOP);
        // Sende Gruppeninformation
        RadioCommand groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_AB : CMD_GROUP_CD;
        sendCommand(groupCmd);

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

    // Oranger Rahmen
    display.drawRect(margin, btnY, btnW, btnH, Display::COLOR_ORANGE);

    display.setTextSize(2);
    display.setTextColor(Display::COLOR_ORANGE);

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

    uint16_t remainingSec;
    uint16_t timerColor;

    if (inPreparationPhase) {
        // Vorbereitungsphase: Orange Countdown (10s)
        uint32_t prepElapsed = millis() - preparationStartTime;
        uint32_t prepRemainingMs = (prepElapsed < Timing::PREPARATION_TIME_MS)
            ? (Timing::PREPARATION_TIME_MS - prepElapsed) : 0;
        remainingSec = (prepRemainingMs + 999) / 1000;  // Aufrunden
        timerColor = Display::COLOR_ORANGE;
    } else {
        // Schießphase: Grüner Countdown (120/240s)
        uint32_t elapsed = millis() - shootingStartTime;
        uint32_t remainingMs = (elapsed < shootingDurationMs) ? (shootingDurationMs - elapsed) : 0;
        remainingSec = (remainingMs + 999) / 1000;  // Aufrunden
        timerColor = ST77XX_GREEN;
    }

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
    display.setTextColor(timerColor);
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
