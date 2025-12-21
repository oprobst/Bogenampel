/**
 * @file DisplayManager.h
 * @brief Anzeige-Manager für LED-Strip (7-Segment + Gruppen)
 *
 * Kapselt die Anzeige-Logik für:
 * - 7-Segment Timer-Anzeige (3 Ziffern)
 * - Gruppen-LEDs (A/B und C/D)
 */

#pragma once

#include <Arduino.h>
#include <FastLED.h>

/**
 * @brief Manager-Klasse für LED-Strip Anzeige
 *
 * Verwaltet die Anzeige von Timer und Gruppen auf dem LED-Strip.
 */
class DisplayManager {
public:
    /**
     * @brief Konstruktor
     * @param ledArray Zeiger auf LED-Array (FastLED)
     */
    DisplayManager(CRGB* ledArray);

    /**
     * @brief Zeigt Timer-Wert auf 7-Segment Anzeige
     * @param seconds Sekunden (0-999)
     * @param color Farbe der Anzeige (z.B. CRGB::Green, CRGB::Red)
     * @param showLeadingZeros Führende Nullen anzeigen (Standard: false)
     */
    void displayTimer(uint16_t seconds, CRGB color, bool showLeadingZeros = false);

    /**
     * @brief Setzt die aktive Gruppe (LEDs)
     * @param group Gruppen-Code: 0=A/B, 1=C/D, 0xFF=keine Gruppe
     * @param color Farbe der Gruppe (Standard: Rot)
     */
    void setGroup(uint8_t group, CRGB color = CRGB::Red);

    /**
     * @brief Schaltet beide Gruppen aus
     */
    void clearGroups();

private:
    CRGB* leds;  // Zeiger auf LED-Array

    /**
     * @brief Zeigt eine Zahl auf der 7-Segment Anzeige
     * @param number Zahl (0-999)
     * @param color Farbe
     * @param showLeadingZeros Führende Nullen anzeigen?
     */
    void displayNumber(uint16_t number, CRGB color, bool showLeadingZeros = false);

    /**
     * @brief Zeigt eine einzelne Ziffer auf 7-Segment Display
     * @param digitStartIndex Start-Index der Ziffer im LED-Array
     * @param digit Ziffer (0-9)
     * @param color Farbe der Ziffer
     */
    void displayDigit(uint8_t digitStartIndex, uint8_t digit, CRGB color);

    /**
     * @brief Setzt Gruppe A/B LEDs
     * @param color Farbe
     */
    void setGroupAB(CRGB color);

    /**
     * @brief Setzt Gruppe C/D LEDs
     * @param color Farbe
     */
    void setGroupCD(CRGB color);
};
