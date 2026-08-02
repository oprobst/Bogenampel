# Phase 1 — Data Model: Entkopplung von Ausschalten und Alarm

**Feature**: 006-shutdown-alarm-gesten
**Datum**: 2026-08-02

> **Autoritativ ab sofort**: Die Gesten-Tabelle in §3 ersetzt die Tabelle in
> `specs/004-v3-esp32-port/data-model.md` §3 (Zeilen „BTN1 ≥ 2 s" / „BTN1 ≥ 3 s").
> Constitution V verlangt State-Diagramm-Updates bei jeder Änderung an der
> Zustandsmaschine — die Zustände selbst bleiben unverändert, nur ihre Auslöser
> ändern sich.

## 1. Geänderte Konstanten (`Sender/Config.h`, namespace `Timing`)

| Konstante | alt | neu | Bezug |
|---|---|---|---|
| `ALARM_THRESHOLD_MS` | 2000 | **entfällt** | FR-016 |
| `POWER_OFF_HOLD_MS` | 3000 | 3000 (unverändert) | FR-001, A-001 |
| `MULTI_CLICK_COUNT` | — | **3** (neu) | FR-007 |
| `MULTI_CLICK_GAP_MS` | — | **400** (neu) | FR-007, A-002 |
| `IDLE_POWER_OFF_MS` | 20 min | **60 min** | FR-019 |
| `IDLE_POWER_OFF_MS` (DEBUG_SHORT_TIMES) | 60 s | 60 s (unverändert) | R-5 |
| `DEBOUNCE_MS` | 80 | 80 (unverändert) | — |

Randbedingung: `DEBOUNCE_MS (80) < MULTI_CLICK_GAP_MS (400) < POWER_OFF_HOLD_MS
(3000)`. Als `static_assert` in `Config.h` zu hinterlegen (Projektkonvention,
siehe bestehende Asserts ab Zeile 404).

## 2. Zustand pro Taster (`ButtonManager::ButtonState`)

Bestehende Felder unverändert; **neu** hinzu:

| Feld | Typ | Bedeutung | Reset |
|---|---|---|---|
| `clickCount` | `uint8_t` | Anzahl gültiger Drückflanken der laufenden Folge (0–3) | Zeitfenster überschritten, Taste zu lange gehalten, `resetMultiClick()` |
| `lastPressTime` | `uint32_t` | `millis()` der letzten gezählten Drückflanke | mit `clickCount` |
| `multiClickFlag` | `bool` | One-shot: Folge vollständig erkannt | Lesen über `wasMultiClicked()`, `resetMultiClick()` |
| `multiClickArmed` | `bool` | Die laufende Betätigung **würde** die Folge schließen — entschieden wird beim Loslassen | jede Loslass-Flanke, `resetMultiClick()` |

Unveränderte Felder: `pressed`, `lastRawState`, `lastChangeTime`, `pressTime`,
`clickedFlag`, `reportedHoldMs`, `bootLockout`.

### Zählregeln (in `update()`, nach dem Entprellen)

**Die Folge wird beim LOSLASSEN abgeschlossen, nicht beim Drücken.** Sonst
stünde der Alarm schon fest, bevor sich zeigt, ob der dritte Druck ein Klick
oder der Beginn eines Haltens ist — und der Edge Case „nach zwei Klicks beim
dritten Mal halten = nur ausschalten" (FR-011) wäre nicht umsetzbar.

```text
Drückflanke (entprellt):
    wenn bootLockout            → nichts (FR-004)
    sonst wenn btn == CONFIG    → clickedFlag = true   // klickt beim Drücken,
                                                       // zählt NICHT (FR-009)
    sonst (btn == OK):
        wenn clickCount > 0 und (now - lastPressTime) <= MULTI_CLICK_GAP_MS
                                → clickCount++
        sonst                   → clickCount = 1
        lastPressTime = now
        multiClickArmed = (clickCount >= MULTI_CLICK_COUNT)

Loslass-Flanke (nicht im bootLockout):
    heldMs  = now - pressTime
    wasHold = heldMs >= MULTI_CLICK_GAP_MS      // war kein Klick mehr

    wenn multiClickArmed und NICHT wasHold:
        multiClickFlag = true
        clickCount = 0
        // kein clickedFlag: der auslösende Klick verfällt (R-2)
    sonst:
        wenn wasHold → clickCount = 0           // FR-011: Halten bricht ab
        wenn btn == OK und heldMs < POWER_OFF_HOLD_MS:
            clickedFlag = true                  // OK klickt beim Loslassen
    multiClickArmed = false
```

Beim Auslösen werden zusätzlich die ausstehenden Klick-Ereignisse **beider**
Taster verworfen (`discardPendingClicks()`) — sonst quittiert ein während des
Bildaufbaus liegengebliebener Klick den Alarm sofort wieder (research.md R-2,
zweiter Nachtrag).

`lastActivity` (Grundlage der Inaktivitäts-Abschaltung) wird wie bisher bei
**jeder** entprellten Flanke gesetzt, auch im Boot-Lockout.

## 3. Gesten-Tabelle (autoritativ, ersetzt 004 §3)

| Geste | Splash | Config-Menü | Pfeile holen | Schießbetrieb | Alarm | Wartungsmodus |
|---|---|---|---|---|---|---|
| OK kurz (Klick) | Splash überspringen | bestätigen | nächste Passe / zurück | Passe beenden | Alarm quittieren | — |
| CONFIG kurz (Klick) | Splash überspringen | weiter / Wert ändern | Auswahl wechseln | — | — | — |
| **OK 3× ≤ 400 ms** | — | — | **Alarm** | **Alarm** | — (wirkungslos) | — |
| CONFIG 3× schnell | — | — | — | — | — | — |
| **beliebige Taste ≥ 3 s halten** | **Aus** | **Aus** | **Aus** | **Aus** | **Aus** | **Aus** |
| 60 min ohne Bedienung | — | **Aus** | **Aus** | — | — | — |
| beide Tasten beim Einschalten | Wartungsmodus | — | — | — | — | — |

Entfallen gegenüber 004: „BTN1 ≥ 2 s = Alarm" und die Ausnahme „im Schießbetrieb
kein Power-Off".

Weiterhin gültig: Boot-Lockout pro Taster (gesperrt bis zum ersten Loslassen)
und `MENU_LOCKOUT_MS` (400 ms Eingabesperre in `PfeileHolenMenu` nach dem
Zustandswechsel). Der Menü-Lockout wirkt **nur** auf die Menü-Aktion, nicht auf
die Zählung im `ButtonManager` — deshalb funktioniert FR-012 (Folge überlebt
Schießbetrieb → Pfeile holen) trotz Lockout.

## 4. Zustandsmaschine — geänderte Übergänge

Die fünf Zustände bleiben unverändert. Geändert werden nur Auslöser:

```text
STATE_SCHIESS_BETRIEB
  ── OK-Klick ───────────────────► STATE_PFEILE_HOLEN      (unverändert)
  ── 3× Klick (OK oder CONFIG) ──► STATE_ALARM             (NEU, ersetzt "OK ≥ 2 s")
  ── beliebige Taste ≥ 3 s ──────► Power-Off               (NEU, vorher gesperrt)

STATE_PFEILE_HOLEN
  ── 3× Klick (OK oder CONFIG) ──► STATE_ALARM             (NEU)

STATE_ALARM
  ── OK-Klick ───────────────────► STATE_PFEILE_HOLEN      (unverändert, + CMD_STOP)
  ── 3× Klick ───────────────────► (kein Übergang)         (FR-015)

alle Zustände
  ── beliebige Taste ≥ 3 s ──────► Power-Off               (jetzt zentral in update())
```

### Alarmfähigkeit (neue Hilfsfunktion)

```text
isAlarmCapable(State s) := s ∈ { STATE_SCHIESS_BETRIEB, STATE_PFEILE_HOLEN }
```

`setState(neu)` ruft `buttons.resetMultiClick()`, **außer** wenn alter **und**
neuer Zustand alarmfähig sind (R-3, FR-011 + FR-012).

## 5. Aufrufreihenfolge pro Schleifendurchlauf

```text
loopNormal()
  1. buttons.update()                    // auch aus dem e-Paper-Busy-Callback (R-1)
  2. radio.update()
  3. stateMachine.update()
       a. checkPowerOffGesture()         // NEU zentral, gilt für alle Zustände
       b. handle<Zustand>()
            - alarmfähige Zustände: zuerst Alarmfolge prüfen, dann Klicks
            - Wartezustände: checkIdleTimeout()
```

Reihenfolge in den alarmfähigen Handlern ist bindend: **Ausschalten vor Alarm
vor normalem Klick.** Damit gewinnt bei gleichzeitig erfüllten Bedingungen immer
die eindeutigere, langsamere Geste.

## 6. Keine persistenten Daten

Kein NVS-Zugriff, keine neuen Einstellungen, keine Protokolländerung. Alle neuen
Felder sind Laufzeitzustand im RAM und werden beim Einschalten mit 0
initialisiert.
