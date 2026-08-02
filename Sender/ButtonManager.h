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
 *   Kurz = OK. Klick feuert beim LOSLASSEN, sonst wäre jeder Halte-Beginn auch
 *   ein Klick.
 *
 * Gesten seit Feature 006 (data-model.md §3):
 * - Klick             → wasClicked(), beide Taster
 * - 3× Klick ≤ 400 ms → wasMultiClicked() = Alarm. NUR auf der Rolle OK; die
 *                       StateMachine wertet es nur im Schießbetrieb und in
 *                       "Pfeile holen" aus. CONFIG zählt bewusst nicht mit —
 *                       damit zügiges Blättern und Werte-Hochzählen im Menü
 *                       keinen Fehlalarm auslösen kann.
 * - ≥ 3 s halten      → wasHeldFor(POWER_OFF_HOLD_MS) = Ausschalten, beide
 *                       Taster, in jedem Zustand. Halten hat sonst KEINE
 *                       Bedeutung mehr — die frühere 2-s-Alarm-Geste ist
 *                       entfallen, weil Ausschalten und Alarm nicht auf
 *                       derselben Bewegung liegen dürfen.
 * Der Manager meldet nur die Gesten, entschieden wird in der StateMachine.
 *
 * Aufrufpfad: update() läuft aus loop() UND aus dem GxEPD2-Busy-Callback
 * (Sender.cpp). Ohne Letzteres stünde die Abfrage während jedes Refreshs
 * 300-400 ms still und der Dreifachklick verlöre dort Klicks (research.md R-1).
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
     * @brief Mehrfachklick-Geste erkannt (one-shot, Flag wird beim Lesen gelöscht)
     * @param btn Button
     * @return true genau einmal, wenn die Folge gerade vollständig wurde
     *
     * Zählt Timing::MULTI_CLICK_COUNT (3) Drückflanken mit höchstens
     * Timing::MULTI_CLICK_GAP_MS (400 ms) Abstand. Gezählt wird ausschließlich
     * die Rolle OK — für Button::CONFIG liefert die Methode immer false.
     *
     * Der auslösende Klick erzeugt KEIN wasClicked()-Ereignis: sonst würde er
     * die Aktion des Zielzustands gleich wieder auslösen (im Alarm quittiert
     * ein OK-Klick sofort). Klicks 1 und 2 wirken dagegen normal — die
     * StateMachine stellt den Alarmzustand darüber (research.md R-2).
     *
     * Boot-Lockout, zu langes Halten und zu große Klickabstände brechen die
     * Folge ab.
     */
    bool wasMultiClicked(Button btn);

    /**
     * @brief Verwirft laufende Mehrfachklick-Folgen beider Taster
     *
     * Von StateMachine::setState() bei jedem Wechsel aufzurufen, der NICHT
     * zwischen zwei alarmfähigen Zuständen stattfindet. Ohne das würde ein im
     * Konfigurationsmenü aufgebauter Zählerstand später im Schießbetrieb einen
     * Fehlalarm auslösen (FR-011).
     */
    void resetMultiClick();

    /**
     * @brief Verwirft ausstehende Klick-Ereignisse BEIDER Taster
     *
     * Nötig an zwei Stellen, an denen sonst Klicks nachwirken, die zur
     * Alarm-Folge gehören:
     *  - beim Auslösen der Folge selbst (Klick 1 und 2 wurden während eines
     *    blockierenden Bildaufbaus nie abgeholt),
     *  - am Ende von enterAlarm(), weil Senden und Refresh rund eine Sekunde
     *    blockieren und der Busy-Callback in dieser Zeit weiter Klicks sammelt.
     * Ohne das quittiert der erste dieser Klicks den gerade ausgelösten Alarm
     * sofort wieder — der Alarmbildschirm flackert nur kurz auf.
     */
    void discardPendingClicks();

    /**
     * @brief Prüft ob irgendein Button gedrückt ist
     */
    bool isAnyPressed() const;

    /**
     * @brief Boot-Lockout aktiv? (Taster seit dem Einschalten nicht losgelassen)
     */
    bool isBootLockoutActive(Button btn) const;

    /**
     * @brief Zeitstempel (millis) der letzten entprellten Tasterflanke
     *
     * Grundlage für die automatische Abschaltung nach Inaktivität. Gezählt wird
     * jede Flanke, auch die des Einschalt-Drucks im Boot-Lockout: der Anwender
     * war nachweislich am Gerät, unabhängig davon ob die Geste gemeldet wurde.
     */
    uint32_t lastActivityMs() const { return lastActivity; }

private:
    /**
     * @brief Zustand eines einzelnen Buttons
     */
    struct ButtonState {
        bool pressed;              // Aktuell gedrückt (nach Debouncing)
        uint32_t lastChangeTime;   // Zeitpunkt der letzten akzeptierten Flanke
                                   // (danach DEBOUNCE_MS Sperre)
        uint32_t pressTime;        // Zeitpunkt des Drückens (für Halte-Gesten)
        bool clickedFlag;          // Event-Flag: Klick erkannt
        uint16_t reportedHoldMs;   // Höchste bereits gemeldete Halte-Schwelle
        bool bootLockout;          // beim Boot gedrückt — bis zum Loslassen stumm

        // Mehrfachklick-Geste (Alarm)
        uint8_t clickCount;        // Drückflanken der laufenden Folge (0..COUNT)
        uint32_t lastPressTime;    // millis der letzten gezählten Drückflanke
        bool multiClickFlag;       // Event-Flag: Folge vollständig erkannt
        bool multiClickArmed;      // laufende Betätigung würde die Folge schließen
    };

    ButtonState buttons[static_cast<uint8_t>(Button::COUNT)];

    uint32_t lastActivity;  // millis der letzten entprellten Flanke

    /**
     * @brief Liest den rohen Button-Zustand (true = gedrückt)
     *
     * Hier — und nur hier — hängt die Rolle am Pin:
     *   CONFIG → Pins::BTN1 (GPIO15), aktiv HIGH (Teiler an +BATT)
     *   OK     → Pins::BTN2 (GPIO9),  aktiv LOW  (interner Pullup gegen GND)
     */
    bool readRawState(Button btn) const;
};
