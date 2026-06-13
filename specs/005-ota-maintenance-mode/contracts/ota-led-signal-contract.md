# Contract: OTA-Mode Status-LED Signalling

Applies only when `BootMode = OTA_MAINTENANCE`. Hardware: status LED on **D9 / GPIO9, active-low**
(`STATUS_LED_ON = LOW`, `STATUS_LED_OFF = HIGH`; 3V3 → LED → R6 → pin, Strapping-Fix Befund 3).
All patterns MUST be produced **non-blocking** (driven from `millis()` each loop), so
`ArduinoOTA.handle()` is never starved — FR-004c runs *during* the transfer.

## Patterns (FR-004a / FR-004b / FR-004c)

| Phase | Period | LED behavior within a period | Drives from |
|-------|--------|------------------------------|-------------|
| `CONNECTING` (FR-004a) | 600 ms | ON ~100 ms, then OFF ~500 ms (fast short flash) | `WiFi.status() != WL_CONNECTED` or `WiFi.localIP()` is `0.0.0.0` |
| `READY` (FR-004b) | 1000 ms | ON ~500 ms, OFF ~500 ms (slow, even) | connected + valid IP, no transfer in progress |
| `UPDATING` (FR-004c) | 600 ms | ON ~500 ms, then OFF ~100 ms (mostly lit, brief dark blip) | ArduinoOTA `onStart` … `onEnd` |

Timings are targets with ±~50 ms tolerance (Spec Assumptions). The three patterns MUST stay mutually
distinguishable by eye and MUST NOT match the normal-mode indications (steady-on "ready" or the
event-driven receive-blink) — FR-011.

## Phase source of truth

- Phase is recomputed each loop: default to `CONNECTING`; promote to `READY` when
  `WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE`; force `UPDATING` between the
  ArduinoOTA `onStart` and `onEnd`/`onError` callbacks (a boolean set in those callbacks overrides
  the WiFi-derived phase).
- On `onError`, clear the updating flag → the next loop falls back to `READY` or `CONNECTING`
  depending on the live WiFi state (FR-010, stays in maintenance mode).

## Acceptance (maps to SC-004, US3)

1. Power on with button held, WiFi briefly unavailable → LED shows the **fast short flash**.
2. WiFi connects and an IP is assigned → LED switches to the **slow even blink**.
3. Start a flash from the workstation → LED switches to **mostly-lit with brief blips** until the
   transfer finishes (then the device reboots).
4. In NORMAL mode none of these three patterns ever appears.
