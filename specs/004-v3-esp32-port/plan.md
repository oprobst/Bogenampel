# Implementation Plan: Bogenampel V3 Hardware Port (ESP32 + e-Paper + ESP-NOW)

**Branch**: `004-v3-esp32-port` | **Date**: 2026-06-09 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/004-v3-esp32-port/spec.md`

## Summary

Port the functionally complete V2 firmware to the V3 hardware: the sender moves from Arduino
Nano + ST7789 TFT + NRF24 to an ESP32-S3 with a 1.54″ e-paper panel, LiPo charging and one-button
soft power; the receiver moves from Arduino Nano + NRF24 to a XIAO ESP32C3 driving the unchanged
158-LED display, with new volume and fan control. The radio transport is replaced by ESP-NOW
(built-in 2.4 GHz) while keeping the 11-command protocol semantics. The tournament logic
(WA timer 120/240 s, group cycle, half-end, alarm, autonomous passe end on the receiver —
FR-004/FR-004a) is ported 1:1 from the V2 code base into two new firmware trees `SenderV3/` and
`EmpfaengerV3/`, built with PlatformIO (sole build path since 2026-06-11 — full PlatformIO
migration, constitution v2.2.0).

## Technical Context

**Language/Version**: C++ (Arduino core for ESP32 / arduino-esp32 3.x, C++17; V2 logic is C++11 and ports without changes)
**Primary Dependencies**:
- Sender: GxEPD2 (+ Adafruit GFX) for the SSD1681 e-paper, `esp_now` + `WiFi` (core), `Preferences` (core, NVS)
- Receiver: FastLED (WS2812B via RMT), `esp_now` + `WiFi` (core), LEDC (core, piezo PWM)
**Storage**: ESP32 NVS via `Preferences` (replaces AVR EEPROM), namespace `bogenampel`
**Testing**: PlatformIO build verification (`pio run`) for both firmwares; hardware-in-the-loop checklist (quickstart.md); pure tournament logic kept in hardware-free classes to allow optional native tests later
**Target Platform**: Sender = ESP32-S3-WROOM-1U-N16R8 (16 MB flash, 8 MB octal PSRAM → GPIO 35–37 reserved); Receiver = Seeed XIAO ESP32C3 (4 MB flash)
**Project Type**: Embedded, two independent firmware projects sharing one protocol header
**Performance Goals**: command delivery confirmed < 500 ms incl. retries; countdown accuracy ±1 s / 240 s; e-paper partial refresh < 400 ms at 1 Hz; sender boot-to-menu < 10 s
**Constraints**: e-paper refresh budget (full ~2 s only on screen changes); power latch must be asserted in the first milliseconds of `setup()`; ESP32 ADC2 unusable while the radio is active → all analog inputs must be on ADC1 (drives the D5→D1 poti rework and forbids analog reading of BTN1 on GPIO15); BTN1 input level is marginal by schematic (see research.md R-13, hardware fix recommended)
**Scale/Scope**: ~3.7 kLOC V2 firmware to port + new platform layer (~1.5 kLOC new/changed); 2 boards; 11 radio commands; 5 sender screens

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment | Status |
|-----------|------------|--------|
| I. Sicherheit Zuerst | Fail-safe is strengthened: receiver completes/ends a passe fully autonomously (FR-004); regular end never depends on radio (FR-004a, regression guard from V2 bug). All frames validated (magic + checksum + dedup) before execution. Red stop state unchanged from V2. | ✅ PASS |
| II. Einfachheit & Zuverlässigkeit | Two-button concept preserves the lived V2 operation; no pairing interaction (FR-010); plug-and-play after power-on; NVS defaults on first boot. One-button start/stop kept. | ✅ PASS |
| III. Embedded-Hardware-Standards | PlatformIO is the sole build path (FR-026, constitution v2.2.0 — Arduino IDE requirement dropped 2026-06-11). Libraries (GxEPD2, FastLED, Adafruit GFX) are stable OSS, pinned via `lib_deps`. Pins centralized as `constexpr` in `Config.h`. Constitution v2.0.0 (amended 2026-06-10) names `Schaltung_v3/BogenampelV3/` as the authoritative schematic for V3 firmware; netlists extracted and frozen in `contracts/hardware-pins.md`. | ✅ PASS |
| IV. Testbarkeit & Validierung | Each timer phase independently testable (receiver debug button = local test without radio); link quality test at startup (FR-009); HIL checklist in quickstart.md covers all 11 commands (SC-009) and radio-loss autonomy (SC-012). | ✅ PASS |
| V. Wartbarkeit & Dokumentation | Pin contract written down in `contracts/hardware-pins.md` with connector references; the poti rework (D5→D1) is specified to be corrected in the KiCad schematic first, code follows (constitution rule respected); code style follows V2 conventions (FR-027). | ✅ PASS |

**Post-design re-check (after Phase 1)**: no new violations introduced. The formerly documented
deviation (outdated V2 hardware section in the constitution) was resolved by constitution
amendments v2.0.0/v2.1.0 on 2026-06-10 (V3 hardware standards, `Schaltung_v3/` authoritative,
operating parameters updated — init time < 10 s; the V2 USB current-protection rule was
removed because the V3 LED strip is always externally powered, 5 V/12 V via jumper).

## Project Structure

### Documentation (this feature)

```text
specs/004-v3-esp32-port/
├── plan.md              # This file
├── research.md          # Phase 0: technology decisions R-1…R-14
├── data-model.md        # Phase 1: config, packet, state machines, power model
├── quickstart.md        # Phase 1: build/flash/HIL test guide
├── contracts/
│   ├── espnow-protocol.md   # Radio frame format, discovery, retry/dedup, timing
│   └── hardware-pins.md     # Binding pin contract (from KiCad netlists)
└── tasks.md             # Phase 2 (/speckit.tasks — NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
SenderV3/                  # NEW — Bedieneinheit (ESP32-S3), flat (Projekt-Konvention)
├── SenderV3.cpp           # Setup/Loop, Power-Latch-Bootstrap, 1-Hz-esp_timer
├── platformio.ini         # env:sender (board esp32s3, 16MB/8MB PSRAM)
├── Config.h               # Pins (aus contracts/hardware-pins.md), Timing, Display, Battery
├── Commands.h             # Protokoll V3 (identische Kopie wie EmpfaengerV3/Commands.h)
├── RadioManager.{h,cpp}   # NEU: ESP-NOW Transport (Discovery, Retry, Dedup, Qualitätstest)
├── PowerManager.{h,cpp}   # NEU: Latch, Power-Off, Akku (Median), Lader-Status, USB
├── EpaperDisplay.{h,cpp}  # NEU: GxEPD2-Wrapper (Voll-/Partial-Refresh-Strategie, LOAD-Rail)
├── ButtonManager.{h,cpp}  # PORT: 2 Taster (BTN1 aktiv-HIGH, BTN2 aktiv-LOW), Gesten
├── StateMachine.{h,cpp}   # PORT: Logik 1:1, Render-Aufrufe auf EpaperDisplay umgestellt
├── ConfigMenu.{h,cpp}     # PORT: 2-Tasten-Navigation (Wrap-around wie V2)
├── SchiessBetriebMenu.{h,cpp} # PORT: Countdown via Partial-Refresh
├── PfeileHolenMenu.{h,cpp}    # PORT
├── SplashScreen.{h,cpp}   # PORT: inkl. ESP-NOW-Qualitätstest
└── AlarmScreen.{h,cpp}    # PORT

EmpfaengerV3/              # NEW — Anzeigeeinheit (XIAO ESP32C3), flat (Projekt-Konvention)
├── EmpfaengerV3.cpp       # PORT von Empfaenger.ino: Timer-Logik 1:1, ESP-NOW statt RF24
├── platformio.ini         # env:empfaenger (board seeed_xiao_esp32c3)
├── Config.h               # Pins (aus contracts/hardware-pins.md), LED-Layout unverändert
├── Commands.h             # Protokoll V3 (identische Kopie wie SenderV3/Commands.h)
├── RadioManager.{h,cpp}   # NEU: ESP-NOW Empfang (Discovery-Antwort, Dedup, PING-Echo)
├── DisplayManager.{h,cpp} # PORT: unverändert (FastLED, 158 LEDs)
├── BuzzerManager.{h,cpp}  # PORT+NEU: LEDC-PWM mit Lautstärke vom Poti
└── FanManager.{h,cpp}     # NEU: Lüfter-PWM (Drehzahl-Poti D2) + Tacho-Diagnose

Sender/                    # V2 — UNVERÄNDERT (FR-025)
Empfaenger/                # V2 — UNVERÄNDERT (FR-025)
Schaltung_v3/              # Hardware (KiCad) — Poti-Korrektur D5→D1 im Schaltplan
```

**Structure Decision**: Two flat firmware directories (V2 convention, kept for consistency),
each self-contained with its own `platformio.ini` (`src_dir = .`). `Commands.h` is intentionally
duplicated in both trees — same approach as V2 — so each firmware folder stays fully
self-contained (no cross-folder include paths); the protocol contract
(`contracts/espnow-protocol.md`) is the single source of truth and both copies must stay
identical (verified via `fc /b`, checked in the HIL checklist). Originally this duplication was
forced by the Arduino IDE sketch model; since the full PlatformIO migration (2026-06-11) it is
retained as a deliberate simplicity choice — a shared header via `build_flags`-include would be
possible but adds coupling for a 6-byte protocol that changes rarely.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| ~~Constitution hardware section (Arduino Nano, NRF24, `/Schaltung/Schaltplan.pdf`) does not match V3~~ **RESOLVED** | The constitution was ratified for V2 hardware; V3 is a sanctioned hardware generation change requested by the project owner | Resolved 2026-06-10: constitution amendment v2.0.0 ratified (V3 hardware standards, `Schaltung_v3/BogenampelV3/` as authoritative source, operating parameters updated) — see T037 (completed) |
| `Commands.h` duplicated in both firmware trees | Self-contained firmware folders (V2 convention; historically an Arduino-IDE-Zwang, seit der PlatformIO-Migration 2026-06-11 bewusste Einfachheits-Entscheidung) | A shared header via cross-folder include path adds build coupling for a rarely changing 6-byte protocol; the duplicate-and-keep-identical convention is verified automatically (`fc /b`, T034) |
