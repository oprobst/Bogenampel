# Data Model: Bogenampel V3 Hardware Port

**Feature**: 004-v3-esp32-port | **Date**: 2026-06-09

Die fachlichen Entitäten sind eine 1:1-Übernahme aus V2; neu sind der ESP-NOW-Frame, der
Power-Zustand des Senders und die Empfänger-Lokalregler. Quellen: `Sender/Config.h`,
`Sender/Commands.h`, `Sender/StateMachine.cpp`, `Empfaenger/Empfaenger.ino` (V2).

## 1. TournamentConfig (Sender, persistent in NVS)

| Feld | Typ | Werte | Default | Validierung |
|------|-----|-------|---------|-------------|
| `shootingTime` | uint16 | 120, 240 (Sekunden) | 120 | Whitelist, sonst Default |
| `shooterCount` | uint8 | 2 (= „1-2 Schützen"), 4 (= „3-4 Schützen") | 2 | Whitelist, sonst Default |

- **Speicherort**: NVS, Namespace `bogenampel`, Keys `timeS`, `count` (R-6).
- **Schreibzeitpunkt**: sofort bei Bestätigung im ConfigMenu (Edge Case „Empty battery").
- **Lesezeitpunkt**: Boot; fehlende/ungültige Werte → Defaults (FR-005). Die V2-CRC8 entfällt
  (NVS sichert Integrität selbst), die Wertebereichsprüfung bleibt.

## 2. RadioPacketV3 (Funkprotokoll, beide Geräte)

6 Bytes, `__attribute__((packed))` — Details siehe `contracts/espnow-protocol.md`:

| Byte | Feld | Beschreibung |
|------|------|--------------|
| 0–1 | `magic` | `'B','3'` — Filter gegen fremde ESP-NOW-Frames |
| 2 | `type` | FrameType: `CMD`, `HELLO`, `HELLO_ACK`, `PING`, `PING_ACK` |
| 3 | `seq` | Sequenznummer (Dedup bei Retries, FR-008) |
| 4 | `command` | RadioCommand (nur bei `type == CMD`, sonst 0) |
| 5 | `checksum` | `command ^ 0xFF` (V2-Semantik) |

**RadioCommand** (unverändert aus V2 `Commands.h`, FR-002): `CMD_STOP=0x01`, `CMD_START_120=0x02`,
`CMD_START_240=0x03`, `CMD_INIT=0x04`, `CMD_ALARM=0x05`, `CMD_PING=0x06`, `CMD_GROUP_AB=0x08`,
`CMD_GROUP_CD=0x09`, `CMD_GROUP_NONE=0x0A`, `CMD_GROUP_FINISH_AB=0x0B`, `CMD_GROUP_FINISH_CD=0x0C`.

**TransmissionResult** (V2-API erhalten): `TX_SUCCESS`, `TX_TIMEOUT`, `TX_ERROR`.

## 3. Sender-Zustandsmaschine (PORT aus V2, + Power-Off)

```text
            ┌──────────┐  Splash-Ende (15 s) / Quality-Test fertig
  Boot ───► │  SPLASH  │ ───────────────────────────────┐
  (LATCH=1) └──────────┘                                ▼
            ┌───────────────────────────────────► CONFIG_MENU ◄──── „Neustart"
            │                                        │ „Start" (sendet CMD_INIT,
            │                                        ▼  CMD_GROUP_*)
            │       „Nächste Passe" (CMD_START_*) PFEILE_HOLEN ◄────────────┐
            │      ┌─────────────────────────────────┘                      │
            │      ▼                                                        │
            │  SCHIESS_BETRIEB ── Zeitablauf (KEIN Funk! FR-004a) ──────────┤
            │      │  │            └─ bei 3-4 Schützen POS_1: 2. Gruppe     │
            │      │  │               (lokaler Restart + CMD_START als Sync)│
            │      │  └── „Passe beenden" (CMD_STOP) ───────────────────────┘
            │      └── BTN1 ≥ 2 s gehalten (CMD_ALARM) ──► ALARM
            │                                               │ quittieren (CMD_STOP)
            └───────────────────────────────────────────────┘ → PFEILE_HOLEN

  Power-Off (orthogonal): BTN1 ≥ 3 s in SPLASH/CONFIG_MENU/PFEILE_HOLEN/ALARM
  → Abschalt-Screen → Display hibernate → LOAD aus → LATCH=0.
  In SCHIESS_BETRIEB nicht verfügbar (BTN1-Geste = Alarm, FR-015).
```

**Zustandsdaten** (wie V2): `currentGroup` (AB/CD), `currentPosition` (POS_1/POS_2),
`inPreparationPhase`, `preparationSecondsRemaining`, `shootingSecondsRemaining`.

**Gruppen-4-Zyklus** (`advanceToNextGroup`, exakt wie V2): 
`AB/POS_1 → CD/POS_2 → CD/POS_1 → AB/POS_2 → AB/POS_1 …`
Bei 1-2 Schützen: keine Gruppen (CMD_GROUP_NONE).

**Taster-Gesten (V3-Mapping)**:

| Geste | SPLASH | CONFIG_MENU | PFEILE_HOLEN | SCHIESS_BETRIEB | ALARM |
|-------|--------|-------------|--------------|-----------------|-------|
| BTN2 kurz („Rechts") | — | Auswahl weiter (Wrap) | Menüpunkt weiter (Wrap) | — | — |
| BTN1 kurz („OK") | Splash überspringen | bestätigen | Menüpunkt ausführen | Passe beenden | Alarm quittieren |
| BTN1 ≥ 2 s | — | — | — | **Alarm** | — |
| BTN1 ≥ 3 s | Power-Off | Power-Off | Power-Off | — (durch Alarm belegt) | Power-Off |

Boot-Lockout: alle BTN1-Gesten gesperrt, bis BTN1 nach dem Einschalt-Druck einmal losgelassen
wurde; zusätzlich V2-`MENU_LOCKOUT_MS` (400 ms) nach jedem Zustandswechsel.

## 4. Empfänger-Zustandsmodell (PORT aus V2, autonom)

```text
  IDLE/INIT ── CMD_START_120/240 ──► PREPARATION (10 s, rot, 2 Pieptöne)
     ▲                                   │ Ablauf (autonom)
     │                                   ▼
     │                              SHOOTING (grün; letzte 30 s orange; 1 Piepton bei Start)
     │                                   │
     │             ┌─────────────────────┼──────────────────────┐
     │     CMD_STOP (manuell)    Zeitablauf, POS_2 oder   Zeitablauf, POS_1 bei
     │     3 Pieptöne, rot       keine Gruppen: 3 Pieptöne,    3-4 Schützen:
     │             │             rot, „000" (autonom!)    autonomer Gruppenwechsel
     │             ▼                     │                + neue PREPARATION ──┐
     └──────── STOPPED ◄─────────────────┘                                     │
                   ▲                                      (CMD_START als Sync  │
                   │                                       wird ignoriert,     │
            CMD_ALARM (jederzeit): 8× blinken rot,         wenn PREPARATION    │
            alle LEDs, danach STOPPED                      schon läuft) ◄──────┘
```

**Zustandsdaten** (wie V2): `timerRunning`, `timerRemainingSeconds`, `inPreparationPhase`,
`preparationRemainingSeconds`, `groupsEnabled`, `currentGroup`, `currentPosition`,
`alarmActive`, `alarmBlinkCount`.

**Autonomie-Invariante (FR-004)**: Übergänge bei Zeitablauf benötigen keinen Funkempfang.
**Dedup-Invariante (FR-008)**: identische `seq` → Frame verwerfen; CMD_START während laufender
PREPARATION der zweiten Gruppe → ignorieren (V2-Verhalten, `Empfaenger.ino:827`).

## 5. PowerState (Sender, NEU)

| Feld | Quelle | Werte/Bedeutung |
|------|--------|-----------------|
| `batteryMv` | ADC GPIO10 × 2,5, Median-5 | 3000–4200 mV (LiPo) |
| `batteryPercent` | linear aus `batteryMv` | 0–100 %, Warnung < 3300 mV (V2-Schwellen) |
| `usbConnected` | GPIO8 (aktiv HIGH) | USB-Versorgung anliegend |
| `chargeState` | GPIO11/12/17 decodiert (MCP73837) | `CHARGING`, `COMPLETE`, `FAULT`, `NO_INPUT` |
| `latchHeld` | GPIO16 Ausgang | true ab Boot bis Power-Off |

Aktualisierung alle 5 s (V2 `Battery::UPDATE_INTERVAL_MS`), Anzeige in der Statuszeile
(Partial-Refresh-Fenster).

## 6. Empfänger-Lokalregler (NEU)

| Feld | Quelle | Abbildung |
|------|--------|-----------|
| `volume` | Poti D0 (GPIO2, ADC1), ≥10 Hz gelesen | quadratische Kurve → LEDC-Duty 0–50 %, Minimum hörbar |
| `brightness` | Poti D1 (GPIO3, ADC1) **nach Umverdrahtung** | linear → FastLED-Brightness 64–255 (V2) |
| `fanSpeed` | Poti D2 (GPIO4, ADC1), ≥10 Hz gelesen | linear → LEDC-PWM auf D6 (≥ 25 kHz); kein Tacho (nicht angeschlossen) |
| `statusLed` | D9 (**aktiv LOW**, Strapping-Befund 3) | an = bereit, kurzes Aus-Blinken bei Frame-Empfang (ersetzt 3 V2-LEDs) |

## 7. e-Paper-Anzeigemodell (Sender, NEU)

| Bereich | Inhalt | Refresh |
|---------|--------|---------|
| Statuszeile (oben, ~200×24 px) | Akku %, USB/Lade-Symbol, Funkstatus | partial, bei Änderung |
| Hauptbereich | Screen-abhängig (Menü, Countdown groß, Alarm) | voll bei Screenwechsel |
| Countdown-Fenster (zentriert, ~120×64 px) | mm:ss bzw. Sekunden | partial, 1 Hz |

Ghosting-Regel: Voll-Refresh bei jedem Zustandswechsel, mindestens einmal pro Passe-Zyklus (R-2).
