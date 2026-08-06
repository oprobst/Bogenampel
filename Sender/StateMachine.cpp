/**
 * @file StateMachine.cpp
 * @brief State Machine Implementierung (Tournament Control, V3)
 */

#include "StateMachine.h"
#include "Commands.h"

// 1-Hz-Flag und Timer-Reset aus Sender.cpp (esp_timer, R-7)
extern volatile bool senderSecondTick;
extern void resetSenderTimer();

StateMachine::StateMachine(EpaperDisplay& epdRef, ButtonManager& btnMgr, RadioManager& radioMgr,
                           PowerManager& powerMgr, ConfigStore& store)
    : epd(epdRef)
    , buttons(btnMgr)
    , radio(radioMgr)
    , power(powerMgr)
    , configStore(store)
    , splashScreen(epdRef)
    , configMenu(epdRef, btnMgr)
    , pfeileHolenMenu(epdRef, btnMgr)
    , schiessBetriebMenu(epdRef, btnMgr)
    , alarmScreen(epdRef)
    , shutdownScreen(epdRef)
    , noticeScreen(epdRef)
    , currentState(State::STATE_SPLASH)
    , stateStartTime(0)
    , shootingTime(TournamentDefaults::DEFAULT_TIME)
    , shooterCount(TournamentDefaults::DEFAULT_COUNT)
    , qualityTestDone(false)
    , connectionQuality(0)
    , qualityDisplayStartTime(0)
    , searchStatusShown(false)
    , lastConnectionCheck(0)
    , lastPingOk(false)
    , currentGroup(Groups::Type::GROUP_AB)     // Start mit A/B
    , currentPosition(Groups::Position::POS_1) // Start mit Position 1
    , ghostClearPending(false)
    , inPreparationPhase(false)
    , preparationSecondsRemaining(0)
    , shootingSecondsRemaining(0)
    , shootingDurationMs(0) {
}

void StateMachine::begin() {
    // Konfiguration aus NVS laden (FR-005)
    configStore.load();
    shootingTime = configStore.getShootingTime();
    shooterCount = configStore.getShooterCount();

    // Starte mit Splash Screen
    enterSplash();
}

void StateMachine::update() {
    // Ausschalten gilt in jedem Zustand und hat Vorrang vor allem anderen
    // (FR-002, Contract P-1). Kehrt im Erfolgsfall nicht zurück.
    checkPowerOffGesture();

    switch (currentState) {
        case State::STATE_SPLASH:         handleSplash(); break;
        case State::STATE_CONFIG_MENU:    handleConfigMenu(); break;
        case State::STATE_PFEILE_HOLEN:   handlePfeileHolen(); break;
        case State::STATE_SCHIESS_BETRIEB: handleSchiessBetrieb(); break;
        case State::STATE_ALARM:          handleAlarm(); break;
    }
}

bool StateMachine::isAlarmCapable(State s) {
    return s == State::STATE_SCHIESS_BETRIEB || s == State::STATE_PFEILE_HOLEN;
}

void StateMachine::setState(State newState) {
    if (newState == currentState) return;

    // Laufende Alarm-Folge verwerfen — außer der Wechsel bleibt innerhalb der
    // alarmfähigen Zustände. Denn genau das passiert im Normalfall: der erste
    // Klick der Folge beendet die Passe (Schießbetrieb → Pfeile holen), die
    // beiden restlichen Klicks müssen trotzdem noch zählen (FR-012). Umgekehrt
    // darf ein im Menü aufgebauter Zählerstand nicht in den Schießbetrieb
    // hineinlecken (FR-011).
    if (!(isAlarmCapable(currentState) && isAlarmCapable(newState))) {
        buttons.resetMultiClick();
    }

    currentState = newState;
    stateStartTime = millis();

    switch (currentState) {
        case State::STATE_SPLASH:          enterSplash(); break;
        case State::STATE_CONFIG_MENU:     enterConfigMenu(); break;
        case State::STATE_PFEILE_HOLEN:    enterPfeileHolen(); break;
        case State::STATE_SCHIESS_BETRIEB: enterSchiessBetrieb(); break;
        case State::STATE_ALARM:           enterAlarm(); break;
    }
}

//=============================================================================
// Statuszeile + Power-Off (T023/T024)
//=============================================================================

void StateMachine::refreshStatusLine() {
    EpaperDisplay::ChargeIcon icon;
    switch (power.chargeState()) {
        case ChargeState::CHARGING:  icon = EpaperDisplay::ChargeIcon::CHARGING; break;
        case ChargeState::COMPLETE:  icon = EpaperDisplay::ChargeIcon::COMPLETE; break;
        case ChargeState::SUSPENDED: icon = EpaperDisplay::ChargeIcon::SUSPENDED; break;
        case ChargeState::TEST_MODE: icon = EpaperDisplay::ChargeIcon::FAULT; break;
        default:                     icon = EpaperDisplay::ChargeIcon::NONE; break;
    }

    bool radioOk = radio.isPeerDiscovered()
                && (currentState != State::STATE_PFEILE_HOLEN || lastPingOk);

    epd.drawStatusLine(power.batteryPercent(), power.isUsbConnected(), icon,
                       power.isLowBattery(), radioOk);
    epd.showStatusLine();
}

void StateMachine::checkPowerOffGesture() {
    // ≥ 3 s halten = Power-Off, in JEDEM Zustand (FR-002). Früher war der
    // Schießbetrieb ausgenommen, weil dort das Halten den Alarm auslöste — der
    // liegt seit Feature 006 auf dem Dreifachklick. Ein Abschalten mitten in
    // der Passe ist unkritisch: der Empfänger führt sie autonom zu Ende
    // (FR-004a) — funktional dasselbe wie ein Sender außer Reichweite.
    // Beide Taster schalten aus: Eingeschaltet wird hardwareseitig immer über
    // CONFIG (Power-Latch), also muss dieselbe Taste auch ausschalten können.
    // Beim Einschalt-Druck greift der Boot-Lockout, die Geste kann also nicht
    // direkt beim Hochfahren auslösen.
    if (buttons.wasHeldFor(Button::OK, Timing::POWER_OFF_HOLD_MS)
        || buttons.wasHeldFor(Button::CONFIG, Timing::POWER_OFF_HOLD_MS)) {
        doPowerOff();
    }
}

void StateMachine::checkIdleTimeout() {
    // Als Aktivität zählt der spätere von beiden: letzter Tastendruck oder
    // letzter Zustandswechsel. Ohne Letzteres liefe die Uhr über einen ganzen
    // Turnierverlauf durch, obwohl das Gerät sichtbar arbeitet.
    uint32_t lastActivity = buttons.lastActivityMs();
    if ((int32_t)(stateStartTime - lastActivity) > 0) {
        lastActivity = stateStartTime;
    }

    if (millis() - lastActivity >= Timing::IDLE_POWER_OFF_MS) {
        DEBUG_PRINTLN("Idle-Timeout — automatische Abschaltung");
        doPowerOff("Automatische Abschaltung");
    }
}

void StateMachine::doPowerOff(const char* reason) {
    DEBUG_PRINTLN("Power-Off-Sequenz");

    // Config ist bereits gespeichert (Save-on-Confirm, R-6) — kein Save nötig

    // Abschalt-Screen — bleibt nach dem Abschalten stehen (e-Paper ist bistabil)
    shutdownScreen.draw(reason, power.batteryPercent());
    epd.fullRefresh();

    // Panel in Tiefschlaf, Rail aus, dann Latch loslassen (FR-018, R-13)
    epd.hibernate();
    epd.railOff();
    power.latchOff();  // kehrt nicht zurück
}

//=============================================================================
// STATE_SPLASH (inkl. Verbindungstest, US5/T033)
//=============================================================================

void StateMachine::enterSplash() {
    qualityTestDone = false;
    connectionQuality = 0;
    qualityDisplayStartTime = 0;
    searchStatusShown = false;

    // Splash zeichnen: ein Voll-Refresh (R-2)
    splashScreen.draw();
    epd.fullRefresh();
}

void StateMachine::handleSplash() {
    // Klick auf einen der beiden Taster: Splash überspringen (neu in V3, US5-Szenario 3)
    if (buttons.wasClicked(Button::OK) || buttons.wasClicked(Button::CONFIG)) {
        setState(State::STATE_CONFIG_MENU);
        return;
    }

    // Fall 1: Discovery läuft noch (RadioManager broadcastet HELLO, 1 Hz)
    if (!radio.isPeerDiscovered()) {
        if (!searchStatusShown) {
            searchStatusShown = true;
            splashScreen.updateConnectionStatus("Suche Empfaenger...");
        }

        // Ohne Empfänger: nach Splash-Dauer trotzdem freigeben (US5-Szenario 2)
        if (timeInState(Timing::SPLASH_DURATION_MS)) {
            splashScreen.showConnectionQuality(0);  // "Keine Verbindung"
            setState(State::STATE_CONFIG_MENU);
        }
        return;
    }

    // Fall 2: Empfänger gefunden, Quality Test noch nicht durchgeführt
    if (!qualityTestDone) {
        splashScreen.updateConnectionStatus("Teste Verbindung...");

        // 10 Pings im 250-ms-Raster (blockierend ~2,5 s, R-5)
        connectionQuality = radio.pingQualityTest();
        qualityTestDone = true;
        qualityDisplayStartTime = millis();

        splashScreen.showConnectionQuality(connectionQuality);
        return;
    }

    // Fall 3: Qualität wird angezeigt — nach Ablauf weiter ins Menü
    if (millis() - qualityDisplayStartTime >= Timing::QUALITY_DISPLAY_DURATION_MS) {
        setState(State::STATE_CONFIG_MENU);
    }
}

//=============================================================================
// STATE_CONFIG_MENU
//=============================================================================

void StateMachine::enterConfigMenu() {
    configMenu.begin();
    configMenu.setConfig(shootingTime, shooterCount);

    epd.clearBuffer();
    refreshStatusLine();  // Statuszeile in den Puffer + Partial (vor Vollbild ok)
    configMenu.draw();
    epd.refreshScreen();
}

void StateMachine::handleConfigMenu() {
    checkIdleTimeout();

    configMenu.update();

    // Navigation: Partial-Refresh des Inhaltsbereichs (kein Blitzen)
    if (configMenu.needsRedraw()) {
        configMenu.draw();
        epd.partialUpdate(0, Display::STATUS_H, EpaperDisplay::WIDTH,
                          EpaperDisplay::HEIGHT - Display::STATUS_H);
    }

    // Statuszeile bei neuen Messwerten aktualisieren
    if (power.update()) {
        refreshStatusLine();
    }

    // Prüfen ob "Start" bestätigt wurde
    if (configMenu.isComplete()) {
        // Konfiguration übernehmen und SOFORT persistieren (R-6, SC-008)
        shootingTime = configMenu.getShootingTime();
        shooterCount = configMenu.getShooterCount();
        configStore.set(shootingTime, shooterCount);
        configStore.save();

        // Turnier beginnt immer mit A/B, Position 1
        currentGroup = Groups::Type::GROUP_AB;
        currentPosition = Groups::Position::POS_1;

        // Sende CMD_INIT an Empfänger
        radio.sendCommand(CMD_INIT);

        setState(State::STATE_PFEILE_HOLEN);
    }
}

//=============================================================================
// STATE_PFEILE_HOLEN
//=============================================================================

void StateMachine::sendGroupCommand() {
    // Bei 1-2 Schützen: CMD_GROUP_NONE (beide Gruppen aus)
    // Bei 3-4 Schützen: Gruppe UND Position berücksichtigen (halbe Passe)
    RadioCommand groupCmd;
    if (shooterCount <= 2) {
        groupCmd = CMD_GROUP_NONE;
    } else if (currentPosition == Groups::Position::POS_1) {
        groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_AB : CMD_GROUP_CD;
    } else {
        groupCmd = (currentGroup == Groups::Type::GROUP_AB) ? CMD_GROUP_FINISH_AB
                                                            : CMD_GROUP_FINISH_CD;
    }
    radio.sendCommand(groupCmd);
}

void StateMachine::enterPfeileHolen() {
    pfeileHolenMenu.begin();
    pfeileHolenMenu.setTournamentConfig(shooterCount, currentGroup, currentPosition);

    epd.clearBuffer();
    refreshStatusLine();
    pfeileHolenMenu.draw();
    epd.refreshScreen();  // Zustandswechsel ohne Blitzen (Ghosting-Regel R-2)

    // Entschattung einplanen: der eine Voll-Refresh dieses Aufenthalts steht
    // noch aus (ausgelöst in handlePfeileHolen nach GHOST_CLEAR_DELAY_MS)
    ghostClearPending = true;

    // Gruppen-Signal sofort senden (Empfänger zeigt die richtige Gruppe)
    sendGroupCommand();

    // Verbindungstest-Timer: sofort testen
    lastConnectionCheck = 0;
}

void StateMachine::handlePfeileHolen() {
    // ALARM-Geste (OK 3×) auch hier: beim Pfeileholen stehen Leute an der
    // Schießlinie (FR-010). Vor allem anderen (Contract P-3).
    if (buttons.wasMultiClicked(Button::OK)) {
        setState(State::STATE_ALARM);
        return;
    }

    checkIdleTimeout();

    // Entschattung: einmal pro Aufenthalt voll durchblitzen, sobald das Gerät
    // GHOST_CLEAR_DELAY_MS (30 s) hier steht. Bis dahin liegt die
    // Bedieneinheit aus der Hand und die Schützen sind an den Scheiben — das
    // ist der einzige Moment im Ablauf, in dem das Blitzen niemanden stört.
    // Ohne angesammelte Partials wird es übersprungen.
    if (ghostClearPending && timeInState(Timing::GHOST_CLEAR_DELAY_MS)) {
        ghostClearPending = false;
        if (epd.hasGhosting()) {
            DEBUG_PRINTLN("Pfeile holen: Entschattung (Voll-Refresh)");
            epd.fullRefresh();
        }
    }

    // Verbindungstest alle 5 Sekunden (PING + Statuszeile, V2-Verhalten)
    if (lastConnectionCheck == 0 || millis() - lastConnectionCheck >= 5000) {
        lastConnectionCheck = millis();
        lastPingOk = (radio.sendCommand(CMD_PING) == TX_SUCCESS);
        power.update();
        refreshStatusLine();
    } else if (power.update()) {
        refreshStatusLine();
    }

    pfeileHolenMenu.update();

    if (pfeileHolenMenu.needsRedraw()) {
        pfeileHolenMenu.draw();
        epd.partialUpdate(0, Display::STATUS_H, EpaperDisplay::WIDTH,
                          EpaperDisplay::HEIGHT - Display::STATUS_H);
    }

    // Prüfen ob eine Aktion gewählt wurde
    PfeileHolenAction action = pfeileHolenMenu.getSelectedAction();
    if (action != PfeileHolenAction::NONE) {
        pfeileHolenMenu.resetAction();

        switch (action) {
            case PfeileHolenAction::NAECHSTE_PASSE:
                // Auto-Erkennung ganze/halbe Passe über currentPosition —
                // CMD_START wird in enterSchiessBetrieb() gesendet (V2)
                setState(State::STATE_SCHIESS_BETRIEB);
                break;

            case PfeileHolenAction::REIHENFOLGE:
                // Schützengruppen-Abfolge einen Schritt weiterschalten
                advanceToNextGroup();
                sendGroupCommand();
                pfeileHolenMenu.setTournamentConfig(shooterCount, currentGroup, currentPosition);
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

//=============================================================================
// STATE_SCHIESS_BETRIEB
//=============================================================================

void StateMachine::enterSchiessBetrieb() {
    // Starte mit Vorbereitungsphase (10 Sekunden, oder 5s im DEBUG)
    inPreparationPhase = true;
    preparationSecondsRemaining = Timing::PREPARATION_TIME_MS / 1000;

    // Schießzeit setzen (normal oder verkürzt für DEBUG)
    #if DEBUG_SHORT_TIMES
        shootingDurationMs = 15000UL;
        shootingSecondsRemaining = 15;
    #else
        shootingDurationMs = shootingTime * 1000UL;
        shootingSecondsRemaining = shootingTime;
    #endif

    // Sende START-Kommando sofort (Empfänger startet eigene 10s Vorbereitung)
    RadioCommand cmd = (shootingTime == 120) ? CMD_START_120 : CMD_START_240;
    radio.sendCommand(cmd);

    // SOFORT danach: Sender-Timer neu starten (synchron mit Empfänger, R-7)
    resetSenderTimer();

    // Menü initialisieren und zeichnen (Screenwechsel-Refresh, kein Blitzen)
    schiessBetriebMenu.begin();
    schiessBetriebMenu.setTournamentConfig(shootingTime, shooterCount,
                                           currentGroup, currentPosition);
    schiessBetriebMenu.setPreparationPhase(true, Timing::PREPARATION_TIME_MS);

    epd.clearBuffer();
    refreshStatusLine();
    schiessBetriebMenu.draw();
    epd.refreshScreen();
    schiessBetriebMenu.updateCountdown(preparationSecondsRemaining);
}

void StateMachine::handleSchiessBetrieb() {
    // ALARM-Geste: OK 3× klicken, je ≤ 400 ms Abstand (FR-007/FR-009).
    // Vor der Klick-Auswertung, damit der Alarm gewinnt (Contract P-3).
    if (buttons.wasMultiClicked(Button::OK)) {
        setState(State::STATE_ALARM);
        return;
    }

    // Prüfe ob eine Sekunde vergangen ist (esp_timer-Flag aus Sender.cpp)
    if (senderSecondTick) {
        senderSecondTick = false;

        // Fall 1: Vorbereitungsphase (10 Sekunden, Countdown in Sekunden)
        if (inPreparationPhase) {
            if (preparationSecondsRemaining > 0) {
                preparationSecondsRemaining--;
            }

            if (preparationSecondsRemaining == 0) {
                // Beende Vorbereitungsphase → Wechsel zur Schießphase
                inPreparationPhase = false;

                DEBUG_PRINTLN("Prep END -> Shooting START");

                // Phasenwechsel: Screen neu zeichnen (Partial des Inhalts)
                schiessBetriebMenu.setShootingPhase(shootingSecondsRemaining * 1000);
                schiessBetriebMenu.draw();
                epd.partialUpdate(0, Display::STATUS_H, EpaperDisplay::WIDTH,
                                  EpaperDisplay::HEIGHT - Display::STATUS_H);
                schiessBetriebMenu.updateCountdown(shootingSecondsRemaining);
            } else {
                // 1-Hz-Countdown im Partial-Fenster (SC-007)
                schiessBetriebMenu.updateCountdown(preparationSecondsRemaining);
            }
        }
        // Fall 2: Eigentliche Schießphase (120/240 Sekunden)
        else {
            if (shootingSecondsRemaining > 0) {
                shootingSecondsRemaining--;
            }

            // Automatisches Ende bei Zeitablauf
            if (shootingSecondsRemaining == 0) {
                handleShootingPhaseEnd(false);  // Kein manualStop: Empfänger endet AUTONOM
                return;
            }

            schiessBetriebMenu.updateCountdown(shootingSecondsRemaining);
        }
    }

    // Menu aktualisieren (OK kurz = "Passe beenden")
    schiessBetriebMenu.update();

    if (schiessBetriebMenu.isEndRequested()) {
        schiessBetriebMenu.resetEndRequest();

        if (inPreparationPhase) {
            // Während Vorbereitungsphase: Abbruch
            advanceToNextGroup();
            sendManualStop();
            setState(State::STATE_PFEILE_HOLEN);
        } else {
            // Während Schießphase: Manueller Abbruch → CMD_STOP senden
            handleShootingPhaseEnd(true);
        }
    }
}

/**
 * REGRESSION-GUARD (FR-004a, V2-Bugfix "Empfänger autonom"):
 * - Zeitablauf (manualStop == false) sendet NIEMALS CMD_STOP — der Empfänger
 *   beendet die Passe selbst (3 Pieptöne, rot, "000") ohne Funk (SC-012).
 * - Das CMD_START_* für die zweite Gruppe (3-4 Schützen) ist nur ein
 *   Sync-Signal; der Empfänger ignoriert es, wenn seine Vorbereitungsphase
 *   bereits autonom läuft (Protokoll-Regel 3).
 */
void StateMachine::handleShootingPhaseEnd(bool manualStop) {
    if (shooterCount <= 2) {
        // 1-2 Schützen: Nur eine Gruppe
        if (manualStop) {
            sendManualStop();
        }

        // Wechsle zur nächsten Gruppe (für nächste Passe)
        advanceToNextGroup();

        setState(State::STATE_PFEILE_HOLEN);
    } else {
        // 3-4 Schützen: Zwei Gruppen pro Passe
        // POS_1 = erste Gruppe der Passe → zweite Gruppe starten
        // POS_2 = zweite Gruppe der Passe → Ende der Passe
        if (currentPosition == Groups::Position::POS_1) {
            // Erste Gruppe fertig → Starte zweite Gruppe
            advanceToNextGroup();

            // Sender-Timer für zweite Gruppe neu aufsetzen
            inPreparationPhase = true;
            preparationSecondsRemaining = Timing::PREPARATION_TIME_MS / 1000;
            shootingSecondsRemaining = shootingDurationMs / 1000;

            // Empfänger startet zweite Gruppe autonom – CMD_START nur als Sync
            RadioCommand startCmd = (shootingTime == 120) ? CMD_START_120 : CMD_START_240;
            radio.sendCommand(startCmd);

            // Timer zurücksetzen für synchronen Start
            resetSenderTimer();

            // Menü für zweite Gruppe aktualisieren
            schiessBetriebMenu.setTournamentConfig(shootingTime, shooterCount,
                                                   currentGroup, currentPosition);
            schiessBetriebMenu.setPreparationPhase(true, Timing::PREPARATION_TIME_MS);
            schiessBetriebMenu.draw();
            epd.partialUpdate(0, Display::STATUS_H, EpaperDisplay::WIDTH,
                              EpaperDisplay::HEIGHT - Display::STATUS_H);
            schiessBetriebMenu.updateCountdown(preparationSecondsRemaining);
        } else {
            // Zweite Gruppe fertig (POS_2) → Ende der Passe
            if (manualStop) {
                sendManualStop();
            }

            advanceToNextGroup();

            setState(State::STATE_PFEILE_HOLEN);
        }
    }
}

//=============================================================================
// STATE_ALARM (T028)
//=============================================================================

TransmissionResult StateMachine::sendAlarmWithRetry() {
    // App-Level-Retries (V2): jede Wiederholung ist ein NEUER Frame mit neuer
    // seq (sendCommand vergibt sie) und erfolgt nur nach TX_TIMEOUT/TX_ERROR;
    // der Empfänger ignoriert CMD_ALARM bei laufendem Alarm (Contract Regel 6)
    TransmissionResult result = TX_ERROR;

    for (uint8_t retry = 0; retry < Timing::ALARM_MAX_RETRIES; retry++) {
        if (retry > 0) {
            delay(Timing::ALARM_RETRY_DELAY_MS);
        }

        // allowRelink=false: Sonst könnte der dritte fehlgeschlagene Link-Retry
        // mitten in dieser Schleife dropPeer() auslösen — danach kehrt
        // sendCommand() sofort mit TX_TIMEOUT zurück, ohne noch einen Frame zu
        // senden, und der Alarm verlöre 2 seiner 3 App-Versuche (also 6 der 9
        // Funkversuche). Genau der Fall tritt ein, wenn der Fehlerzähler durch
        // vorherige Kommandos schon bei 2 stand.
        result = radio.sendCommand(CMD_ALARM, false);

        if (result == TX_SUCCESS) {
            return TX_SUCCESS;
        }
    }

    // Alle Versuche erfolglos → jetzt darf der Peer fallen und die Discovery
    // neu anlaufen (aufgeschoben, nicht aufgehoben).
    radio.relinkIfExhausted();

    return result;
}

void StateMachine::sendManualStop() {
    if (radio.sendCommand(CMD_STOP) != TX_SUCCESS) {
        DEBUG_PRINTLN("STOP nicht bestaetigt — Warnhinweis");
        showStopFailedNotice();
    }
}

void StateMachine::showStopFailedNotice() {
    noticeScreen.drawStopFailed();
    epd.refreshScreen();

    // Der Bildaufbau blockiert ~500 ms; was in dieser Zeit an Klicks anfiel,
    // gehört noch zur Abbruch-Bedienung und darf die Meldung nicht sofort
    // wegquittieren (gleiche Begründung wie in enterAlarm()).
    buttons.discardPendingClicks();

    const uint32_t until = millis() + Timing::NOTICE_TIMEOUT_MS;
    while ((int32_t)(millis() - until) < 0) {
        buttons.update();
        if (buttons.wasClicked(Button::OK) || buttons.wasClicked(Button::CONFIG)) {
            break;
        }
        delay(10);
    }

    buttons.discardPendingClicks();
}

void StateMachine::enterAlarm() {
    DEBUG_PRINTLN("ALARM triggered");

    // CMD_ALARM mit Retries senden, Zustellstatus anzeigen (US3-Szenario 3)
    TransmissionResult result = sendAlarmWithRetry();

    alarmScreen.draw(result == TX_SUCCESS);
    epd.refreshScreen();

    // Senden (bis 600 ms Retries) und Refresh (~500 ms) blockieren zusammen
    // rund eine Sekunde, in der der Busy-Callback weiter Tasten pollt. Was in
    // dieser Zeit noch getippt wurde, gehört zum Auslösen der Geste — nicht
    // zum Quittieren. Ohne das Verwerfen quittiert der nächste Schleifenlauf
    // den Alarm sofort wieder und das Gerät fällt nach "Pfeile holen" zurück.
    buttons.discardPendingClicks();
}

void StateMachine::handleAlarm() {
    // Die Alarm-Folge wird hier bewusst NICHT ausgewertet: ein stehender Alarm
    // darf sich nicht selbst neu auslösen (FR-015). Ausschalten läuft zentral
    // in update(). Bleibt der Quittier-Klick.

    // OK kurz: Alarm quittieren → CMD_STOP → Pfeile holen
    if (buttons.wasClicked(Button::OK)) {
        radio.sendCommand(CMD_STOP);
        setState(State::STATE_PFEILE_HOLEN);
    }
}

//=============================================================================
// Hilfsfunktionen
//=============================================================================

bool StateMachine::timeInState(uint32_t milliseconds) const {
    return (millis() - stateStartTime) >= milliseconds;
}

void StateMachine::advanceToNextGroup() {
    // 4-Zyklus: AB_POS1 -> CD_POS2 -> CD_POS1 -> AB_POS2 -> AB_POS1
    if (currentGroup == Groups::Type::GROUP_AB && currentPosition == Groups::Position::POS_1) {
        currentGroup = Groups::Type::GROUP_CD;
        currentPosition = Groups::Position::POS_2;
    }
    else if (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_2) {
        currentGroup = Groups::Type::GROUP_CD;
        currentPosition = Groups::Position::POS_1;
    }
    else if (currentGroup == Groups::Type::GROUP_CD && currentPosition == Groups::Position::POS_1) {
        currentGroup = Groups::Type::GROUP_AB;
        currentPosition = Groups::Position::POS_2;
    }
    else { // AB_POS2
        currentGroup = Groups::Type::GROUP_AB;
        currentPosition = Groups::Position::POS_1;
    }
}
