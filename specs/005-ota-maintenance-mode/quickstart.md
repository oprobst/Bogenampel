# Quickstart: Receiver OTA Maintenance Mode

## Operator: how to update a deployed receiver

1. Power the receiver **off**.
2. **Hold the debug button** (J5) and apply power; keep holding until the status LED starts the
   **fast short flash** (~100 ms on / 500 ms off). You may release the button now — the mode is
   latched for this power cycle.
3. Wait for the LED to change to the **slow even blink** (~500 ms on / 500 ms off) — the receiver is
   now on the home WiFi with an IP and ready to be flashed.
4. From the workstation, push the firmware:
   ```bash
   # Windows: force UTF-8 so esptool/espota output does not crash the console
   #   PowerShell:  $env:PYTHONIOENCODING="utf-8"; $env:PYTHONUTF8="1"
   pio run -e empfaenger-ota -t upload
   ```
   During the transfer the LED goes **mostly lit with a brief dark blip every ~500 ms**.
5. On success the receiver reboots automatically. With the button **not** held it comes back in
   normal operation (rainbow effect → ESP-NOW).

If the LED keeps the fast short flash and never goes to the slow blink, the home WiFi was not joined
(check the network/credentials). The device stays in maintenance mode and keeps retrying — it never
falls back to an access point.

## Developer: first install (USB bootstrap)

OTA can only update a device that already runs OTA-capable firmware, so the **first** flash of this
firmware must be over USB-C:

```bash
# Windows (PowerShell): UTF-8 is required, otherwise esptool 5.x aborts with UnicodeEncodeError
$env:PYTHONIOENCODING="utf-8"; $env:PYTHONUTF8="1"
pio run -e empfaenger -t upload --upload-port COM3
```

No button press is needed for the USB flash; the XIAO's native USB-Serial/JTAG auto-resets into the
bootloader.

## Finding the receiver's IP

OTA over a direct IP needs no mDNS (and mDNS does not cross the wired-LAN ↔ WiFi subnet boundary in
this network). Set `upload_port` in `platformio.ini [env:empfaenger-ota]` to the receiver's address.
A router-side DHCP reservation on the XIAO's MAC keeps this address stable. To discover it ad hoc,
probe the ArduinoOTA UDP handshake on candidate hosts (a device that answers `AUTH …` on UDP 3232 is
running ArduinoOTA).

## HIL acceptance checklist

| # | Step | Expected | Verifies |
|---|------|----------|----------|
| 1 | Power on **without** button | Rainbow effect, then normal ESP-NOW; **no** `Bogenampel-Empfaenger` AP in a WiFi scan; no router association | FR-005, SC-003 |
| 2 | From the sender, run commands | Received and acted on as before | FR-006, SC-006 |
| 3 | Power on **with** button held | No rainbow/timer/ESP-NOW; LED **fast short flash** | FR-002, FR-004a, SC-001 |
| 4 | Wait for WiFi | LED switches to **slow even blink**; device answers OTA at its IP within 20 s | FR-003, FR-004b, SC-002 |
| 5 | Run `pio run -e empfaenger-ota -t upload` | LED **mostly lit + brief blips**; transfer completes | FR-004c, US1 |
| 6 | After flash | Device reboots; button released → normal operation | FR-008 |
| 7 | In OTA mode, disable the AP / wrong credentials | Stays in maintenance mode, keeps fast flash, never opens own AP, never starts ESP-NOW | FR-009 |
| 8 | Interrupt a transfer (cancel upload mid-way) | Old firmware still runs; device still reachable in maintenance mode | FR-010 |
| 9 | Attempt OTA with wrong password | Rejected | FR-012 |

## Build sanity

```bash
pio run -e empfaenger      # compiles the receiver firmware (no upload)
```
