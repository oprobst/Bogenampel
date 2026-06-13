# Phase 1 Data Model: Receiver OTA Maintenance Mode

This feature has no persisted data. The "model" is the in-RAM state that governs boot-time control
flow and the LED signaller. Two small state types capture it.

## Entity: BootMode

The single decision made once at startup; fixed for the rest of the power cycle (FR-001, FR-007).

| Value | Meaning | Selected when |
|-------|---------|---------------|
| `NORMAL` | Tournament/display operation over ESP-NOW | Button **not** held at boot |
| `OTA_MAINTENANCE` | WiFi station joined, awaiting an authenticated OTA flash; ESP-NOW and timer **not** started | Button held (stable active-low) at boot |

**Lifecycle**: decided in `setup()` after strapping-pin fixes; never changes until reset/power-cycle.
Releasing the button afterwards has no effect (FR-007). Represented as a boolean flag
(`bootOtaMode`) or enum in `EmpfaengerV3.cpp`.

**Validation / rules**:
- Decision MUST be made before any radio init and before the boot rainbow effect (R1, R2).
- In `OTA_MAINTENANCE`, `radio.begin()` (ESP-NOW) MUST NOT be called (FR-002, FR-006).
- In `NORMAL`, no WiFi join and no SoftAP MUST occur (FR-005).

## Entity: OtaPhase (only meaningful in `BootMode = OTA_MAINTENANCE`)

Drives the status-LED pattern (FR-004a/b/c) and reflects the OTA progress.

| Value | Meaning | LED pattern (D9, active-low) |
|-------|---------|------------------------------|
| `CONNECTING` | WiFi not yet associated or no IP assigned | fast short flash: ~100 ms on / ~500 ms off (period 600 ms) |
| `READY` | Associated **and** IP assigned; idle, listening for OTA | slow even blink: ~500 ms on / ~500 ms off (period 1000 ms) |
| `UPDATING` | Firmware transfer in progress (ArduinoOTA active) | mostly lit, ~100 ms dark blip every ~500 ms (period 600 ms) |

### State transitions

```text
            power-on (button held)
                    │
                    ▼
             ┌─────────────┐   WiFi associated AND localIP() valid
             │ CONNECTING  │ ─────────────────────────────────────► ┌────────┐
             │ (fast flash)│ ◄───────────────────────────────────── │ READY  │
             └─────────────┘   connection lost / IP dropped         │ (slow  │
                    ▲                                                │  blink)│
                    │                                                └────────┘
                    │ ArduinoOTA onEnd (success → device reboots;          │
                    │ on error → back toward READY/CONNECTING)             │ ArduinoOTA onStart
                    │                                                       ▼
                    │                                              ┌──────────────┐
                    └───────────────────────────────────────────  │  UPDATING    │
                                                                   │ (lit+blips)  │
                                                                   └──────────────┘
                                                                          │ success
                                                                          ▼
                                                                   device restarts
                                                                   → next boot decides
                                                                     BootMode again
```

**Transition rules**:
- `CONNECTING → READY`: `WiFi.status() == WL_CONNECTED` and `WiFi.localIP()` is non-zero.
- `READY → CONNECTING`: connection lost before a transfer starts (keep retrying, FR-009).
- `READY → UPDATING`: ArduinoOTA `onStart` fires.
- `UPDATING → (reboot)`: ArduinoOTA `onEnd` success → ESP restarts; the post-reboot boot re-runs the
  `BootMode` decision (FR-008).
- `UPDATING → READY/CONNECTING`: ArduinoOTA `onError` (failed/interrupted transfer) → remain in
  maintenance mode; existing firmware stays intact (FR-010).

**Validation / rules**:
- The phase MUST be advanced/rendered without blocking, so `ArduinoOTA.handle()` runs every loop
  (R4).
- No `OtaPhase` value MUST reproduce the normal-mode "steady on" or event-driven receive-blink
  (FR-011).

## Relationship

`BootMode` selects whether `OtaPhase` exists at all. In `NORMAL`, `OtaPhase` is not instantiated and
the status LED keeps its existing normal-operation behavior (steady on = ready, brief off on ESP-NOW
frame receipt). In `OTA_MAINTENANCE`, the normal status-LED logic is not run.
