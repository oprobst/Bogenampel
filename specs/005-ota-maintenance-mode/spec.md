# Feature Specification: Receiver OTA Maintenance Mode (button-at-boot)

**Feature Branch**: `005-ota-maintenance-mode`
**Created**: 2026-06-13
**Status**: Draft
**Input**: User description: "Wenn der Boot erfolgt (startup) UND der Knopf gleichzeitig gedrückt wird, dann startet die Ampel nicht normal und startet nicht den ESP-NOW Funkbetrieb. Stattdessen verbindet sich der Empfänger mit dem Wifi und wartet auf ein OTA Flash. In diesem Zustand signalisiert die Debug LED durch ein regelmäßiges Blitzen."

## Overview

The V3 receiver (display unit, XIAO ESP32C3) currently tries to do two mutually exclusive things
on every boot: run ESP-NOW with the sender on a fixed radio channel **and** join the home WiFi for
over-the-air (OTA) updates. Because the chip has a single radio, these cannot coexist — joining the
home router pulls the radio off the ESP-NOW channel, while staying on the ESP-NOW channel makes the
WiFi join unreliable. In practice the receiver intermittently falls back to a device-local access
point that is unreachable from a wired workstation, so OTA fails.

This feature replaces that always-on dual behavior with an **explicit, on-demand OTA maintenance
mode**. Normal power-on runs ESP-NOW only, exactly as the tournament use requires. To update the
firmware, the operator holds the receiver's button while powering on; the receiver then skips normal
operation, joins the home WiFi, and waits to be flashed, signalling this state with a regularly
blinking indicator LED. This removes the radio-channel conflict from normal operation entirely and
makes OTA deterministic and repeatable.

Scope is the **receiver** only (the unit the user calls "die Ampel"). The sender is not changed by
this feature.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Flash the receiver on demand over the network (Priority: P1)

The maintainer wants to install new firmware on an already-deployed receiver without opening the
enclosure or attaching a USB cable. They power the receiver on while holding its button. The
receiver does not start a tournament/timer session and does not start the sender radio link;
instead it joins the home WiFi and becomes reachable for an OTA update at its known network address.
The maintainer pushes the new firmware over the network. When the transfer finishes, the receiver
restarts and resumes normal operation.

**Why this priority**: This is the entire point of the feature — a reliable, repeatable way to
update a mounted receiver. Without it there is no OTA at all.

**Independent Test**: Power on with the button held, confirm the device becomes reachable for OTA
at its expected address within a short, bounded time, perform a full firmware update, and confirm
the device restarts into normal operation.

**Acceptance Scenarios**:

1. **Given** the receiver is powered off and the button is held, **When** power is applied, **Then**
   the receiver enters OTA maintenance mode, joins the home WiFi, and becomes reachable for an OTA
   update — and does not start the timer display or the sender radio link.
2. **Given** the receiver is in OTA maintenance mode and reachable, **When** a valid firmware image
   is sent over the network, **Then** the update completes and the receiver restarts.
3. **Given** a completed update and the button no longer held, **When** the receiver restarts,
   **Then** it boots into normal operation.

---

### User Story 2 - Normal power-on is unaffected and the radio link is reliable (Priority: P2)

The tournament organizer powers the receiver on normally (without holding the button). The receiver
behaves exactly as before — it shows the start indication and links to the sender over ESP-NOW —
and it never attempts to join a WiFi network or open its own access point. The radio link is no
longer disrupted by WiFi activity.

**Why this priority**: The product's core function is the sender↔display radio link. The fix is only
acceptable if it removes the OTA/WiFi interference from everyday use; otherwise it would regress the
main use case.

**Independent Test**: Power on repeatedly without the button and confirm the sender↔receiver link
establishes with normal reliability and timing, with no WiFi access point appearing and no WiFi
join attempts.

**Acceptance Scenarios**:

1. **Given** the receiver is powered off and the button is not held, **When** power is applied,
   **Then** the receiver boots straight into normal ESP-NOW operation and makes no WiFi connection
   attempt and opens no access point.
2. **Given** the receiver is running normally, **When** the sender sends commands, **Then** they are
   received and acted on with the same reliability as before this feature.

---

### User Story 3 - The maintenance state is visually unmistakable (Priority: P3)

A person standing at the receiver can tell at a glance not only that it is in OTA maintenance mode
but also which stage it is in. The indicator LED uses three distinct patterns — a fast short flash
while still trying to join WiFi, a slow even blink once connected and waiting to be flashed, and a
mostly-lit-with-brief-blips pattern while a firmware transfer is running — none of which can be
confused with the normal "ready" indication or the brief receive-blink.

**Why this priority**: Field maintenance is error-prone; clear feedback prevents the maintainer from
waiting on a device that is actually in normal mode (or vice versa). It is valuable but secondary to
the mode actually working.

**Independent Test**: Enter OTA maintenance mode and confirm the indicator blinks in the defined
pattern; confirm normal operation never produces that same pattern.

**Acceptance Scenarios**:

1. **Given** the receiver has just entered OTA maintenance mode but has not yet joined WiFi, **When**
   it is retrying the connection, **Then** the indicator LED shows the fast short-flash pattern
   (FR-004a).
2. **Given** the receiver has joined WiFi and been assigned an IP, **When** it is waiting to be
   flashed, **Then** the indicator LED shows the slow even blink (FR-004b).
3. **Given** a firmware transfer is in progress, **When** data is being written, **Then** the
   indicator LED is mostly lit with brief periodic dark blips (FR-004c).
4. **Given** the receiver is in normal operation, **When** it is idle or receiving commands, **Then**
   the indicator never shows any of the OTA-mode patterns.

---

### Edge Cases

- **WiFi not available / join fails in OTA mode**: the receiver stays in OTA maintenance mode and
  keeps trying to join; it does **not** silently fall back into normal/ESP-NOW operation. The
  blinking signal continues so the maintainer knows the device is still in maintenance mode.
- **Button released after boot**: the boot mode is decided once at startup. Releasing the button
  after entering OTA maintenance mode does not change the mode; only a power-cycle/reset does.
- **Button still held after a post-flash restart**: the device would re-enter OTA maintenance mode
  (consistent, deterministic behavior); normal operation requires powering on without the button.
- **Interrupted or failed transfer / power loss during flashing**: the previously installed,
  working firmware remains intact and runnable; a failed update never bricks the device.
- **Unexpected network address**: if the network assigns a different address than expected, the
  device is still in OTA mode and reachable at whatever address it obtained (a reserved/fixed
  address on the router keeps it predictable).
- **No button activity at all (sender use)**: identical to normal power-on — no maintenance mode.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The receiver MUST determine its boot mode once during startup by checking whether its
  button is held at that moment.
- **FR-002**: When the button is held at startup, the receiver MUST enter OTA maintenance mode and
  MUST NOT start normal timer/display operation and MUST NOT start the ESP-NOW radio link.
- **FR-003**: In OTA maintenance mode, the receiver MUST join the configured home WiFi network and
  MUST become reachable for an over-the-air firmware update.
- **FR-004**: In OTA maintenance mode, the receiver MUST continuously signal its sub-state with the
  status LED (D9, active-low) using three distinct, recognizable patterns (timings are target values; small
  tolerance is acceptable):
  - **FR-004a — Entered, WiFi not yet connected**: a short, fast flash — roughly 100 ms on followed
    by ~500 ms off (a brief blip about every 600 ms).
  - **FR-004b — WiFi connected and an IP has been assigned, waiting to be flashed**: a slow, even
    blink — roughly equal on/off intervals (~500 ms on, ~500 ms off).
  - **FR-004c — Firmware transfer in progress**: predominantly lit, briefly going dark (~100 ms)
    about every 500 ms.
- **FR-005**: When the button is NOT held at startup, the receiver MUST boot directly into normal
  ESP-NOW operation and MUST NOT attempt any WiFi connection and MUST NOT open any access point.
- **FR-006**: Normal operation MUST keep the ESP-NOW radio on its fixed channel and free of any
  WiFi-induced channel changes (the OTA/WiFi path runs only in maintenance mode).
- **FR-007**: Once entered, the receiver MUST remain in OTA maintenance mode until it is
  power-cycled or reset, regardless of whether the button is released.
- **FR-008**: After a successful firmware update, the receiver MUST restart and (with the button not
  held) resume normal operation.
- **FR-009**: If the WiFi connection cannot be established in OTA maintenance mode, the receiver MUST
  keep retrying and remain in maintenance mode, continuing the blinking signal; it MUST NOT fall
  back to normal/ESP-NOW operation or to a device-local access point.
- **FR-010**: The update mechanism MUST protect the existing firmware so that an interrupted or
  failed transfer leaves the device able to boot the previously working firmware.
- **FR-011**: The OTA-mode blink pattern MUST be visually distinct from the normal "ready"
  indication and from the brief receive-blink used in normal operation.
- **FR-012**: Access to OTA maintenance mode MUST be gated by **physical access**: it is reachable
  only by holding the button during boot (FR-002). This button-at-boot gate is the security control;
  the firmware therefore does NOT require a network OTA password. (Rationale: the ArduinoOTA password
  hash is incompatible with the PlatformIO flash tool, and a person able to power-cycle the device
  while holding the button already has physical access. See research R7.)

### Key Entities

- **Boot mode**: the single decision made at startup — either *normal operation* (ESP-NOW, no WiFi)
  or *OTA maintenance mode* (WiFi joined, ESP-NOW off, awaiting flash). Determined solely by the
  button state at power-on and fixed for the remainder of that power cycle.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Holding the button during power-on results in OTA maintenance mode on 100% of
  attempts (deterministic, no intermittent fall-through to normal mode).
- **SC-002**: In OTA maintenance mode under normal network conditions, the receiver becomes
  reachable for an OTA update within 20 seconds of power-on.
- **SC-003**: A normal power-on (button not held) completes the boot sequence (rainbow effect ~5 s)
  and is ready to receive ESP-NOW commands within 15 seconds of power-on; in 100% of normal
  power-ons it opens no access point and makes no WiFi join attempt.
- **SC-004**: An operator can visually confirm OTA maintenance mode within 2 seconds of power-on
  from the blink pattern, and can distinguish the three sub-states (joining WiFi, ready to flash,
  transfer in progress) from one another by sight; no normal-operation state reproduces any of these
  patterns.
- **SC-005**: A complete firmware update can be performed end-to-end over the network and the device
  returns to normal operation after restart, in under 3 minutes total for a typical image.
- **SC-006**: Across 20 consecutive normal power-ons, the sender↔receiver link establishes
  successfully on all 20 (100%), and no boot attempts a WiFi association or opens an access point.

## Assumptions

- Terminology: the "status LED" / "indicator LED" in this spec is the receiver's single status LED
  on **D9** (active-low). The original request called it the "Debug LED"; it is **not** the **D7
  debug button** that triggers the boot gesture and the local test run.
- The receiver has exactly one suitable button available at startup (the existing maintenance/debug
  button); using it for the boot-time mode selection does not conflict with its normal-operation
  role, because the normal role is only evaluated after a normal boot.
- The configured home WiFi credentials are present in the build; without them OTA maintenance mode
  cannot join a network (the device would remain blinking and retrying — see FR-009). Providing and
  storing those credentials is out of scope here (handled by existing configuration).
- A predictable network address for the receiver (e.g. a router-side reservation) is recommended so
  the maintainer always flashes the same address, but the feature does not require it to function.
- The three indicator patterns and their approximate timings are specified in FR-004a–c. The given
  millisecond values are targets with small tolerance, not exact hard real-time requirements; what
  matters is that the three patterns remain clearly distinguishable from each other and from the
  normal-operation indications.
- OTA maintenance mode does not need to run the timer, group display, buzzer, or fan; only the
  indicator LED and the network/OTA path are active.

## Out of Scope

- Changing the sender's boot or OTA behavior. This feature covers the receiver only.
- Any over-the-air update path for normal (non-maintenance) operation, including the previous
  device-local access-point fallback, which is removed from the normal boot path.
- Provisioning or editing WiFi credentials at runtime.
- Simultaneous ESP-NOW and WiFi operation (explicitly avoided by design).
