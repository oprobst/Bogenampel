# Contract: Tasten-Gesten-API (`ButtonManager`, Sender V3)

**Feature**: 006-shutdown-alarm-gesten
**Datum**: 2026-08-02
**Gilt für**: `Sender/ButtonManager.h`, `Sender/ButtonManager.cpp`

Dieses Feature hat keine Netzwerk-API. Der Vertrag zwischen `ButtonManager`
(Gestenerkennung) und `StateMachine` (Auswertung) tritt an ihre Stelle. Die
Rollen-zu-Pin-Zuordnung bleibt ausschließlich in `readRawState()` und ist nicht
Teil dieses Vertrags.

## 1. Öffentliche Schnittstelle

### Unverändert

```cpp
void     begin();
void     update();
bool     isPressed(Button btn) const;
bool     wasClicked(Button btn);                 // one-shot
bool     wasHeldFor(Button btn, uint16_t ms);    // one-shot pro Schwelle
bool     isAnyPressed() const;
bool     isBootLockoutActive(Button btn) const;
uint32_t lastActivityMs() const;
```

### Neu

```cpp
/**
 * @brief Mehrfachklick-Geste erkannt (one-shot, Flag wird beim Lesen gelöscht)
 *
 * true genau einmal, wenn derselbe Taster MULTI_CLICK_COUNT (3) Mal mit
 * höchstens MULTI_CLICK_GAP_MS (400 ms) Abstand zwischen den Drückflanken
 * betätigt wurde. Der auslösende Klick erzeugt KEIN wasClicked()-Ereignis.
 */
bool wasMultiClicked(Button btn);

/**
 * @brief Verwirft laufende Mehrfachklick-Folgen und ausstehende Flags (beide Taster)
 *
 * Von StateMachine::setState() bei jedem Wechsel aufzurufen, der nicht zwischen
 * zwei alarmfähigen Zuständen stattfindet.
 */
void resetMultiClick();
```

## 2. Garantien

| # | Garantie | Anforderung |
|---|---|---|
| G-1 | `wasMultiClicked()` liefert nach einer erkannten Folge **genau einmal** `true`; jeder weitere Aufruf `false`, bis eine neue Folge vollständig ist. | FR-007 |
| G-2 | Gezählt werden ausschließlich OK-Drückflanken; CONFIG-Klicks zählen weder mit noch brechen sie eine laufende Folge ab. | FR-008 |
| G-3 | `wasMultiClicked(Button::CONFIG)` liefert **immer** `false` — CONFIG hat keine Alarmfolge. | FR-009 |
| G-4 | Der Klick, der die Folge vervollständigt, erzeugt **kein** `wasClicked()`-Ereignis (weder beim Drücken noch beim Loslassen). | FR-013, R-2 |
| G-4b | Beim Auslösen der Folge werden **alle** ausstehenden Klick-Ereignisse beider Taster verworfen (`discardPendingClicks()`) — auch die von Klick 1 und 2, die während eines blockierenden Bildaufbaus liegen geblieben sind. | FR-013, R-2 |
| G-4a | Die Folge wird auf der **Loslass**-Flanke abgeschlossen. Wird die abschließende Betätigung länger als `MULTI_CLICK_GAP_MS` gehalten, gilt sie als Beginn eines Haltens: kein Alarm, Folge verworfen. | FR-011, R-2 |
| G-5 | Die Zählung bricht ab, sobald zwischen zwei Drückflanken mehr als `MULTI_CLICK_GAP_MS` liegen oder eine Betätigung länger als `MULTI_CLICK_GAP_MS` dauert. | FR-011 |
| G-6 | Betätigungen im Boot-Lockout zählen nicht — weder für die Folge noch für Halte-Gesten. | FR-004 |
| G-7 | `wasHeldFor(btn, POWER_OFF_HOLD_MS)` liefert für **beide** Taster identisch `true` nach ≥ 3 s ununterbrochenem Halten. | FR-001, FR-003 |
| G-8 | Ein Klick entsteht auf der Rolle OK beim Loslassen nur, wenn die Betätigung kürzer als `POWER_OFF_HOLD_MS` war. | R-4 |
| G-9 | `lastActivityMs()` liefert den Zeitstempel der letzten entprellten Flanke — inklusive Boot-Lockout-Flanken und inklusive der Klicks einer Alarmfolge. | FR-020 |
| G-10 | `update()` ist reentranzfrei nutzbar aus dem GxEPD2-Busy-Callback: sie liest nur GPIOs, schreibt nur eigene Felder und ruft nichts Blockierendes auf. | R-1 |

## 3. Pflichten des Aufrufers (`StateMachine`)

| # | Pflicht |
|---|---|
| P-1 | `checkPowerOffGesture()` wird **einmal zentral** in `update()` vor der Zustandsbehandlung aufgerufen — nicht mehr in den einzelnen Handlern. |
| P-2 | `wasMultiClicked()` wird **ausschließlich** in `handleSchiessBetrieb()` und `handlePfeileHolen()` abgefragt (FR-010). |
| P-3 | Innerhalb eines alarmfähigen Handlers gilt die Reihenfolge: Ausschalten → Alarmfolge → normale Klicks. |
| P-4 | `setState()` ruft `resetMultiClick()`, außer wenn alter **und** neuer Zustand alarmfähig sind. |
| P-5 | Nach `setState(STATE_ALARM)` wird im selben Durchlauf kein Klick mehr ausgewertet (`return` direkt nach dem Zustandswechsel). |
| P-6 | `enterAlarm()` ruft am Ende `discardPendingClicks()` — Senden und Refresh blockieren ~1 s, in der der Busy-Callback weiter Klicks sammelt. |

## 4. Vertrag mit dem Display-Layer

```cpp
// EpaperDisplay (neu, Passthrough auf GxEPD2)
void setBusyCallback(void (*cb)(const void*), const void* param = nullptr);
```

| # | Garantie |
|---|---|
| D-1 | Ist ein Callback registriert, ruft GxEPD2 ihn während jedes BUSY-Wartens kontinuierlich auf. |
| D-2 | Der Callback wird in `setup()` einmalig auf eine statische Funktion gesetzt, die `buttons.update()` aufruft. |
| D-3 | Der Callback darf weder zeichnen noch einen Refresh anstoßen (Reentranz in GxEPD2). |

## 5. Nicht Teil dieses Vertrags

- Funkprotokoll (`Commands.h`) — byte-identisch unverändert
- Empfänger-Firmware — nicht angefasst
- Pin-Belegung, Beschaltung, KiCad-Projekt — unverändert (R-7)
- Wartungsmodus-Pfad (`loopOta()`) — nutzt weiterhin nur `wasHeldFor()`
