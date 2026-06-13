# Phase 0 Research: Receiver OTA Maintenance Mode

All Technical Context items were resolvable from the existing codebase and ESP32-C3 platform
knowledge; there are no open NEEDS CLARIFICATION items.

## R1 — Where and how to read the button at boot

**Decision**: Configure `pinMode(Pins::BTN_DEBUG, INPUT_PULLUP)` and read it as the first thing in
`setup()` *after* the mandatory strapping-pin fixes (`POTI_GND` LOW, `fan.begin()`), then debounce
by sampling a few times over ~20–30 ms and requiring a stable `LOW` (button = active-low against
GND, J5). Latch the result into a `bootOtaMode` flag that decides the rest of `setup()`/`loop()`.

**Rationale**: The boot decision must happen *before* the ~5 s rainbow effect and before
`radio.begin()`/`OTAManager::begin()`, otherwise we would already have started ESP-NOW. D7/GPIO20 is
**not** a strapping pin on the ESP32-C3 (strapping pins are GPIO2/8/9), so reading it early is safe.
A short multi-sample debounce avoids a noisy single-read false negative (SC-001 needs 100 %
determinism).

**Alternatives considered**: (a) Reusing the existing `checkButton()` debounce loop — rejected: it
runs in `loop()` long after init has started. (b) Holding a boot-strap combination or a config flag
in NVS — rejected: less discoverable, contradicts the simple "hold the button while powering on"
gesture the user specified.

**Caveat**: D7/GPIO20 is also UART0 RX. Serial debug over UART0 is unused in the shipped build
(`DEBUG_ENABLED 0`), so there is no conflict; this is unchanged from today (the debug button already
lives on D7).

## R2 — Normal path vs OTA path: keeping the single radio out of conflict

**Decision**: Make the two paths mutually exclusive at the radio level.

- **Normal path** (button not held): call `radio.begin()` exactly as today (ESP-NOW, fixed
  channel 1). Do **not** call any WiFi-join/SoftAP code. `OTAManager::begin()` is **not** called.
- **OTA path** (button held): do **not** call `radio.begin()` (no ESP-NOW). Put the radio in plain
  `WIFI_STA` mode and join the configured network, then start ArduinoOTA.

**Rationale**: ESP-NOW pins the radio to channel 1; a WiFi-STA association moves the radio to the
router's channel. Running both is the root cause of the current flakiness and of broken ESP-NOW when
STA succeeds. Choosing exactly one per boot eliminates the conflict (FR-006). `WIFI_STA` (not
`WIFI_AP_STA`) is correct for the OTA path because no access point is wanted (FR-005/FR-009).

**Alternatives considered**: (a) Keep `WIFI_AP_STA` and a SoftAP fallback — rejected: it is the
current design and is exactly what we are removing (unreachable from a wired workstation, and it
masks STA failure). (b) Park ESP-NOW and time-slice channels — rejected: complex, unreliable, and
unnecessary because OTA is a deliberate maintenance action that does not need ESP-NOW.

## R3 — Removing the SoftAP fallback while keeping OTA reachable when WiFi is slow

**Decision**: In the OTA path, call `WiFi.mode(WIFI_STA)`, `WiFi.setAutoReconnect(true)`,
`WiFi.begin(ssid, pass)` and **keep retrying indefinitely** (re-issue `WiFi.begin()` on a timeout
budget) instead of falling back to SoftAP. Start `ArduinoOTA` once; its UDP listener binds on the STA
interface and begins answering as soon as an IP exists. Drive the LED phase from
`WiFi.status()`/`WiFi.localIP()` and ArduinoOTA callbacks.

**Rationale**: FR-009 requires staying in maintenance mode and continuing to blink rather than
stranding on an unreachable AP. Auto-reconnect plus periodic re-`begin()` makes the join robust
against a slow/briefly-absent AP. ArduinoOTA over a direct IP needs no mDNS, so mDNS not crossing
subnets is irrelevant for the flash itself.

**Alternatives considered**: Single 10 s attempt then give up (today's behavior) — rejected: that is
the intermittent failure the user reported.

## R4 — LED signalling for the three OTA sub-phases (FR-004a/b/c)

**Decision**: A small non-blocking signaller, ticked every `loop()` iteration, with an explicit
phase enum mapped to active-low D9 (`STATUS_LED_ON = LOW`, `STATUS_LED_OFF = HIGH`):

| Phase | Trigger | Pattern (period) | On-time |
|-------|---------|------------------|---------|
| `CONNECTING` | OTA mode entered, `WiFi.status() != WL_CONNECTED` or no IP | fast short flash, period 600 ms | ~100 ms on / ~500 ms off |
| `READY` | connected + IP assigned, idle | slow even blink, period 1000 ms | ~500 ms on / ~500 ms off |
| `UPDATING` | between ArduinoOTA `onStart` and `onEnd` | inverted brief blip, period 600 ms | mostly on, ~100 ms off every ~500 ms |

Implemented with `millis()` timestamps (no `delay()`), so `ArduinoOTA.handle()` keeps running and
transfers are not stalled.

**Rationale**: Matches the user's explicit timings. Non-blocking is mandatory: ArduinoOTA must be
serviced continuously during a transfer (FR-004c runs concurrently with the flash). Driving phase
from ArduinoOTA `onStart`/`onEnd` cleanly distinguishes READY from UPDATING.

**Alternatives considered**: Reusing the normal-mode status-LED logic — rejected: that logic encodes
"steady on = ready, brief off on RX" which would collide with FR-011 distinctness.

## R5 — Fan/strapping safety in OTA mode

**Decision**: Keep the early hardware-safe init that is independent of mode: `POTI_GND` LOW and
`fan.begin()` (which takes over the PWM line so the fan does not sit at minimum/again-undefined),
exactly as today, *before* the button read. Skip the LED-strip rainbow, buzzer, ESP-NOW, timer, and
poti loops in OTA mode.

**Rationale**: The `R5` pull-up strapping fix for the fan PWM line and the `POTI_GND` strapping fix
are hardware-correctness obligations regardless of mode (Constitution III, "Fehlbeschaltung
geschützt"). Everything else is normal-operation behavior not wanted in maintenance mode.

**Alternatives considered**: Doing nothing hardware-wise in OTA mode — rejected: would leave the fan
PWM line in its boot-default state per the Config.h note (Befund 6).

## R6 — Build/flash bootstrap (USB) and the Windows encoding pitfall

**Decision**: Document that the *first* install of this firmware must go over USB
(`pio run -e empfaenger -t upload`) and that on Windows the upload requires
`PYTHONIOENCODING=utf-8` (and `PYTHONUTF8=1`) in the environment, because esptool 5.x emits Unicode
progress glyphs that crash under the cp1252 console codec. Subsequent updates use the button-at-boot
OTA path (`pio run -e empfaenger-ota -t upload`).

**Rationale**: Observed during this feature's bring-up: the USB upload aborted with
`UnicodeEncodeError: 'charmap' codec ...` until UTF-8 output was forced. Capturing it here prevents
the next maintainer from misdiagnosing it as a bootloader/connect problem.

**Alternatives considered**: Enabling Windows long-path / changing console codepage globally —
rejected as environment-specific; the per-invocation env var is the minimal reliable fix.
