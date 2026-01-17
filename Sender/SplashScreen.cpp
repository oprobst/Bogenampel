/**
 * @file SplashScreen.cpp
 * @brief Splash Screen Implementierung (nur UI)
 */

#include "SplashScreen.h"

SplashScreen::SplashScreen(Adafruit_ST7789& tft)
    : display(tft) {
}

void SplashScreen::draw() {
    // Hintergrund schwarz
    display.fillScreen(ST77XX_BLACK);

    // Layout-Berechnungen (bei Rotation 1: 320x240)
    const int16_t centerY = display.height() / 2;
    const int16_t centerX = display.width() / 2;

    // Logo-Text "BOGENAMPEL" - groß und zentriert
    display.setTextColor(ST77XX_GREEN);
    display.setTextSize(3);

    // Text-Bounds für "BOGENAMPEL" ermitteln
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("BOGENAMPEL", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centerX - w/2, centerY - 75 - h/2);  // 15 Pixel nach oben verschoben
    display.print("BOGENAMPEL");

    // Rahmen um Logo (Portrait: 240 Breite)
    display.drawRect(centerX - 110, centerY - 95, 220, 60, ST77XX_GREEN);  // 15 Pixel nach oben verschoben

    // Versions-Text
    display.setTextColor(ST77XX_WHITE);
    display.setTextSize(2);
    display.getTextBounds("Bogenampel V1.0", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centerX - w/2, centerY + 20 - h/2);
    display.print("Bogenampel V2.3");

    // Hinweis zum Überspringen
    display.setTextColor(Display::COLOR_GRAY);
    display.setTextSize(1);
    display.getTextBounds("Taste druecken zum Ueberspringen", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centerX - w/2, centerY + 80 - h/2);
    display.print("Taste druecken zum Ueberspringen");

    // Verbindungsstatus (initial)
    updateConnectionStatus("Suche Empfaengermodul...");

    // RF-Konfiguration anzeigen (unten, klein, grau)
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);

    // Zeile 1: Kanal und Data Rate (linksbündig statt zentriert spart Code)
    display.setCursor(10, STATUS_Y + 25);
    display.print(F("RF: Ch"));
    display.print(RF::CHANNEL);
    display.print(F("  "));
    switch (RF::DATA_RATE) {
        case RF24_250KBPS: display.print(F("250K")); break;
        case RF24_1MBPS:   display.print(F("1M")); break;
        case RF24_2MBPS:   display.print(F("2M")); break;
    }

    // Zeile 2: Power Level (linksbündig statt zentriert spart Code)
    display.setCursor(10, STATUS_Y + 35);
    display.print(F("Power: "));
    switch (RF::POWER_LEVEL) {
        case RF24_PA_MIN:  display.print(F("MIN (-18dBm)")); break;
        case RF24_PA_LOW:  display.print(F("LOW (-12dBm)")); break;
        case RF24_PA_HIGH: display.print(F("HIGH (-6dBm)")); break;
        case RF24_PA_MAX:  display.print(F("MAX (0dBm)")); break;
    }
}

void SplashScreen::updateConnectionStatus(const char* status) {
    // Bereich für Statustext löschen (zentriert, unten)
    const int16_t centerX = display.width() / 2;

    // Bereich löschen (großzügig für längere Texte)
    display.fillRect(0, STATUS_Y - 5, display.width(), 20, ST77XX_BLACK);

    // Statustext zentriert zeichnen
    int16_t x1, y1;
    uint16_t w, h;
    display.setTextSize(1);
    display.setTextColor(ST77XX_CYAN);
    display.getTextBounds(status, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centerX - w/2, STATUS_Y);
    display.print(status);
}

void SplashScreen::showConnectionQuality(uint8_t qualityPercent) {
    const int16_t centerX = display.width() / 2;
    const int16_t centerY = display.height() / 2;

    // Bereich für Qualitätsanzeige löschen (Mitte des Displays)
    display.fillRect(0, centerY - 30, display.width(), 70, ST77XX_BLACK);

    // Überschrift "Verbindung"
    display.setTextSize(1);
    display.setTextColor(ST77XX_WHITE);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("Verbindung", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centerX - w/2, centerY - 25);
    display.print("Verbindung");

    // Prozentanzeige (mittel)
    display.setTextSize(3);

    // Farbe abhängig von Qualität
    uint16_t color;
    if (qualityPercent >= 80) {
        color = ST77XX_GREEN;      // Sehr gut (80-100%)
    } else if (qualityPercent >= 50) {
        color = Display::COLOR_ORANGE;  // Mittel (50-79%)
    } else {
        color = ST77XX_RED;        // Schlecht (0-49%)
    }

    // Zentrier-Position berechnen (nutze "100%" als Worst-Case für Breite)
    display.setTextColor(color);
    display.getTextBounds("100%", 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centerX - w/2, centerY - 5);
    display.print(qualityPercent);
    display.print(F("%"));

    // Balken-Anzeige (unter Prozentanzeige)
    const int16_t barWidth = 160;
    const int16_t barHeight = 12;
    const int16_t barX = centerX - barWidth/2;
    const int16_t barY = centerY + 20;

    // Rahmen
    display.drawRect(barX, barY, barWidth, barHeight, ST77XX_WHITE);

    // Gefüllter Balken (abhängig von Qualität)
    int16_t fillWidth = (barWidth - 4) * qualityPercent / 100;
    if (fillWidth > 0) {
        display.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, color);
    }

    // Text darunter (optional entfernen für noch mehr Platz)
    display.setTextSize(1);
    display.setTextColor(Display::COLOR_GRAY);
    const char* qualityText;
    if (qualityPercent >= 80) {
        qualityText = "Sehr gut";
    } else if (qualityPercent >= 50) {
        qualityText = "Mittel";
    } else if (qualityPercent > 0) {
        qualityText = "Schlecht";
    } else {
        qualityText = "Keine Verbindung";
    }
    display.getTextBounds(qualityText, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(centerX - w/2, barY + barHeight + 5);
    display.print(qualityText);
}
