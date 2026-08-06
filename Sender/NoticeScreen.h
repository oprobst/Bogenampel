/**
 * @file NoticeScreen.h
 * @brief Vollbild-Warnhinweis für nicht zugestellte Kommandos (e-Paper)
 *
 * Gegenstück zum AlarmScreen für den Fall, dass ein sicherheitsrelevantes
 * Kommando NICHT bestätigt wurde. Der Bediener muss das erfahren: Bleibt ein
 * manueller Abbruch beim Empfänger aus, zählt die Ampel draußen weiter grün,
 * während der Sender längst in "Pfeile holen" steht.
 */

#pragma once

#include "Config.h"
#include "EpaperDisplay.h"

class NoticeScreen {
public:
    NoticeScreen(EpaperDisplay& epd);

    /**
     * @brief Zeichnet den Hinweis "Abbruch nicht zugestellt" (ohne Refresh!)
     *
     * Bewusst in derselben invertierten Optik wie der Alarm-Screen — beides
     * sind Situationen, in denen der Bediener sofort handeln muss.
     */
    void drawStopFailed();

private:
    EpaperDisplay& epd;
};
