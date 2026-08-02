/**
 * @file ShutdownScreen.h
 * @brief Abschalt-Screen (e-Paper)
 *
 * Das Bild bleibt nach dem Abschalten stehen — e-Paper ist bistabil und
 * braucht keinen Strom, um den Inhalt zu halten. Es ist damit das, was der
 * Bediener sieht, solange das Gerät aus im Koffer oder am Ladekabel liegt.
 * Entsprechend steht hier kein Abschieds-Text, sondern die einzige Information,
 * die in diesem Zustand nützlich ist: wie man das Gerät wieder anbekommt.
 *
 * Gezeichnet wird an zwei Stellen (Normalbetrieb über StateMachine::doPowerOff,
 * Wartungsmodus über loopOta) — deshalb eine gemeinsame Klasse statt zweier
 * Kopien, die auseinanderlaufen.
 */

#pragma once

#include "Config.h"
#include "EpaperDisplay.h"

class ShutdownScreen {
public:
    ShutdownScreen(EpaperDisplay& epd);

    /**
     * @brief Zeichnet den Abschalt-Screen in den Puffer (ohne Refresh!)
     * @param reason optionale Zeile über dem Einschalt-Hinweis; nullptr = keine
     * @param batteryPercent Akkustand 0-100 für die Fußzeile; -1 = nicht anzeigen
     *
     * Der Grund wird nur bei der automatischen Abschaltung gesetzt (FR-022):
     * Wer sein Gerät nach einer Stunde ausgeschaltet vorfindet, soll das nicht
     * für einen Defekt halten. Beim Ausschalten von Hand bleibt die Zeile leer.
     *
     * Der Akkustand ist bewusst als Momentaufnahme beschriftet: Am Ladekabel
     * bleibt das Bild stehen, während der Akku weiterlädt — der Wert altert
     * also, ohne dass ihn jemand aktualisieren könnte. Der Controller läuft
     * beim Laden nicht mit; VBUS hat keinen Pfad zum Enable des TPS62742
     * (geprüft an der Netzliste 2026-08-02: EN hängt nur an D1/Taster+LATCH).
     */
    void draw(const char* reason = nullptr, int16_t batteryPercent = -1);

private:
    EpaperDisplay& epd;
};
