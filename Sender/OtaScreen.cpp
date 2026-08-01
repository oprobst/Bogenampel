/**
 * @file OtaScreen.cpp
 * @brief Implementierung der Wartungsmodus-Anzeige
 */

#include "OtaScreen.h"

#include <string.h>

OtaScreen::OtaScreen(EpaperDisplay& epdRef)
    : epd(epdRef)
    , lastPhase(OTAManager::Phase::CONNECTING)
    , drawn(false) {
    lastIp[0] = '\0';
}

void OtaScreen::captureState() {
    lastPhase = OTAManager::phase();
    strncpy(lastIp, OTAManager::ipString(), sizeof(lastIp) - 1);
    lastIp[sizeof(lastIp) - 1] = '\0';
}

void OtaScreen::draw() {
    Adafruit_GFX& g = epd.gfx();

    // Zustand zuerst festhalten — siehe captureState()
    captureState();

    g.fillScreen(GxEPD_WHITE);

    // Titelbalken invertiert — hebt den Sondermodus deutlich vom Normalbetrieb ab
    g.fillRect(0, 0, EpaperDisplay::WIDTH, 26, GxEPD_BLACK);
    g.setTextColor(GxEPD_WHITE);
    epd.printCentered("WARTUNGSMODUS", 9, 1);
    g.setTextColor(GxEPD_BLACK);

    // WLAN-Name (bei zu langer SSID schneidet das Panel rechts ab — die
    // Zeile ist nur eine Erinnerung, welches Netz konfiguriert ist)
    g.setTextSize(1);
    g.setCursor(6, 36);
    g.print("WLAN:");
    g.setCursor(6, 48);
    g.print(strlen(OTA::WIFI_SSID) > 0 ? OTA::WIFI_SSID : "(keine Zugangsdaten)");

    // Dynamischer Teil
    drawStatus();

    // Fußzeile: Hostname als Alternative zur IP
    g.setTextSize(1);
    g.setCursor(6, EpaperDisplay::HEIGHT - 22);
    g.print(OTA::HOSTNAME);
    g.setCursor(6, EpaperDisplay::HEIGHT - 10);
    g.print("Taste 3s = aus");

    epd.fullRefresh();
    drawn = true;
}

void OtaScreen::drawStatus() {
    Adafruit_GFX& g = epd.gfx();

    g.fillRect(0, STATUS_Y, EpaperDisplay::WIDTH, STATUS_H, GxEPD_WHITE);
    g.setTextColor(GxEPD_BLACK);

    switch (lastPhase) {
        case OTAManager::Phase::UPDATING:
            epd.printCentered("Update laeuft", STATUS_Y + 24, 2);
            epd.printCentered("bitte warten...", STATUS_Y + 52, 1);
            epd.printCentered("Neustart am Ende", STATUS_Y + 70, 1);
            break;

        case OTAManager::Phase::READY: {
            epd.printCentered("Verbunden", STATUS_Y + 6, 1);

            // IP prominent: das ist der Wert, den man in platformio.ini braucht.
            // Größe 2 = 12 px/Zeichen; "255.255.255.255" (15 Zeichen) füllt mit
            // 180 px die 200-px-Breite gerade noch aus.
            g.drawRect(6, STATUS_Y + 20, EpaperDisplay::WIDTH - 12, 32, GxEPD_BLACK);
            epd.printCentered(lastIp, STATUS_Y + 30, 2);

            epd.printCentered("Warte auf Update", STATUS_Y + 62, 1);
            epd.printCentered("Port 3232", STATUS_Y + 78, 1);
            break;
        }

        case OTAManager::Phase::CONNECTING:
        default:
            if (strlen(OTA::WIFI_SSID) > 0) {
                epd.printCentered("Verbinde mit WLAN", STATUS_Y + 30, 1);
                epd.printCentered("...", STATUS_Y + 50, 2);
            } else {
                epd.printCentered("Keine WLAN-Daten", STATUS_Y + 24, 1);
                epd.printCentered("wifi_credentials.h", STATUS_Y + 44, 1);
                epd.printCentered("fehlt", STATUS_Y + 60, 1);
            }
            break;
    }
}

void OtaScreen::update() {
    if (!drawn) return;

    OTAManager::Phase p = OTAManager::phase();
    const char* ip = OTAManager::ipString();

    if (p == lastPhase && strcmp(ip, lastIp) == 0) {
        return;  // nichts Neues — Panel nicht anfassen
    }

    captureState();
    drawStatus();
    epd.partialUpdate(0, STATUS_Y, EpaperDisplay::WIDTH, STATUS_H);
}

void OtaScreen::showUpdating() {
    if (!drawn) return;

    lastPhase = OTAManager::Phase::UPDATING;
    drawStatus();
    epd.partialUpdate(0, STATUS_Y, EpaperDisplay::WIDTH, STATUS_H);
}
