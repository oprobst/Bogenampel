/**
 * @file DisplayManager.cpp
 * @brief Implementierung des Anzeige-Managers
 */

#include "DisplayManager.h"
#include "Config.h"

DisplayManager::DisplayManager(CRGB* ledArray)
    : leds(ledArray) {
}

void DisplayManager::displayTimer(uint16_t seconds, CRGB color, bool showLeadingZeros) {
    displayNumber(seconds, color, showLeadingZeros);
}

void DisplayManager::setGroup(uint8_t group, CRGB color) {
    if (group == 0) {
        // Gruppe A/B aktiv
        setGroupAB(color);
        setGroupCD(CRGB::Black);
    } else if (group == 1) {
        // Gruppe C/D aktiv
        setGroupAB(CRGB::Black);
        setGroupCD(color);
    } else {
        // Keine Gruppe (0xFF oder andere)
        clearGroups();
    }
}

void DisplayManager::clearGroups() {
    setGroupAB(CRGB::Black);
    setGroupCD(CRGB::Black);
}

//=============================================================================
// Private Hilfsfunktionen
//=============================================================================

void DisplayManager::setGroupAB(CRGB color) {
    fill_solid(leds + LEDStrip::GROUP_AB_START, LEDStrip::GROUP_AB_LEDS, color);
    FastLED.show();
}

void DisplayManager::setGroupCD(CRGB color) {
    fill_solid(leds + LEDStrip::GROUP_CD_START, LEDStrip::GROUP_CD_LEDS, color);
    FastLED.show();
}

void DisplayManager::displayNumber(uint16_t number, CRGB color, bool showLeadingZeros) {
    // Begrenze auf 0-999
    if (number > 999) {
        number = 999;
    }

    // Extrahiere einzelne Ziffern
    uint8_t digit100 = number / 100;
    uint8_t digit10 = (number / 10) % 10;
    uint8_t digit1 = number % 10;

    // Zeige Ziffern an
    // 100er-Stelle
    if (showLeadingZeros || number >= 100) {
        displayDigit(LEDStrip::DIGIT_100_START, digit100, color);
    } else {
        displayDigit(LEDStrip::DIGIT_100_START, 0, CRGB::Black);  // Ausschalten
    }

    // 10er-Stelle
    if (showLeadingZeros || number >= 10) {
        displayDigit(LEDStrip::DIGIT_10_START, digit10, color);
    } else {
        displayDigit(LEDStrip::DIGIT_10_START, 0, CRGB::Black);  // Ausschalten
    }

    // 1er-Stelle: Immer anzeigen
    displayDigit(LEDStrip::DIGIT_1_START, digit1, color);

    FastLED.show();
}

void DisplayManager::displayDigit(uint8_t digitStartIndex, uint8_t digit, CRGB color) {
    // 7-Segment-Mapping für Ziffern 0-9
    // Jedes Bit repräsentiert ein Segment in der Reihenfolge: B, A, F, G, C, D, E
    const uint8_t segmentMap[10] = {
        0b1110111,  // 0: B, A, F, C, D, E (kein G)
        0b1000100,  // 1: B, C
        0b1101011,  // 2: B, A, G, D, E
        0b1101110,  // 3: B, A, G, C, D
        0b1011100,  // 4: B, F, G, C
        0b0111110,  // 5: A, F, G, C, D
        0b0111111,  // 6: A, F, G, C, D, E
        0b1100100,  // 7: B, A, C
        0b1111111,  // 8: B, A, F, G, C, D, E (alle)
        0b1111110   // 9: B, A, F, G, C, D
    };

    if (digit > 9) {
        digit = 0;  // Fallback auf 0 bei ungültiger Ziffer
    }

    uint8_t pattern = segmentMap[digit];

    // Segment-Reihenfolge: B, A, F, G, C, D, E
    for (uint8_t seg = 0; seg < LEDStrip::SEGMENTS_PER_DIGIT; seg++) {
        bool segmentOn = (pattern >> (LEDStrip::SEGMENTS_PER_DIGIT - 1 - seg)) & 0x01;
        CRGB segmentColor = segmentOn ? color : CRGB::Black;

        // Setze alle 6 LEDs dieses Segments
        uint8_t segmentStart = digitStartIndex + (seg * LEDStrip::LEDS_PER_SEGMENT);
        fill_solid(leds + segmentStart, LEDStrip::LEDS_PER_SEGMENT, segmentColor);
    }
}
