# Contract: Boot-Mode Decision & Per-Mode Initialization

Defines the observable obligations of the receiver's startup. "MUST"/"MUST NOT" are testable at the
hardware boundary (radio scans, access-point presence, LED, OTA reachability).

## C1 — Boot decision (FR-001, FR-007, SC-001)

- The receiver MUST evaluate the debug button (D7 / J5, `INPUT_PULLUP`, active-low) exactly once,
  early in `setup()`, after the strapping-pin safety steps (`POTI_GND` LOW, `fan.begin()`) and
  **before** any radio initialization or the boot rainbow effect.
- The read MUST be debounced (stable `LOW` across multiple samples over ~20–30 ms) so a single noisy
  sample cannot flip the decision.
- Result: `bootOtaMode = true` if held, else `false`. The value MUST NOT change for the rest of the
  power cycle, regardless of later button activity.

## C2 — NORMAL path (button not held) (FR-005, FR-006)

When `bootOtaMode == false`, the receiver MUST:

- Initialize ESP-NOW via `radio.begin()` on the fixed channel (channel 1), exactly as before this
  feature.
- Run the existing boot sequence (rainbow effect, timer/display, buzzer, potis, fan, status-LED
  normal behavior, 1 Hz tick) unchanged.

When `bootOtaMode == false`, the receiver MUST NOT:

- Call `WiFi.begin()` or otherwise attempt to join any WiFi network.
- Start any SoftAP / access point (none may be observable in a WiFi scan).
- Start ArduinoOTA.

Observable acceptance: a normal boot shows **no** `Bogenampel-Empfaenger` access point and makes no
association with the home router; the sender↔receiver ESP-NOW link establishes with the prior
reliability (SC-003, SC-006).

## C3 — OTA path (button held) (FR-002, FR-003, FR-009, FR-012)

When `bootOtaMode == true`, the receiver MUST:

- **Not** call `radio.begin()` and **not** start ESP-NOW, the timer, the rainbow effect, the buzzer,
  or the poti loops.
- Set the radio to `WIFI_STA` (station only — no access point) and join the configured home network
  using the compile-time credentials, with auto-reconnect and indefinite retry (no SoftAP fallback).
- Start ArduinoOTA with the configured hostname. No network OTA password is set — access is gated by
  the physical button-at-boot requirement instead (FR-012).
- Drive the status LED via the OTA LED contract (see `ota-led-signal-contract.md`).
- Remain in this mode until reset/power-cycle even if WiFi never connects (keep retrying + keep
  blinking) — it MUST NOT fall through to NORMAL/ESP-NOW.

Observable acceptance: with the button held at power-on, within 20 s under normal network conditions
the device answers the ArduinoOTA UDP handshake at its assigned IP (SC-002); a full flash succeeds
and the device reboots (US1).

## C4 — Mutual exclusion (FR-006)

At no point in a single power cycle may both ESP-NOW (channel 1) and a WiFi-STA association
(router channel) be active. The mode chosen at C1 selects exactly one radio role for the whole power
cycle.

## C5 — Post-update (FR-008, FR-010)

- A successful OTA ends with an automatic restart; the next boot re-runs C1 (so, button released →
  NORMAL).
- An interrupted/failed transfer MUST leave the previously running firmware intact and bootable; the
  device stays in OTA maintenance mode (does not brick, does not silently switch modes).
