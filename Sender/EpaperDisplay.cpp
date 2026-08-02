/**
 * @file EpaperDisplay.cpp
 * @brief Implementierung des GxEPD2-Wrappers
 */

#include "EpaperDisplay.h"

#include <SPI.h>

EpaperDisplay::EpaperDisplay()
    : display(GxEPD2_154_D67(Pins::EPD_CS, Pins::EPD_DC, Pins::EPD_RST, Pins::EPD_BUSY))
    , railEnabled(false)
    , partialCount(0)
    , forceFullNext(true) {
}

void EpaperDisplay::begin() {
    // 1. LOAD-Rail einschalten (3V3_LOAD versorgt das Panel, FR-018)
    pinMode(Pins::LOAD, OUTPUT);
    digitalWrite(Pins::LOAD, HIGH);
    railEnabled = true;
    delay(10);  // Rail stabilisieren lassen

    // 2. SPI auf den Panel-Pins starten (kein MISO — e-Paper ist write-only)
    SPI.begin(Pins::SPI_CLK, -1, Pins::SPI_MOSI, Pins::EPD_CS);

    // 3. Panel initialisieren
    display.init(0, true, 2, false);  // initial=true, reset_duration=2ms
    display.setRotation(Display::ROTATION);
    display.setTextColor(GxEPD_BLACK);
    clearBuffer();

    // Frisch initialisiertes Panel hat kein gültiges Referenzbild im RAM —
    // das erste Bild muss voll geschrieben werden (GxEPD2 erzwingt das intern
    // ebenfalls über _initial_refresh; hier explizit für unsere Buchhaltung).
    partialCount = 0;
    forceFullNext = true;
}

void EpaperDisplay::clearBuffer() {
    display.fillScreen(GxEPD_WHITE);
}

void EpaperDisplay::refreshScreen() {
    // Nur wenn das Panel zwingend ein Vollbild braucht (frisch initialisiert
    // oder aus dem Tiefschlaf) — sonst blitzt ein Zustandswechsel NIE.
    //
    // Früher stand hier zusätzlich ein Ghosting-Budget (20 Partials). Das war
    // in der Praxis unbrauchbar: Der 1-Hz-Countdown brauchte es binnen 20 s
    // auf, also blitzte jeder Wechsel nach einer Passe. Die Entschattung
    // passiert jetzt zeitgesteuert in "Pfeile holen" (GHOST_CLEAR_DELAY_MS).
    if (forceFullNext) {
        fullRefresh();
        return;
    }

    // Partial-Waveform über das volle Fenster: schreibt nur die geänderten
    // Pixel um, ohne den schwarz/weiß-Invertierungszyklus des Voll-Refresh.
    display.setFullWindow();
    display.display(true);
    if (partialCount < 255) {
        partialCount++;
    }
}

void EpaperDisplay::fullRefresh() {
    display.setFullWindow();
    display.display(false);  // Voll-Refresh (setzt Ghosting zurück, R-2)
    partialCount = 0;
    forceFullNext = false;
}

void EpaperDisplay::partialUpdate(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    display.displayWindow(x, y, w, h);  // Partial-Refresh aus dem Puffer

    // Restschatten-Buchhaltung: Der Zähler sagt nur noch, OB sich seit dem
    // letzten Voll-Refresh Ghosting angesammelt hat (hasGhosting()). Er löst
    // selbst nichts mehr aus — ein Voll-Refresh darf niemals mitten in einen
    // laufenden Countdown platzen.
    if (partialCount < 255) {
        partialCount++;
    }
}

void EpaperDisplay::hibernate() {
    display.hibernate();
    forceFullNext = true;  // nach dem Aufwachen fehlt das Referenzbild im Panel-RAM
}

void EpaperDisplay::railOff() {
    if (railEnabled) {
        digitalWrite(Pins::LOAD, LOW);
        railEnabled = false;
    }
}

void EpaperDisplay::printCentered(const char* text, int16_t y, uint8_t textSize) {
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(textSize);
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((WIDTH - w) / 2, y);
    display.print(text);
}

void EpaperDisplay::drawStatusLine(uint8_t batteryPercent, bool usbConnected, ChargeIcon charge,
                                   bool lowBattery, bool radioConnected) {
    const uint16_t y = Display::STATUS_Y;
    const uint16_t h = Display::STATUS_H;

    // Bereich löschen + Trennlinie unten
    display.fillRect(Display::STATUS_X, y, Display::STATUS_W, h, GxEPD_WHITE);
    display.drawFastHLine(0, y + h - 1, Display::STATUS_W, GxEPD_BLACK);

    // --- Links: Batterie-Icon + Prozent ---
    const uint16_t iconX = 4;
    const uint16_t iconY = y + 6;
    const uint16_t bodyW = 22;
    const uint16_t bodyH = 11;
    display.drawRect(iconX, iconY, bodyW, bodyH, GxEPD_BLACK);
    display.fillRect(iconX + bodyW, iconY + 3, 2, 5, GxEPD_BLACK);  // Pluspol
    uint16_t fillW = ((bodyW - 4) * batteryPercent) / 100;
    if (fillW > 0) {
        display.fillRect(iconX + 2, iconY + 2, fillW, bodyH - 4, GxEPD_BLACK);
    }

    display.setTextSize(1);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(iconX + bodyW + 6, iconY + 2);
    display.print(batteryPercent);
    display.print('%');

    // --- Mitte: USB-/Lade-Status bzw. Low-Battery-Warnung ---
    const char* midText = nullptr;
    if (lowBattery && !usbConnected) {
        midText = "AKKU LEER!";          // Warnung < 3,3 V (FR-016)
    } else if (charge == ChargeIcon::CHARGING) {
        midText = "laedt...";
    } else if (charge == ChargeIcon::COMPLETE && usbConnected) {
        midText = "voll";
    } else if (charge == ChargeIcon::SUSPENDED) {
        midText = "laedt nicht!";        // Temperatur-/Timer-Fehler oder Standby
    } else if (charge == ChargeIcon::FAULT) {
        midText = "Ladefehler!";
    } else if (usbConnected) {
        midText = "USB";
    }
    if (midText) {
        int16_t x1, y1;
        uint16_t w, th;
        display.getTextBounds(midText, 0, 0, &x1, &y1, &w, &th);
        display.setCursor((Display::STATUS_W - w) / 2 + 10, iconY + 2);
        display.print(midText);
    }

    // --- Rechts: Funkstatus (Antennen-Symbol + OK/--) ---
    const uint16_t rx = Display::STATUS_W - 38;
    // Antenne: Mast + zwei Schrägen
    display.drawFastVLine(rx + 4, iconY, 11, GxEPD_BLACK);
    display.drawLine(rx, iconY, rx + 4, iconY + 5, GxEPD_BLACK);
    display.drawLine(rx + 8, iconY, rx + 4, iconY + 5, GxEPD_BLACK);
    display.setCursor(rx + 14, iconY + 2);
    display.print(radioConnected ? "OK" : "--");
}
