/**
 * @file ButtonManager.h
 * @brief Button-Verwaltung mit Debouncing und Gesten (2-Taster-Bedienung V3)
 *
 * PORT aus V2 (Sender/ButtonManager.h), reduziert auf die real gelebte
 * 2-Taster-Bedienung. Die Enum benennt die ROLLE, nicht den Pin — welcher
 * Taster welche Rolle trägt, legt allein readRawState() fest:
 *
 * - Button::CONFIG → SW2, GPIO15, AKTIV HIGH (externer Teiler R2/R7, keine
 *   internen Pulls!). Das ist der Einschalt-Taster: er hält beim Boot den
 *   Power-Latch. Welcher Taster einschaltet, bestimmt die Hardware und ist
 *   per Firmware nicht tauschbar. Auf dem Gehäuse steht "Config", also trägt
 *   er die Weiter-/Ändern-Rolle. Klick feuert beim DRÜCKEN (keine Gesten).
 * - Button::OK → SW1, GPIO9, gegen GND, INPUT_PULLUP, aktiv LOW.
 *   Kurz = OK, ≥2s = Alarm (im Schießbetrieb), ≥3s = Power-Off (sonst) —
 *   Schwellen meldet der Manager, die Auswertung übernimmt die StateMachine
 *   (T016). Klick feuert beim LOSLASSEN, sonst wäre jeder Halte-Beginn auch
 *   ein Klick.
 *
 * - Boot-Lockout (pro Taster): Wer beim Einschalten schon gedrückt ist, meldet
 *   weder Klick noch Geste, bis er einmal losgelassen wurde. Nötig für den
 *   Einschalt-Druck auf CONFIG (Edge Case "Boot press duration") und für den
 *   Wartungsmodus, bei dem beide Taster gehalten werden — ohne den Lockout
 *   liefe dort nach 3 s die Power-Off-Geste von OK los.
 * - Kein Buzzer am V3-Sender (FR-019): Klick-Ton-Code entfernt
 */

#pragma once

#include "Config.h"

/**
 * @brief Button-Rollen (V3: nur 2 Taster) — Pin-Zuordnung in readRawState()
 */
enum class Button : uint8_t {
    OK = 0,      // SW1/GPIO9: Bestätigen, Alarm (2s), Power-Off (3s)
    CONFIG = 1,  // SW2/GPIO15: Weiter/Ändern — zugleich der Einschalt-Taster
    COUNT = 2    // Anzahl der Buttons
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
     * CONFIG: feuert beim Drücken (sofortige Reaktion, keine Halte-Geste).
     * OK: feuert beim LOSLASSEN, wenn kürzer als die Alarm-Schwelle (2 s)
     * gehalten wurde — sonst wäre jeder Halte-Beginn auch ein Klick.
     */
    bool wasClicked(Button btn);

    /**
     * @brief Halte-Geste (one-shot): Button wird gerade ≥ ms gehalten
     * @param btn Button
     * @param ms Schwelle in Millisekunden (z.B. 2000 Alarm, 3000 Power-Off)
     * @return true genau einmal beim Überschreiten der Schwelle
     *
     * Solange der Taster im Boot-Lockout steht (seit dem Einschalten noch nie
     * losgelassen), werden keine Halte-Gesten gemeldet.
     */
    bool wasHeldFor(Button btn, uint16_t ms);

    /**
     * @brief Prüft ob irgendein Button gedrückt ist
     */
    bool isAnyPressed() const;

    /**
     * @brief Boot-Lockout aktiv? (Taster seit dem Einschalten nicht losgelassen)
     */
    bool isBootLockoutActive(Button btn) const;

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
        bool bootLockout;          // beim Boot gedrückt — bis zum Loslassen stumm
    };

    ButtonState buttons[static_cast<uint8_t>(Button::COUNT)];

    /**
     * @brief Liest den rohen Button-Zustand (true = gedrückt)
     *
     * Hier — und nur hier — hängt die Rolle am Pin:
     *   CONFIG → Pins::BTN1 (GPIO15), aktiv HIGH (Teiler an +BATT)
     *   OK     → Pins::BTN2 (GPIO9),  aktiv LOW  (interner Pullup gegen GND)
     */
    bool readRawState(Button btn) const;
};
