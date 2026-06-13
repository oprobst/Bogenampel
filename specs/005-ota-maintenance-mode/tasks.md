# Tasks: Receiver OTA Maintenance Mode (button-at-boot)

**Input**: Design documents from `/specs/005-ota-maintenance-mode/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: No automated test tasks — this firmware has no host unit-test harness (research.md);
validation is Hardware-in-the-Loop per `quickstart.md`. HIL acceptance steps are included as the
last task of each story.

**Organization**: Tasks are grouped by user story. The whole feature touches three files
(`EmpfaengerV3/EmpfaengerV3.cpp`, `EmpfaengerV3/OTAManager.{h,cpp}`, `EmpfaengerV3/Config.h`), so
[P] (parallel) opportunities are limited — same-file tasks are sequential by necessity.

## Path Conventions

Embedded firmware, flat directory (project convention, PlatformIO-only build). All paths under
`EmpfaengerV3/` at repository root.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Constants and a known-good baseline before behavioral changes.

- [x] T001 [P] Add OTA-mode LED timing constants to `EmpfaengerV3/Config.h` (new `namespace OtaSignal` or extend `Timing`): `CONNECTING` period 600 ms / on 100 ms; `READY` period 1000 ms / on 500 ms; `UPDATING` period 600 ms / off-blip 100 ms — per `contracts/ota-led-signal-contract.md`.
- [x] T002 Confirm baseline build is green before changes: run `pio run -e empfaenger` and verify it compiles (Windows: set `PYTHONIOENCODING=utf-8`). Also confirm an **OTA-capable partition table** (two app slots): the size report must show the app partition ≈ `0x140000` (1.31 MB, default 4 MB dual-OTA scheme) with the firmware fitting inside; if a single-app (`huge_app`) scheme is set, OTA and rollback (FR-010) are impossible — switch the partition scheme in `platformio.ini` first.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The single boot-mode decision and the setup()/loop() fork that BOTH user stories build on.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [x] T003 In `EmpfaengerV3/EmpfaengerV3.cpp`, implement the boot-mode decision and fork per `contracts/boot-mode-contract.md` C1: after the early strapping-safe steps (`POTI_GND` LOW, `fan.begin()`), set `pinMode(Pins::BTN_DEBUG, INPUT_PULLUP)`, read it debounced (stable LOW over ~20–30 ms multi-sample), latch a `bootOtaMode` flag, and branch `setup()` (and `loop()`) into a normal path vs an OTA path. At this step the normal path keeps today's exact sequence and the OTA path is an empty stub. Add forward decls / helper functions `setupNormal()/loopNormal()` and `setupOta()/loopOta()`.

**Checkpoint**: Holding D7 at boot reaches the OTA stub (nothing happens yet but timer/ESP-NOW do NOT start); not holding boots exactly as before.

---

## Phase 3: User Story 1 - Flash the receiver on demand over the network (Priority: P1) 🎯 MVP

**Goal**: With the button held at boot, the receiver joins WiFi (station-only, no SoftAP) and is
flashable over the LAN; a normal restart follows a successful flash.

**Independent Test**: Power on with D7 held → device answers the ArduinoOTA handshake at its IP
within 20 s and a full authenticated `pio run -e empfaenger-ota -t upload` completes and reboots.

### Implementation for User Story 1

- [x] T004 [US1] Rework `OTAManager::begin()` in `EmpfaengerV3/OTAManager.cpp` to **station-only**: `WiFi.mode(WIFI_STA)`, `WiFi.setAutoReconnect(true)`, `WiFi.begin(OTA::WIFI_SSID, OTA::WIFI_PASS)`; **remove the SoftAP fallback branch** entirely (no `WiFi.softAP(...)`). Per `contracts/boot-mode-contract.md` C3 and research R2/R3.
- [x] T005 [US1] Add indefinite-retry + state accessors to `EmpfaengerV3/OTAManager.{h,cpp}`: on a connect-timeout budget re-issue `WiFi.begin()` (never give up / never fall back), and expose `bool isConnected()` (returns `WiFi.status()==WL_CONNECTED && WiFi.localIP()` valid) for the LED and loop (FR-009).
- [x] T006 [US1] In `EmpfaengerV3/OTAManager.cpp` keep `ArduinoOTA.setHostname(OTA::HOSTNAME)` and `setPassword(OTA::PASSWORD)` so the endpoint requires auth (FR-012); ensure `ArduinoOTA.begin()` runs as part of the OTA path only (called from `setupOta()`).
- [x] T007 [US1] Fill `setupOta()` in `EmpfaengerV3/EmpfaengerV3.cpp`: do NOT call `radio.begin()`, the rainbow effect, buzzer, or poti loops; call `OTAManager::begin()` (depends on T004–T006). Per contract C3.
- [x] T008 [US1] Fill `loopOta()` in `EmpfaengerV3/EmpfaengerV3.cpp`: call `OTAManager::handle()` every iteration plus the WiFi-retry servicing from T005, non-blocking (no `delay()` that could starve OTA).
- [ ] T009 [US1] Build (`pio run -e empfaenger`) then HIL-validate per `quickstart.md` rows #3 (enters OTA, no normal start), #4 (reachable < 20 s), #5–#6 (flash + reboot), #9 (wrong password rejected).

**Checkpoint**: The receiver can be updated over the air by holding the button at boot — MVP delivered.

---

## Phase 4: User Story 2 - Normal power-on is unaffected and the radio link is reliable (Priority: P2)

**Goal**: A button-free boot runs ESP-NOW only — no WiFi join, no access point — and the
sender↔receiver link is no longer disturbed by WiFi.

**Independent Test**: Repeatedly power on without the button → no `Bogenampel-Empfaenger` AP in a
WiFi scan, no router association, and sender commands act with prior reliability.

### Implementation for User Story 2

- [x] T010 [US2] Finalize `setupNormal()` in `EmpfaengerV3/EmpfaengerV3.cpp`: ensure it calls `radio.begin()` (ESP-NOW) and does NOT call `OTAManager::begin()` or any WiFi/SoftAP code (contract C2, FR-005). Confirm the existing rainbow/timer/buzzer/poti/status-LED sequence stays intact.
- [x] T011 [US2] Ensure `loopNormal()` in `EmpfaengerV3/EmpfaengerV3.cpp` does NOT call `OTAManager::handle()`; the existing ESP-NOW/timer/alarm/poti/fan/status-LED loop body is preserved unchanged.
- [x] T012 [US2] Simplify `RadioManager::begin()` in `EmpfaengerV3/RadioManager.cpp` from `WIFI_AP_STA` to `WIFI_STA` (no access point needed for ESP-NOW) while keeping `esp_wifi_set_channel(Radio::CHANNEL)`; verify ESP-NOW receive still works (contract C4 — exactly one radio role). Treat as careful change; revert to `WIFI_AP_STA` if ESP-NOW regresses.
- [ ] T013 [US2] Build then HIL-validate per `quickstart.md` rows #1 (no AP, normal ESP-NOW boot) and #2 (sender commands received), covering SC-003 and SC-006.

**Checkpoint**: Normal operation is clean and conflict-free; US1 and US2 both work independently.

---

## Phase 5: User Story 3 - The maintenance state is visually unmistakable (Priority: P3)

**Goal**: In OTA mode the status LED shows three distinct, non-blocking patterns (joining / ready /
updating) per FR-004a–c.

**Independent Test**: Enter OTA mode and confirm fast-flash → slow-blink → mostly-lit-with-blips as
the phase advances; confirm none of these appear in normal mode.

### Implementation for User Story 3

- [x] T014 [US3] Add an `OtaPhase` enum (`CONNECTING`/`READY`/`UPDATING`) and a non-blocking LED signaller (millis-based, active-low D9 via `STATUS_LED_ON/OFF`) in `EmpfaengerV3/OTAManager.{h,cpp}` (or `EmpfaengerV3.cpp`), using the T001 timing constants per `contracts/ota-led-signal-contract.md`.
- [x] T015 [US3] Wire the phase source in `EmpfaengerV3/OTAManager.cpp`: derive `CONNECTING`/`READY` from `isConnected()` (T005); set/clear an `updating` flag in `ArduinoOTA.onStart` / `onEnd` / `onError` so `UPDATING` overrides while a transfer runs, and `onError` returns to READY/CONNECTING (FR-004c, FR-010).
- [x] T016 [US3] Call the signaller every iteration of `loopOta()` in `EmpfaengerV3/EmpfaengerV3.cpp` (after `OTAManager::handle()`); confirm the LED never blocks `ArduinoOTA.handle()`.
- [ ] T017 [US3] HIL-validate per `quickstart.md` rows #3 (fast flash while joining), #4 (slow even blink when ready), #5 (mostly-lit blips during transfer), and confirm normal mode shows none of them (SC-004, FR-011).

**Checkpoint**: All three user stories are independently functional.

---

## Phase 6: Polish & Cross-Cutting Concerns

- [x] T018 [P] In `EmpfaengerV3/Config.h`, mark/remove the now-unused SoftAP constant(s) (`OTA::AP_SSID`) and update the `// OTA (WiFi SoftAP + ArduinoOTA)` header comment to reflect station-only OTA.
- [x] T019 [P] Add/refresh Doxygen + inline comments referencing connectors and FR ids (J5 debug button, D9 status LED active-low, FR-001/FR-004a-c, FR-009) per Constitution V across the changed files.
- [x] T020 Update `platformio.ini [env:empfaenger-ota]`: note the receiver's current/reserved IP for `upload_port`, and add the Windows `PYTHONIOENCODING=utf-8` upload hint as a comment (research R6).
- [ ] T021 Run the full `quickstart.md` HIL checklist (#1–#9), including #7 (WiFi unavailable → stays in maintenance, keeps fast flash, no AP/ESP-NOW) and #8 (interrupted transfer → old firmware intact), and record pass/fail.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: no dependencies — start immediately.
- **Foundational (Phase 2)**: depends on Setup; BLOCKS all user stories (creates the fork).
- **User Stories (Phase 3–5)**: depend on Foundational. US1 and US2 are independent branches of the
  fork. US3 layers on the OTA path (needs US1's OTA mode + `isConnected()` + ArduinoOTA callbacks).
- **Polish (Phase 6)**: after the targeted stories are complete.

### User Story Dependencies

- **US1 (P1)**: after Foundational. Independent of US2.
- **US2 (P2)**: after Foundational. Independent of US1 (different branch of the fork).
- **US3 (P3)**: after Foundational; depends on US1 (T005 `isConnected()`, T006 ArduinoOTA callbacks).

### Within / across stories (same-file ordering)

- `EmpfaengerV3.cpp` is edited by T003, T007, T008, T010, T011, T016 → these are **sequential**.
- `OTAManager.{h,cpp}` is edited by T004→T005→T006, then T014→T015 → **sequential**.
- T012 (`RadioManager.cpp`) is independent of the above files.

### Parallel Opportunities

- T001 (Config.h) can run in parallel with reading/prep, and T018/T019 (polish, different concerns)
  are marked [P]. Most behavioral tasks share `EmpfaengerV3.cpp`/`OTAManager.*` and cannot parallelize.
- US1 and US2 can be developed by two people in parallel **only** if they coordinate edits to
  `EmpfaengerV3.cpp` (the fork branches are separate functions, which helps).

---

## Implementation Strategy

### MVP First (User Story 1)

1. Phase 1 Setup → 2. Phase 2 Foundational (the fork) → 3. Phase 3 US1 (OTA path).
4. **STOP and VALIDATE**: hold button at boot, flash over LAN, confirm reboot (quickstart #3–6, #9).
5. This already restores reliable OTA — ship/demo.

### Incremental Delivery

1. Foundation ready (fork in place).
2. US1 → button-at-boot OTA works (MVP).
3. US2 → normal boot cleaned (no AP, ESP-NOW solid) → demo.
4. US3 → three-phase LED feedback → demo.

### Notes

- Commit after each task or logical group.
- The first install of this firmware must be over USB (`pio run -e empfaenger -t upload`, UTF-8);
  OTA can only update an already-OTA-capable device (quickstart "Developer: first install").
- Keep `delay()` out of `loopOta()` so `ArduinoOTA.handle()` is never starved during a transfer.
