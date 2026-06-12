/**
 * @file ButtonManager.h
 * @brief Button-Verwaltung mit Debouncing und Gesten (2-Taster-Bedienung V3)
 *
 * PORT aus V2 (Sender/ButtonManager.h), reduziert auf die real gelebte
 * 2-Taster-Bedienung:
 * - BTN1 (SW2, GPIO15): Power/OK — AKTIV HIGH (externer Teiler R2/R7, keine
 *   internen Pulls!); kurz = OK, ≥2s = Alarm (im Schießbetrieb),
 *   ≥3s = Power-Off (sonst) — Schwellen meldet der Manager, die Auswertung
 *   übernimmt die StateMachine (T016)
 * - BTN2 (SW1, GPIO9): Weiter/Rechts — gegen GND, INPUT_PULLUP, aktiv LOW
 * - Boot-Lockout: BTN1-Gesten gesperrt, bis BTN1 nach dem Einschalt-Druck
 *   einmal losgelassen wurde (Edge Case "Boot press duration")
 * - Kein Buzzer am V3-Sender (FR-019): Klick-Ton-Code entfernt
 */

#pragma once

#include "Config.h"

/**
 * @brief Button-Enumeration (V3: nur 2 Taster)
 */
enum class Button : uint8_t {
    BTN1 = 0,   // SW2: Power/OK (aktiv HIGH)
    BTN2 = 1,   // SW1: Weiter/Rechts (aktiv LOW)
    COUNT = 2   // Anzahl der Buttons
};

/**
 * @brief Button Manager mit Debouncing, Klick- und Halte-Gesten
 */
class ButtonManager {
public:
    ButtonManager();

    /**
     * @brief Initialisiert die Button-Pins (inkl. Boot-Lockout-Erkennung)
     */
    void begin();

    /**
     * @brief Update-Funktion (in loop() aufrufen)
     */
    void update();

    /**
     * @brief Prüft ob Button aktuell gedrückt ist (mit Debouncing)
     */
    bool isPressed(Button btn) const;

    /**
     * @brief Klick-Ereignis (one-shot, Flag wird gelöscht)
     *
     * BTN2: feuert beim Drücken (sofortige Reaktion, keine Halte-Geste).
     * BTN1: feuert beim LOSLASSEN, wenn kürzer als die Alarm-Schwelle (2 s)
     * gehalten wurde — sonst wäre jeder Halte-Beginn auch ein Klick.
     */
    bool wasClicked(Button btn);

    /**
     * @brief Halte-Geste (one-shot): Button wird gerade ≥ ms gehalten
     * @param btn Button
     * @param ms Schwelle in Millisekunden (z.B. 2000 Alarm, 3000 Power-Off)
     * @return true genau einmal beim Überschreiten der Schwelle
     *
     * Während des Boot-Lockouts (BTN1 seit dem Einschalten noch nie
     * losgelassen) werden keine Halte-Gesten gemeldet.
     */
    bool wasHeldFor(Button btn, uint16_t ms);

    /**
     * @brief Prüft ob irgendein Button gedrückt ist
     */
    bool isAnyPressed() const;

    /**
     * @brief Boot-Lockout aktiv? (BTN1 seit Einschalt-Druck nicht losgelassen)
     */
    bool isBootLockoutActive() const { return bootLockout; }

private:
    /**
     * @brief Zustand eines einzelnen Buttons
     */
    struct ButtonState {
        bool pressed;              // Aktuell gedrückt (nach Debouncing)
        bool lastRawState;         // Letzter roher Pin-Zustand (gedrückt-Logik)
        uint32_t lastChangeTime;   // Zeitpunkt der letzten Zustandsänderung
        uint32_t pressTime;        // Zeitpunkt des Drückens (für Halte-Gesten)
        bool clickedFlag;          // Event-Flag: Klick erkannt
        uint16_t reportedHoldMs;   // Höchste bereits gemeldete Halte-Schwelle
    };

    ButtonState buttons[static_cast<uint8_t>(Button::COUNT)];

    bool bootLockout;  // BTN1 hielt das Gerät beim Boot — Gesten gesperrt

    /**
     * @brief Liest den rohen Button-Zustand (true = gedrückt)
     *
     * BTN1 aktiv HIGH (Teiler an +BATT), BTN2 aktiv LOW (Pullup gegen GND).
     */
    bool readRawState(Button btn) const;
};
