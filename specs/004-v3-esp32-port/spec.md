# Feature Specification: Bogenampel V3 Hardware Port (ESP32 + e-Paper + ESP-NOW)

**Feature Branch**: `004-v3-esp32-port`
**Created**: 2026-06-09
**Status**: Draft
**Input**: User description: "Portierung der Bogenampel auf die V3-Hardware: ESP32-S3-Sender mit 1.54-Zoll-e-Paper und ESP-NOW statt Arduino Nano mit ST7789 und NRF24; Empfänger auf XIAO ESP32C3 mit WS2812B (158 LEDs), Piezo mit Lautstärkeregelung und Lüftersteuerung. Fachliche Funktionalität (WA-Timer 120/240s, Gruppenwechsel, halbe Passe, Alarm, Konfiguration) bleibt erhalten."

## Overview

The functionally complete V2 firmware (sender = control unit, receiver = display unit) is ported
to the new V3 hardware whose schematics are finalized in `Schaltung_v3/`. The tournament
functionality stays identical to V2: WA-compliant countdown timer (120 s / 240 s with 10 s
preparation phase), group rotation A/B–C/D with 4-state cycle, half-end ("halbe Passe") support,
emergency alarm, persistent configuration (shooting time, shooter count), and battery monitoring.

What changes is the platform:

| Aspect | V2 (existing) | V3 (target) |
|--------|---------------|-------------|
| Sender MCU | Arduino Nano (ATmega328P) | ESP32-S3 module |
| Sender display | ST7789 color TFT 240×320, ~10 fps | 1.54″ e-Paper 200×200 (slow refresh) |
| Sender buttons | 3 designed, 2 actually used (Right, OK) | 2 (BTN1 = OK + power button, BTN2 = Right) |
| Sender power | Battery, hard switch | LiPo with charger, one-button soft power on/off |
| Receiver MCU | Arduino Nano (ATmega328P) | Seeed XIAO ESP32C3 |
| Radio | NRF24L01 modules on both sides | Built-in 2.4 GHz radio (ESP-NOW), no extra module |
| Receiver display | WS2812B strip, 158 LEDs | unchanged (same display unit) |
| Receiver extras | 3 status LEDs, debug jumper, brightness poti | 1 status LED, debug button, brightness poti, **new:** volume poti, fan-speed poti |
| Configuration storage | EEPROM | non-volatile flash storage (NVS) |

The V2 source trees (`Sender/`, `Empfaenger/`) remain untouched; V3 firmware lives in new
directories (`SenderV3/`, `EmpfaengerV3/`).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Running an End (Passe) on V3 Hardware (Priority: P1)

The tournament organizer powers on the V3 sender, configures shooting time (120/240 s) and
shooter count (1-2 / 3-4 per target), and runs alternating "Pfeile holen" / "Schießbetrieb"
phases exactly as in V2. The receiver shows the preparation phase, the countdown on the
three-digit 7-segment LED display, the active group (A/B or C/D), and sounds the buzzer signals.
All menu navigation works with two buttons: BTN2 advances the selection (wrap-around at the end,
as lived in V2), BTN1 confirms.

**Why this priority**: This is the core value of the device — without a working end cycle on the
new hardware there is no product. It also exercises the complete new stack (radio link, e-paper
UI, LED display, state machine) end to end.

**Independent Test**: Power on both V3 devices, configure 120 s / 1-2 shooters, start a passe,
observe 10 s preparation (red), countdown with color phases on the receiver, stop the passe.
Delivers a usable tournament timer.

**Acceptance Scenarios**:

1. **Given** the sender shows the configuration menu, **When** the user presses BTN2 (Right), **Then** the highlighted value advances and wraps around after the last option, identical to the V2 menu logic.
2. **Given** the sender shows the configuration menu, **When** the user confirms "Start" with BTN1 (OK), **Then** the sender enters "Pfeile holen" and the receiver is initialized (INIT command).
3. **Given** the sender is in "Pfeile holen", **When** the user starts the next passe, **Then** the receiver runs a 10 s preparation phase followed by the configured countdown (120 s or 240 s), with the same signal tones and traffic-light color phases as V2.
4. **Given** a passe is running, **When** the user ends it early on the sender ("Passe beenden"), **Then** the sender transmits STOP and the receiver stops and shows the stop state (red) within 0.5 s.
5. **Given** a passe is running, **When** the time expires, **Then** the receiver ends the passe autonomously (stop state, end signal tones) WITHOUT any radio command from the sender; the sender returns to "Pfeile holen" based on its own local countdown and does NOT transmit a stop command.
6. **Given** 3-4 shooters are configured and the first group's time expires, **Then** receiver AND sender each switch autonomously to the second group with a new 10 s preparation phase and countdown; the sender's start command for the second group serves only as a synchronization signal and is ignored by the receiver if its preparation phase is already running.
7. **Given** 3-4 shooters are configured, **When** ends alternate, **Then** the group display (A/B, C/D, 4-state cycle, half-end commands) behaves identically to V2.
8. **Given** any screen change on the sender, **Then** the e-paper UI updates without requiring more than one full refresh (~2 s), and the running countdown on the sender updates at 1 Hz using partial refresh without full-screen flicker.
9. **Given** the sender was powered off, **When** it is powered on again, **Then** the previously saved configuration (shooting time, shooter count) is restored.

---

### User Story 2 - One-Button Power and Battery Management (Sender) (Priority: P2)

The organizer turns the sender on with a single press of BTN1. The firmware latches its own power
supply, shows the splash screen, and displays battery level, charging state and USB presence.
Holding BTN1 (≥ 3 s) outside of "Schießbetrieb" turns the device off (firmware releases the power
latch). When a USB cable is connected, the charging state is visible on screen.

**Why this priority**: Without correct power-latch handling the V3 sender either never stays on
or can never be switched off — but it can be developed and tested independently from the
tournament flow.

**Independent Test**: Press BTN1 → device boots and stays on. Hold BTN1 3 s in the config menu →
device switches itself off completely (no residual current at the battery sensor). Connect USB →
charge indicator appears; battery percentage matches a reference measurement within tolerance.

**Acceptance Scenarios**:

1. **Given** the device is off, **When** BTN1 is pressed, **Then** the firmware latches the supply immediately at boot and the device stays on after the button is released.
2. **Given** the device is on and NOT in "Schießbetrieb", **When** BTN1 is held ≥ 3 s, **Then** the device powers itself off; the configuration is already persisted at confirmation time (FR-005), so power-off does not depend on a save-at-shutdown (a defensive re-save is permitted but not required).
3. **Given** the device is in "Schießbetrieb", **When** BTN1 is held, **Then** the device does NOT power off (the hold gesture is reserved for the alarm there).
4. **Given** the device is running, **Then** the display shows battery state of charge derived from the measured battery voltage (filtered, as in V2) and warns below the low-battery threshold.
5. **Given** USB power is connected, **Then** the display indicates USB/charging status (charging / charge complete / no charge) based on the charger status inputs.

---

### User Story 3 - Emergency Alarm (Priority: P2)

During "Schießbetrieb" the organizer holds BTN1 (OK) for 2 s to trigger the emergency alarm,
exactly as in V2: the receiver interrupts the countdown, flashes and sounds the alarm pattern
(8× blink), and the sender shows the alarm screen until dismissed.

**Why this priority**: Safety-critical feature required for real tournament use; functionally
identical to V2, so it mainly validates gesture handling and radio reliability on the new stack.

**Independent Test**: Start a passe, hold BTN1 2 s → receiver enters alarm pattern within 0.5 s;
dismiss alarm on sender → both devices return to "Pfeile holen".

**Acceptance Scenarios**:

1. **Given** "Schießbetrieb" is active, **When** BTN1 is held ≥ 2 s, **Then** the ALARM command is transmitted (with retries) and the receiver starts the alarm pattern.
2. **Given** the alarm is active, **When** the organizer dismisses it on the sender, **Then** both devices return to the "Pfeile holen" state.
3. **Given** a transient radio problem, **When** the alarm is triggered, **Then** the sender retries transmission and reports on screen whether delivery was confirmed.

---

### User Story 4 - Receiver Local Controls: Volume, Brightness, Fan (Priority: P3)

The operator adjusts the buzzer volume with the volume potentiometer (new in V3) and the LED
display brightness with the brightness potentiometer (as in V2, 25–100 %), and the enclosure fan
speed with the fan potentiometer (new in V3, decided 2026-06-10). A single status LED replaces
the three V2 status LEDs; the debug button allows a local display test without a sender.

**Why this priority**: Comfort features; the tournament works without them, but they are part of
the V3 hardware and needed for the finished device.

**Independent Test**: Turn each poti and observe volume/brightness/fan speed change live;
press debug button → local test pattern/countdown runs without radio.

**Acceptance Scenarios**:

1. **Given** the receiver is running, **When** the volume poti is turned, **Then** the buzzer loudness changes noticeably across the range (minimum still audible, maximum = full).
2. **Given** the receiver is running, **When** the brightness poti is turned, **Then** the LED strip brightness scales between 25 % and 100 % as in V2.
3. **Given** the receiver is powered, **When** the fan potentiometer is turned, **Then** the fan speed follows the potentiometer (PWM); **and** the status LED signals readiness and blinks on command reception.
4. **Given** no sender is available, **When** the debug button is pressed, **Then** the receiver runs its local test/demo behavior (as V2's debug mode).

---

### User Story 5 - Connection Quality Check at Startup (Priority: P3)

As in V2, the sender verifies the radio link to the receiver during the splash phase (ping-based
quality test) and shows the result (quality percentage / receiver reachable or not) before
entering the configuration menu.

**Why this priority**: Diagnostic comfort; the organizer learns about radio problems before the
tournament starts instead of during the first passe.

**Independent Test**: Boot sender with receiver on → quality shown ≥ 90 %. Boot sender with
receiver off → "no connection" indicated, sender still proceeds to the menu.

**Acceptance Scenarios**:

1. **Given** both devices are on and within range, **When** the sender boots, **Then** it reports a link quality equivalent to the V2 ping test (n pings, percentage of confirmed deliveries).
2. **Given** the receiver is off, **When** the sender boots, **Then** the sender indicates the missing connection and still allows entering the configuration menu.
3. **Given** the splash/link test is running, **When** BTN1 is pressed briefly, **Then** the sender skips the remaining test and proceeds directly to the configuration menu. *(New in V3 — V2 had no skip; useful when no receiver is powered yet.)*

---

### Edge Cases

- **Radio loss during a running passe**: the receiver continues its countdown autonomously (V2 behavior), performs the automatic group switch itself if applicable, and ends the passe on its own — no radio command is needed for the regular end (see FR-004/FR-004a); the sender shows failed deliveries for subsequent commands.
- **Power-off gesture vs. alarm gesture**: both live on BTN1 long-press; the firmware resolves this strictly by state — in "Schießbetrieb" 2 s = alarm and power-off is unavailable; in all other states ≥ 3 s = power-off and no alarm is sent.
- **Boot press duration**: the power-on press of BTN1 must not be misinterpreted as an immediate power-off long-press or as a menu input after boot (debounce/lockout after state change, as in V2).
- **First boot / corrupt configuration**: missing or invalid persisted configuration falls back to defaults (120 s, 1-2 shooters) without blocking the boot.
- **Empty battery**: below the cutoff the sender warns; the power latch design means a deeply discharged device simply turns off — it must save configuration at change time, not only at shutdown.
- **Charging while off**: the charger charges the battery without the MCU running (hardware feature); the firmware must show correct charge status whenever it IS running with USB connected.
- **e-Paper ghosting**: periodic full refresh (e.g. on every state change and at least once per passe cycle) prevents ghosting from frequent partial updates of the countdown.
- **Receiver brightness poti**: the schematic currently wires it to a pin without analog capability; the hardware is reworked to the designated analog pin (see Hardware Constraints) — the firmware targets the corrected wiring only.
- **Duplicate radio frames**: command repetition/retry on the new transport must not re-trigger actions on the receiver (idempotent command handling, e.g. duplicate suppression).

## Requirements *(mandatory)*

### Functional Requirements

**Parity with V2 (tournament logic)**

- **FR-001**: The system MUST provide the complete V2 tournament functionality: configuration (shooting time 120/240 s, shooter count 1-2/3-4), alternating "Pfeile holen"/"Schießbetrieb" phases, 10 s preparation phase, group display with 4-state cycle, half-end flow, and emergency alarm.
- **FR-002**: The radio protocol MUST keep the 11 V2 command semantics (STOP, START_120, START_240, INIT, ALARM, PING, GROUP_AB, GROUP_CD, GROUP_NONE, GROUP_FINISH_AB, GROUP_FINISH_CD) so that the receiver-side behavior is unchanged.
- **FR-003**: The receiver MUST drive the unchanged 158-LED display unit (16 LEDs group A/B, 16 LEDs group C/D, 3 digits × 7 segments × 6 LEDs) with the same color phases, digit rendering and signal tones as V2.
- **FR-004**: The receiver MUST decide the regular end of a passe autonomously when its countdown expires, without requiring any radio command: (a) with 1-2 shooters, or after the second group with 3-4 shooters, the passe ends (stop state, end signal tones); (b) with 3-4 shooters after the first group, the receiver automatically switches to the second group including a new 10 s preparation phase and countdown. A passe started once MUST always complete correctly even with total radio loss.
- **FR-004a**: The sender MUST NOT transmit the regular passe end over radio. STOP is sent only for a manual early end ("Passe beenden", alarm). At regular expiry the sender follows its own local countdown for its UI transitions (back to "Pfeile holen", or on to the second group). The start command the sender sends for the second group is a synchronization signal only; the receiver MUST ignore it if its autonomous preparation phase is already running. *(Regression guard: an earlier V2 version transmitted the regular end over radio — after losing the link the passe never ended on the display. This behavior must not return.)*
- **FR-005**: The sender MUST persist configuration (shooting time, shooter count) in non-volatile storage and restore it at boot; invalid/missing data falls back to V2 defaults (120 s, 1-2 shooters).

**Radio transport (replaces NRF24)**

- **FR-006**: Sender and receiver MUST communicate via the ESP32 built-in 2.4 GHz radio (ESP-NOW, peer-to-peer) without external radio modules.
- **FR-007**: The transport MUST provide per-command delivery confirmation and automatic retries so the sender can report success/failure per command (equivalent to V2's ACK/retry) within 500 ms.
- **FR-008**: The receiver MUST ignore duplicated frames caused by retries (idempotent command processing).
- **FR-009**: The sender MUST offer a startup link-quality test equivalent to V2's ping test and display the result.
- **FR-010**: Sender and receiver MUST pair with each other without user interaction (fixed addressing/broadcast scheme; no pairing menu required).

**Sender hardware adaptation**

- **FR-011**: The sender firmware MUST run on the ESP32-S3 module of the V3 board ("Universal-Fernbedienung", `Schaltung_v3/BogenampelV3/BogenampelV3.kicad_sch`) using the pin assignment from that schematic.
- **FR-012**: The sender UI MUST be rendered on the 1.54″ 200×200 monochrome e-paper panel: screen/state changes via full refresh, the 1 Hz countdown and other small live values via partial refresh; no UI element may rely on animations faster than 1 Hz.
- **FR-013**: All menus MUST be operable with exactly two buttons: BTN2 = advance/next with wrap-around (V2 "Rechts" behavior), BTN1 = confirm (V2 "OK" behavior).
- **FR-014**: Immediately after boot the firmware MUST assert the power latch to keep the device on; releasing the latch MUST switch the device off completely.
- **FR-015**: Holding BTN1 ≥ 3 s outside "Schießbetrieb" MUST power the device off; holding BTN1 ≥ 2 s inside "Schießbetrieb" MUST trigger the alarm (V2 gesture). Power-off is not available during "Schießbetrieb".
- **FR-016**: The sender MUST measure battery voltage via the on-board divider, filter it (median, as V2), display state of charge, and warn below the low-battery threshold (LiPo 3.0–4.2 V range as V2).
- **FR-017**: The sender MUST read the charger status inputs and USB detection and display charging / charge-complete / USB-present states.
- **FR-018**: The sender MUST power the display rail (switched supply) only when the display is in use.
- **FR-019**: The sender MUST emit the V2 button click feedback if a buzzer is present on the V3 board; if the V3 sender has no buzzer, button feedback is visual only. *(Schematic shows no buzzer on the sender — visual feedback only.)* "Visual feedback" means the immediately visible UI reaction to the input (selection advances, screen changes); no dedicated click indicator is required — e-paper latency makes a sub-second click animation infeasible.

**Receiver hardware adaptation**

- **FR-020**: The receiver firmware MUST run on the Seeed XIAO ESP32C3 of the V3 add-on board (`Schaltung_v3/BogenampelV3/Zusatzplatine-Empfänger.kicad_sch`) using the pin assignment from that schematic, with the brightness poti on the corrected analog pin (see Hardware Constraints).
- **FR-021**: The receiver MUST generate its signal tones on the piezo transducer via PWM with volume controlled by the volume potentiometer (continuous adjustment, full range from quiet-but-audible to maximum).
- **FR-022**: The receiver MUST scale LED display brightness between 25 % and 100 % via the brightness potentiometer (V2 behavior).
- **FR-023**: The receiver MUST control the enclosure fan speed via PWM according to the fan potentiometer (new in V3, replaces the original always-on policy). The fan tach signal is deliberately NOT connected (decided 2026-06-10: low-side PWM garbles the open-collector signal, and D8/GPIO8 is a strapping pin).
- **FR-024**: The receiver MUST signal its state on the single status LED (ready, command received) replacing the three V2 status LEDs, and MUST provide the V2 debug/local-test behavior on the debug button. V2's debug-mode brightness cap (`BRIGHTNESS_DEBUG`, USB-port protection) is deliberately NOT ported: the V3 LED strip is always powered from an external supply (5 V or 12 V, selected by jumper), never from the MCU's USB port.

**Project constraints**

- **FR-025**: The V2 firmware directories (`Sender/`, `Empfaenger/`) MUST remain unchanged; V3 firmware lives in new directories (`SenderV3/`, `EmpfaengerV3/`).
- **FR-026**: Both V3 firmwares MUST build with PlatformIO (`pio run` per firmware folder) as the sole supported build path. Arduino IDE compatibility is NOT required (decided 2026-06-11: full migration to PlatformIO + ESP32; constitution v2.2.0). The flat sketch structure with `.ino` files is retained as a project convention, not as an IDE requirement.
- **FR-027**: Code structure and style MUST follow the existing conventions (central `Config.h` with namespaces, state machine + manager classes, Doxygen-style comments, German comments/UI texts).

### Key Entities

- **TournamentConfig**: persisted configuration — shooting time (120/240 s), shooter count (2/4), validity check; stored in non-volatile flash, defaults on first boot.
- **RadioCommand / RadioPacket**: the 11-command protocol with integrity check (unchanged semantics from `Commands.h`), now carried over ESP-NOW with sequence/duplicate handling.
- **Sender state machine**: Splash (incl. link test) → Config → Pfeile holen ↔ Schießbetrieb, Alarm overlay; extended by power-off handling.
- **Receiver display model**: 158-LED layout (groups + 3-digit 7-segment), color phases, brightness; unchanged from V2.
- **Power state (sender)**: latch control, battery voltage/percentage, charger status, USB presence.

## Hardware Constraints *(fixed by the V3 schematics)*

These pin assignments are the binding contract between schematic and firmware
(extracted from the KiCad netlists; sender pins are ESP32-S3 GPIO numbers, receiver pins are
XIAO D-numbers):

**Sender (ESP32-S3-WROOM-1U-N16R8)**: e-paper BUSY=38, RES=48, DC=47, CS=21, CLK=14, MOSI=13;
power LATCH=16 (HIGH = stay on), display rail LOAD=7; battery ADC=10 (divider 150k/100k);
USB detect=8; charger C_ST1=11, C_ST2=12, C_PG=17, C_PRG=18; BTN1 (power/OK)=15, BTN2 (next)=9.
GPIO 35–37 are reserved by the module's PSRAM and stay unused.

**Receiver (XIAO ESP32C3)**: WS2812B data=D10; button=D7; status LED=D9; piezo (via transistor,
12 V transducer)=**D3** (reworked from D2 on 2026-06-10); **fan-speed poti=D2 (analog, ADC1)**
(new); fan PWM=D6 (gate pull-up → fan defaults to full speed until firmware takes over);
fan tach not connected (D8 free); volume poti=D0 (analog, foot switched via D4 `POTI_GND`);
**brightness poti=D1 (analog)** — the schematic currently shows D5, but D5 has no analog input
capability on this MCU; the board is hand-wired (Lochraster) and is reworked to D1. D1 is the
specified target state; the schematic will be corrected accordingly.

**ESP32-C3 strapping constraint**: D0/GPIO2, D8/GPIO8 and D9/GPIO9 are strapping pins; the
circuit MUST guarantee a HIGH level on GPIO2 and GPIO9 at reset, or the receiver does not boot.
Implemented in the schematic (verified via netlist 2026-06-10): volume poti foot switched via
D4/GPIO6 (`POTI_GND`), status LED wired active LOW, D8 left unconnected — see
`contracts/hardware-pins.md`, findings 2–4.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A complete passe cycle (configure → start → countdown → stop) works on V3 hardware with behavior indistinguishable from V2 for the archers (same phases, colors, tones, group logic).
- **SC-002**: Receiver reacts to any sender command within 0.5 s under normal conditions.
- **SC-003**: The countdown is accurate to ±1 s over a 240 s passe on both sender display and receiver display.
- **SC-004**: Radio range is at least equal to V2 in the field (full shooting range distance, ≥ 30 m line of sight, link quality ≥ 90 % in the startup test).
- **SC-005**: Sender is ready for input (config menu reachable) within 10 s after pressing the power button.
- **SC-006**: One press turns the sender on; holding BTN1 for 3 s suffices to turn it off from any non-shooting state (gesture threshold ≥ 3 s per FR-015 — no longer hold is ever required); off-state battery drain is limited to hardware leakage (firmware fully releases the latch).
- **SC-007**: The sender countdown updates every second without a disturbing full-screen flash; full refreshes occur only on screen changes.
- **SC-008**: Configuration survives power cycles in 100 % of cases, including first boot with empty storage (defaults applied).
- **SC-009**: All 11 protocol commands are demonstrably exercised on V3 hardware (manual test checklist) with confirmed delivery feedback on the sender.
- **SC-010**: Buzzer volume, LED brightness and fan speed follow their potentiometers continuously over the full mechanical range.
- **SC-011**: Both firmwares compile reproducibly with PlatformIO (CI-able). *(Arduino IDE build dropped 2026-06-11 — full PlatformIO migration.)*
- **SC-012**: A passe started normally completes correctly on the receiver even if the sender is switched off or moved out of range immediately after the start — including the automatic group switch with 3-4 shooters and the regular end signal tones.

## Assumptions

- **Button roles**: BTN1 (the power button) doubles as "OK", BTN2 as "Rechts/weiter" — matching the two buttons actually used in V2; menu wrap-around replaces the left button.
- **Power-off gesture**: BTN1 ≥ 3 s outside "Schießbetrieb"; inside "Schießbetrieb" the 2 s hold keeps its V2 alarm meaning and power-off is deliberately unavailable.
- **Sender buzzer**: the V3 sender schematic contains no buzzer; V2's click feedback becomes visual-only on the sender. Receiver tones are unchanged.
- **Fan policy**: fan speed is set manually via the fan potentiometer (PWM); automatic control (temperature/timer based) is out of scope for the port.
- **Display unit**: the physical 158-LED display (mechanics, LED order, segment mapping) is reused unchanged from V2.
- **ESP-NOW details** (channel, addressing, acknowledgment mechanism) are implementation decisions for the planning phase; the spec only fixes the observable behavior (confirmed delivery, retries, duplicate suppression, no pairing interaction).
- **Receiver power**: receiver remains externally powered (USB 5 V or 12 V input per schematic); no battery management on the receiver.
- **V2 test programs** (`TestSender/`, `TestEmpfaenger/`) are not ported; new hardware bring-up tests may be added under the V3 directories if needed.
