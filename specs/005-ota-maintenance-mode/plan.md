# Implementation Plan: Receiver OTA Maintenance Mode (button-at-boot)

**Branch**: `005-ota-maintenance-mode` | **Date**: 2026-06-13 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/005-ota-maintenance-mode/spec.md`

## Summary

Replace the receiver's always-on "join home WiFi + ESP-NOW at once" boot behavior with an explicit,
button-gated boot decision. A normal power-on runs ESP-NOW only on the fixed channel (no WiFi, no
access point). Holding the receiver's debug button (D7) while powering on selects **OTA maintenance
mode**: the receiver does not start the timer or ESP-NOW, switches the radio to plain WiFi station
mode, joins the home network, and waits for an authenticated ArduinoOTA flash, signalling its
sub-state on the status LED (D9) with three distinct blink patterns. This removes the single-radio
channel conflict from everyday operation and makes OTA deterministic.

Technical approach: read the button once at the very start of `setup()` (after the mandatory
strapping-pin fixes), then fork into one of two initialization paths. The existing
`OTAManager::begin()` is reworked to a **station-only** entry (its SoftAP fallback is removed) and is
called *only* on the OTA path; `radio.begin()` (ESP-NOW) is called *only* on the normal path. A small
non-blocking LED signaller drives the FR-004a/b/c patterns from the OTA connection/update phase.

## Technical Context

**Language/Version**: C++17, Arduino core for ESP32 (arduino-esp32 3.3.7)
**Primary Dependencies**: WiFi, ArduinoOTA, ESPmDNS (Arduino-ESP32 built-ins), FastLED (already used); ESP-NOW via `esp_now`/`esp_wifi` (normal path only)
**Storage**: N/A — no new persistence. WiFi credentials remain compile-time (`wifi_credentials.h`, gitignored); no NVS changes.
**Testing**: PlatformIO build (`pio run -e empfaenger`); Hardware-in-the-Loop acceptance per `quickstart.md` (no host unit-test harness exists for this firmware)
**Target Platform**: Seeed XIAO ESP32C3 (single 2.4 GHz radio, 4 MB flash) — `EmpfaengerV3/`
**Project Type**: single (embedded firmware, flat `EmpfaengerV3/` directory)
**Performance Goals**: button decision deterministic at boot (FR-001/SC-001); reachable for OTA < 20 s after power-on (SC-002); LED pattern timings per FR-004a–c (±~50 ms tolerance)
**Constraints**: single radio → ESP-NOW (ch1) and WiFi-STA (router channel) are mutually exclusive and MUST NOT run together; button D7/GPIO20 is `INPUT_PULLUP`, active-low; status LED D9/GPIO9 active-low (`STATUS_LED_ON = LOW`); button must be read before the ~5 s boot rainbow effect
**Scale/Scope**: receiver firmware only; touches `EmpfaengerV3.cpp` (boot fork), `OTAManager.{h,cpp}` (STA-only + phase callback), `Config.h` (LED-timing constants); sender untouched

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **I. Sicherheit Zuerst** — PASS. OTA maintenance mode is entered only by a deliberate
  button-at-boot gesture and never during a running pass; the autonomous pass-end logic and all
  ESP-NOW frame validation are untouched on the normal path. The boot decision is a single,
  deterministic branch (FR-001/FR-007). Red-phase distinctness is unaffected (the LED strip is not
  used in OTA mode). OTA itself requires authentication (FR-012), consistent with "no unvalidated
  execution".
- **II. Einfachheit & Zuverlässigkeit** — PASS. Normal one-button operation is unchanged; OTA is a
  simple power-on-with-button gesture with no menu/pairing. Removing the flaky always-STA + SoftAP
  fallback makes the normal boot *simpler and more reliable* (no WiFi channel disruption).
- **III. Embedded-Hardware-Standards** — PASS. Uses only already-assigned pins (D7 button = J5, D9
  status LED) from `specs/004-v3-esp32-port/contracts/hardware-pins.md`; no schematic change, so no
  KiCad edit required. Build stays on `pio run`. Pins remain `constexpr` in `Config.h`; new LED
  timing values added as named constants. Dependencies (WiFi/ArduinoOTA/FastLED) are stable and
  open source.
- **IV. Testbarkeit & Validierung** — PASS. The two boot modes and the three LED phases are each
  independently observable; `quickstart.md` defines HIL steps mapped to FR/SC.
- **V. Wartbarkeit & Dokumentation** — PASS. State machine and timings documented in
  `data-model.md` and `contracts/`; code comments reference connectors (J5) and the FR ids. No pin
  reassignment, so no documentation drift against the schematic.

**Result**: No violations. Complexity Tracking not required.

## Project Structure

### Documentation (this feature)

```text
specs/005-ota-maintenance-mode/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output (state model)
├── quickstart.md        # Phase 1 output (operator + HIL acceptance)
├── contracts/
│   ├── boot-mode-contract.md      # boot decision + per-mode init obligations
│   └── ota-led-signal-contract.md # LED state machine + exact timings
└── tasks.md             # Phase 2 output (/speckit.tasks — not created here)
```

### Source Code (repository root)

```text
EmpfaengerV3/
├── EmpfaengerV3.cpp     # CHANGE: read button early in setup(); fork normal vs OTA boot;
│                        #         loop() runs either ESP-NOW path or OTA path (not both)
├── Config.h             # CHANGE: add OTA-mode LED timing constants; (SoftAP SSID now unused)
├── OTAManager.h         # CHANGE: STA-only begin(); expose connection/update phase for the LED
├── OTAManager.cpp       # CHANGE: remove SoftAP fallback; keep retrying STA; report phase;
│                        #         drive ArduinoOTA onStart/onProgress/onEnd into the phase
├── RadioManager.*       # UNCHANGED logic; simply not started in OTA mode
├── DisplayManager.*     # UNCHANGED (LED strip idle in OTA mode)
├── BuzzerManager.*      # UNCHANGED (silent in OTA mode)
└── FanManager.*         # UNCHANGED (fan still safe-initialized early, see research)
```

**Structure Decision**: Single embedded firmware in the existing flat `EmpfaengerV3/` directory
(project convention: no subdirectories, PlatformIO-only build). The feature is a boot-time control-
flow fork plus a small LED state machine; it adds no new translation units beyond optional inlining
into `OTAManager`/`EmpfaengerV3.cpp`. The sender (`SenderV3/`) is explicitly out of scope.

## Complexity Tracking

> No constitution violations — section intentionally empty.
