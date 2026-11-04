/**
 * @file Bruecke.cpp
 * @brief Hauptdatei der Bluetooth-Brücke (Bogenampel V3, Steckbrett-Stand)
 *
 * Bekommt die Signaltöne des Empfängers über eine UART-Leitung und spielt sie
 * über eine Bluetooth-Box. Bedienung mit dem KY-040:
 *
 * Normalbetrieb
 * - Drehen:        Lautstärke (mit kurzem Vorhörton)
 * - Kurz drücken:  Testton (1×)
 * - Lang drücken:  Box-Suche (Neustart in den Suchmodus)
 *
 * Box-Suche (auch automatisch, solange noch keine Box gewählt ist)
 * - Drehen:        durch die gefundenen Geräte blättern
 * - Kurz drücken:  Gerät wählen → wird beim nächsten Fund verbunden
 * - Lang drücken:  abbrechen, zurück zur bisherigen Box (Neustart)
 *
 * Die Box-Suche läuft bewusst über einen Neustart: im Suchmodus darf die
 * A2DP-Bibliothek die alte Box nicht wieder ansteuern, und das lässt sich nur
 * vor ihrem start() festlegen.
 *
 * Wartungsmodus: Taster beim Einschalten halten → WLAN + ArduinoOTA statt
 * Bluetooth, das LCD zeigt die IP (siehe OTAManager.h).
 *
 * @date 2026-11-03
 */

#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Preferences.h>

#include "Config.h"
#include "SignalLink.h"
#include "AudioManager.h"
#include "EncoderManager.h"
#include "OTAManager.h"

//=============================================================================
// Globale Objekte
//=============================================================================

LiquidCrystal lcd(Pins::LCD_RS, Pins::LCD_E,
                  Pins::LCD_D4, Pins::LCD_D5, Pins::LCD_D6, Pins::LCD_D7);
SignalReceiver signalLine;
AudioManager audio;
EncoderManager encoder;
Preferences prefs;

enum class Mode : uint8_t { MAIN, SCAN };
Mode mode = Mode::MAIN;
bool maintenanceMode = false;

uint8_t volume = Audio::VOLUME_DEFAULT;
bool volumeDirty = false;
uint32_t volumeChangedMs = 0;
uint32_t showVolumeUntil = 0;

uint8_t lastSignalCount = 0;

String boxName;          // Anzeigename der gespeicherten Box
uint8_t scanIndex = 0;   // Cursor in der Geräteliste
char selectedName[33] = "";

char lcdCache[2][17];
uint32_t lastDrawMs = 0;

// Funktionsprototypen
void handleMain(int16_t detents, EncoderManager::Event event);
void handleScan(int16_t detents, EncoderManager::Event event);
void restartInto(bool scan, const char* line0);
void draw();
void lcdLine(uint8_t row, const char* text);
void toLcdText(const char* in, char* out, size_t len);
bool readMaintenanceButton();
void loopMaintenance();
void showOtaProgress(uint8_t percent);

//=============================================================================
// Setup / Loop
//=============================================================================

void setup() {
    Serial.begin(115200);
    DEBUG_PRINTLN(System::VERSION);

    lcd.begin(16, 2);
    memset(lcdCache, 0, sizeof(lcdCache));
    lcdLine(0, "Bogenampel");
    lcdLine(1, "BT-Br\xF5" "cke");  // \xF5 = ü im HD44780-ROM (A00)

    // Vor allem anderen: im Wartungsmodus startet Bluetooth gar nicht erst
    maintenanceMode = readMaintenanceButton();
    if (maintenanceMode) {
        DEBUG_PRINTLN("Boot-Modus: WARTUNG");
        OTAManager::begin(showOtaProgress);
        return;
    }

    prefs.begin(Nvs::NAMESPACE, false);
    volume = prefs.getUChar(Nvs::KEY_VOLUME, Audio::VOLUME_DEFAULT);
    if (volume > Audio::VOLUME_STEPS) volume = Audio::VOLUME_DEFAULT;
    boxName = prefs.getString(Nvs::KEY_BOX_NAME, "");
    // Ohne gespeicherte Adresse kann die Bibliothek nicht neu verbinden, auch wenn
    // der Anzeigename noch da ist → dann muss die Box einmal gewählt werden
    if (!audio.hasStoredBox()) boxName = "";
    bool scanBoot = prefs.getUChar(Nvs::KEY_SCAN, 0) != 0 || boxName.length() == 0;

    encoder.begin();
    signalLine.begin();
    audio.setVolume(volume);
    audio.begin(scanBoot);

    mode = scanBoot ? Mode::SCAN : Mode::MAIN;
    DEBUG_PRINTF("Box: \"%s\", Modus: %s\n", boxName.c_str(), scanBoot ? "Suche" : "normal");
}

void loop() {
    if (maintenanceMode) {
        loopMaintenance();
        return;
    }

    encoder.update();
    audio.update();
    signalLine.update();

    uint8_t signal = signalLine.takeSignal();
    if (signal != 0) {
        audio.playSignal(signal);
        lastSignalCount = signal;
        // Verworfen > 0 heißt: Störungen auf der Leitung
        DEBUG_PRINTF("Signal %ux (Signale %lu, verworfen %lu)\n", signal,
                     (unsigned long)signalLine.signalsReceived(),
                     (unsigned long)signalLine.rejected());
    }

    int16_t detents = encoder.takeDetents();
    EncoderManager::Event event = encoder.takeEvent();
    if (mode == Mode::MAIN) {
        handleMain(detents, event);
    } else {
        handleScan(detents, event);
    }

    if (volumeDirty && millis() - volumeChangedMs >= Ui::SAVE_DELAY_MS) {
        prefs.putUChar(Nvs::KEY_VOLUME, volume);
        volumeDirty = false;
    }

    if (millis() - lastDrawMs >= 100) {
        lastDrawMs = millis();
        draw();
    }

    delay(1);
}

//=============================================================================
// Wartungsmodus
//=============================================================================

/**
 * @brief Liest den Drehgeber-Taster entprellt beim Boot
 * @return true, wenn er stabil gedrückt ist (~25 ms)
 */
bool readMaintenanceButton() {
    pinMode(Pins::ENC_SW, INPUT_PULLUP);
    delay(1);  // Pull-up einschwingen lassen
    for (uint8_t i = 0; i < 5; i++) {
        if (digitalRead(Pins::ENC_SW) != LOW) return false;
        delay(5);
    }
    return true;
}

void loopMaintenance() {
    OTAManager::handle();  // blockiert während einer Übertragung

    if (millis() - lastDrawMs < 200) return;
    lastDrawMs = millis();

    if (OTAManager::lastUpdateFailed()) {
        lcdLine(0, "Update-Fehler");
    } else {
        lcdLine(0, "Wartungsmodus");
    }

    if (!OTAManager::hasCredentials()) {
        lcdLine(1, "Keine WLAN-Daten");
    } else if (OTAManager::isConnected()) {
        lcdLine(1, OTAManager::ipAddress().c_str());
    } else {
        lcdLine(1, "WLAN verbindet");
    }
}

void showOtaProgress(uint8_t percent) {
    char line[17];
    snprintf(line, sizeof(line), "%u %%", percent);
    lcdLine(0, "Update l\xE1uft");  // \xE1 = ä im HD44780-ROM (A00)
    lcdLine(1, line);
}

//=============================================================================
// Bedienung
//=============================================================================

void handleMain(int16_t detents, EncoderManager::Event event) {
    if (detents != 0) {
        int16_t v = (int16_t)volume + detents;
        if (v < 0) v = 0;
        if (v > Audio::VOLUME_STEPS) v = Audio::VOLUME_STEPS;
        volume = (uint8_t)v;
        audio.setVolume(volume);
        audio.preview();
        volumeDirty = true;
        volumeChangedMs = millis();
        showVolumeUntil = millis() + 2000;
        lastDrawMs = 0;  // sofort zeichnen
    }

    if (event == EncoderManager::Event::SHORT_PRESS) {
        audio.playSignal(1);
    } else if (event == EncoderManager::Event::LONG_PRESS) {
        restartInto(true, "Box suchen ...");
    }
}

void handleScan(int16_t detents, EncoderManager::Event event) {
    uint8_t count = audio.deviceCount();

    if (audio.hasSelection()) {
        // Gewählt, Verbindung läuft: auf Erfolg warten
        if (audio.state() == AudioManager::State::CONNECTED) {
            boxName = selectedName;
            audio.rememberSelectedBox();
            prefs.putString(Nvs::KEY_BOX_NAME, boxName);
            prefs.putUChar(Nvs::KEY_SCAN, 0);
            mode = Mode::MAIN;
            DEBUG_PRINTF("Box gespeichert: %s\n", boxName.c_str());
        }
        return;
    }

    if (detents != 0 && count > 0) {
        int16_t i = (int16_t)scanIndex + detents;
        if (i < 0) i = 0;
        if (i >= count) i = count - 1;
        scanIndex = (uint8_t)i;
        lastDrawMs = 0;
    }

    if (event == EncoderManager::Event::SHORT_PRESS && count > 0) {
        audio.deviceName(scanIndex, selectedName, sizeof(selectedName));
        audio.selectDevice(scanIndex);
        DEBUG_PRINTF("Gewählt: %s\n", selectedName);
    } else if (event == EncoderManager::Event::LONG_PRESS && boxName.length() > 0) {
        restartInto(false, "Abbruch");
    }
}

/**
 * @brief Merkt den Startmodus im NVS und startet neu
 */
void restartInto(bool scan, const char* line0) {
    prefs.putUChar(Nvs::KEY_SCAN, scan ? 1 : 0);
    if (volumeDirty) prefs.putUChar(Nvs::KEY_VOLUME, volume);
    lcdLine(0, line0);
    lcdLine(1, "Neustart");
    delay(500);
    ESP.restart();
}

//=============================================================================
// Anzeige
//=============================================================================

void draw() {
    char l0[17];
    char l1[17];
    char name[17];
    AudioManager::State st = audio.state();

    if (mode == Mode::MAIN) {
        switch (st) {
            case AudioManager::State::CONNECTED:
                toLcdText(boxName.c_str(), l0, sizeof(l0));
                break;
            case AudioManager::State::STARTING:
                strcpy(l0, "BT startet ...");
                break;
            case AudioManager::State::CONNECTING:
                strcpy(l0, "Verbinde ...");
                break;
            default:
                strcpy(l0, "Suche Box ...");
                break;
        }

        if ((int32_t)(showVolumeUntil - millis()) > 0) {
            snprintf(l1, sizeof(l1), "Lautst\xE1rke %u/%u", volume, Audio::VOLUME_STEPS);
        } else if (lastSignalCount == 0) {
            strcpy(l1, "Kein Signal");
        } else {
            uint32_t age = (millis() - signalLine.lastSignalMs()) / 1000;
            if (age < 60) {
                snprintf(l1, sizeof(l1), "Sig %ux vor %lus", lastSignalCount, (unsigned long)age);
            } else if (age < 3600) {
                snprintf(l1, sizeof(l1), "Sig %ux vor %lum", lastSignalCount, (unsigned long)(age / 60));
            } else {
                snprintf(l1, sizeof(l1), "Sig %ux vor >1h", lastSignalCount);
            }
        }
    } else if (audio.hasSelection()) {
        strcpy(l0, "Verbinde mit");
        toLcdText(selectedName, l1, sizeof(l1));
    } else {
        uint8_t count = audio.deviceCount();
        if (count == 0) {
            strcpy(l0, st == AudioManager::State::STARTING ? "BT startet ..." : "Suche Boxen ...");
            strcpy(l1, "Box: Pairing an");
        } else {
            snprintf(l0, sizeof(l0), "Box w\xE1hlen %u/%u", scanIndex + 1, count);
            char raw[33];
            audio.deviceName(scanIndex, raw, sizeof(raw));
            toLcdText(raw, name, sizeof(name) - 1);
            snprintf(l1, sizeof(l1), ">%s", name);
        }
    }

    lcdLine(0, l0);
    lcdLine(1, l1);
}

/**
 * @brief Schreibt eine Zeile (auf 16 Zeichen aufgefüllt), nur bei Änderung
 */
void lcdLine(uint8_t row, const char* text) {
    char buf[17];
    size_t n = strlen(text);
    if (n > 16) n = 16;
    memcpy(buf, text, n);
    memset(buf + n, ' ', 16 - n);
    buf[16] = '\0';

    if (memcmp(buf, lcdCache[row], 16) == 0) return;
    memcpy(lcdCache[row], buf, 17);
    lcd.setCursor(0, row);
    lcd.print(buf);
}

/**
 * @brief Gerätenamen (UTF-8) für das HD44780-ROM aufbereiten
 *
 * ä/ö/ü/ß werden auf die ROM-Zeichen abgebildet, alles andere außerhalb von
 * ASCII wird zu '?'.
 */
void toLcdText(const char* in, char* out, size_t len) {
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o + 1 < len; i++) {
        uint8_t c = (uint8_t)in[i];
        if (c == 0xC3 && in[i + 1] != '\0') {
            uint8_t c2 = (uint8_t)in[++i];
            switch (c2) {
                case 0xA4: case 0x84: out[o++] = (char)0xE1; break;  // ä Ä
                case 0xB6: case 0x96: out[o++] = (char)0xEF; break;  // ö Ö
                case 0xBC: case 0x9C: out[o++] = (char)0xF5; break;  // ü Ü
                case 0x9F:            out[o++] = (char)0xE2; break;  // ß
                default:              out[o++] = '?';        break;
            }
        } else if (c >= 0x20 && c < 0x7F) {
            out[o++] = (char)c;
        } else if (c < 0x80 || c >= 0xC0) {
            out[o++] = '?';  // Steuerzeichen bzw. Beginn eines UTF-8-Zeichens
        }
        // UTF-8-Folgebytes (0x80-0xBF) still überspringen
    }
    out[o] = '\0';
}
