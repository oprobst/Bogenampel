# Research: Bogenampel V3 Hardware Port

**Feature**: 004-v3-esp32-port | **Date**: 2026-06-09
**Sources**: V2-Code (`Sender/`, `Empfaenger/`), KiCad-Netzlisten aus `Schaltung_v3/BogenampelV3/`
(per `kicad-cli sch export netlist` extrahiert), Datenblätter ESP32-S3/ESP32-C3/MCP73837/TPS62742.

Alle `NEEDS CLARIFICATION` aus dem Technical Context sind aufgelöst. Nummerierte Entscheidungen
R-1 … R-14.

## R-1: e-Paper-Bibliothek (Sender)

**Decision**: **GxEPD2** mit Treiberklasse `GxEPD2_154_D67` (GDEH0154D67 / SSD1681, 200×200 s/w),
dazu Adafruit GFX für Fonts/Primitives.

**Rationale**: Das Waveshare 1.54″ V2-Panel ist ein rohes Panel am 24-poligen FPC mit diskretem
Boost (Si1308EDL + MBR0530, GDR/RESE-Referenzschaltung laut Schaltplan) — elektrisch identisch zum
Waveshare-Modul, daher normale SPI-Ansteuerung. GxEPD2 ist die etablierte Arduino-Bibliothek für
SSD1681, unterstützt **Partial Refresh** (`setPartialWindow` + `nextPage`), Hibernate und freie
Pin-Wahl; aktiv gepflegt, Open Source (Constitution III).

**Alternatives considered**:
- *Waveshare-Beispielcode*: kein Bibliotheks-Charakter, kein sauberes Partial-Window-API.
- *Adafruit EPD*: SSD1681-Support schwächer, Partial Refresh eingeschränkt.
- *LVGL + eigene Flush-Schicht*: massiv überdimensioniert für 5 statische Screens (Constitution II).

**Verwendung**: SPI auf eigenen Pins via `SPI.begin(CLK=14, MISO=-1, MOSI=13, CS=21)`;
Konstruktor `GxEPD2_154_D67(CS=21, DC=47, RST=48, BUSY=38)`. Display erst initialisieren, nachdem
die LOAD-Rail (GPIO7) eingeschaltet ist (FR-018); vor Power-Off `hibernate()` und Rail aus.

## R-2: Refresh-Strategie e-Paper

**Decision**: Voll-Refresh nur bei Screen-/Zustandswechsel; 1-Hz-Countdown und Statuszeile
(Akku/USB/Funk) als Partial Refresh in festen Fenstern; erzwungener Voll-Refresh mindestens einmal
pro Passe-Zyklus (beim Wechsel Schießbetrieb → Pfeile holen) gegen Ghosting.

**Rationale**: SSD1681-Partial-Update dauert ~300–400 ms — passt in den 1-Hz-Takt (FR-012,
SC-007). Ghosting akkumuliert über viele Partial-Updates; der ohnehin stattfindende
Zustandswechsel-Vollrefresh setzt das Panel regelmäßig zurück (Edge Case „e-Paper ghosting").

**Alternatives considered**: Voll-Refresh pro Sekunde (Blitzen, ~2 s — unbrauchbar);
Countdown nur auf Empfänger anzeigen (Verlust an Bedienkomfort gegenüber V2).

## R-3: Funktransport ESP-NOW

**Decision**: ESP-NOW, beide Geräte im Modus `WIFI_STA` (kein AP, kein Connect), fester Kanal 1,
`esp_wifi_set_channel()` nach `WiFi.mode()`. **Discovery statt fester MACs**: Der Sender schickt
beim Start ein HELLO als Broadcast; der Empfänger antwortet unicast; der Sender registriert die
Empfänger-MAC als Peer. Danach ausschließlich Unicast — ESP-NOW liefert dafür Link-Layer-ACKs über
den Send-Callback (`esp_now_send_status_t`).

**Rationale**: Erfüllt FR-006/FR-010 ohne Pairing-Menü und ohne beim Flashen bekannte MACs;
Unicast-ACK ersetzt das NRF24-AutoACK funktional 1:1, sodass der Sender weiterhin pro Kommando
Erfolg/Misserfolg anzeigen kann (FR-007). Reichweite ESP-NOW (1 MBit, 802.11b-Raten) ist erprobt
≥ NRF24@250kbps-Niveau (SC-004).

**Alternatives considered**:
- *Hardcodierte MACs*: bricht bei Board-Tausch (Constitution III „austauschbar").
- *Nur Broadcast*: keine ACKs → FR-007 nicht erfüllbar.
- *WiFi AP + TCP*: Verbindungsaufbau >2 s, Reconnect-Komplexität, mehr Strom.
- *BLE*: geringere Reichweite im Freifeld, Pairing-Aufwand.
- *ESP-NOW v2 Long Range (LR-Protokoll)*: optional später aktivierbar (`WIFI_PROTOCOL_LR`),
  aber nur ESP↔ESP; als Reserve notiert, nicht Teil des Ports.

## R-4: Frame-Format und Duplikat-Unterdrückung

**Decision**: 6-Byte-Frame `{magic[2]='B','3', type, seq, command, checksum}`;
`checksum = command ^ 0xFF` (wie V2). `type` unterscheidet CMD/HELLO/HELLO_ACK/PING/PING_ACK.
`seq` ist ein pro Boot zufällig initialisierter, pro Kommando inkrementierter Zähler; der
Empfänger verwirft Frames mit zuletzt gesehener `seq` (Dedup, FR-008). App-Level-Retry: bis 3
Versuche à 50 ms Abstand, wenn der Send-Callback NACK meldet; Gesamtbudget < 500 ms (FR-007).

**Rationale**: Die 11 Kommandos und die XOR-Prüfsumme aus V2-`Commands.h` bleiben semantisch
unverändert (FR-002); `magic` schützt gegen fremde ESP-NOW-Frames auf demselben Kanal
(Constitution I „Funksignale validieren"); `seq` macht Retries idempotent — wichtig, weil z. B.
ein doppelt verarbeitetes CMD_START die Vorbereitungsphase neu starten würde.

**Alternatives considered**: Verschlüsselung (ESP-NOW LMK/PMK) — möglich, aber Pairing-Aufwand;
für ein Ampelsystem ohne Schutzbedarf verworfen, magic+checksum+seq genügt der Constitution.

## R-5: Verbindungsqualitätstest (Splash)

**Decision**: PING-Typ-Frames, 10 Stück im 250-ms-Raster während des Splash; Qualität = Anteil
bestätigter Zustellungen (Link-Layer-ACK); Anzeige in % wie V2. Läuft nach der Discovery; ohne
HELLO_ACK wird „keine Verbindung" angezeigt und das Menü trotzdem freigegeben (US5).

**Rationale**: Funktionsgleich zum V2-Ping-Test (RF::QUALITY_TEST_*), nutzt aber den
ESP-NOW-Callback statt RF24-ACK-Payloads. 10×250 ms = 2,5 s passt in die Splash-Dauer.

## R-6: Konfigurations-Persistenz

**Decision**: `Preferences` (NVS), Namespace `"bogenampel"`, Keys `"time"` (uint8: 120/240,
gespeichert als Sekunden/2 nicht nötig — uint16 `"timeS"`=120/240) und `"count"` (uint8: 2/4).
Schreiben sofort bei Bestätigung im ConfigMenu (nicht erst beim Ausschalten — Edge Case
„Empty battery"); Lesen im Boot mit Plausibilitätsprüfung, sonst Defaults 120 s / 2.

**Rationale**: NVS ist verschleißarm (Wear Leveling) und atomar; ersetzt EEPROM+CRC8 aus V2 —
die CRC entfällt, weil NVS selbst Integrität sichert; die Wertebereichs-Validierung bleibt.

**Alternatives considered**: EEPROM-Emulation (`EEPROM.h` auf ESP32) — deprecated-Pfad, kein
Vorteil; LittleFS-Datei — überdimensioniert für 2 Bytes.

## R-7: 1-Hz-Zeitbasis

**Decision**: `esp_timer_create` mit periodischem 1-s-Callback, der nur ein `volatile bool`-Flag
setzt; die Verarbeitung läuft im `loop()` (identisches Muster wie der V2-Timer1-Interrupt mit
`timerTick`-Flag). `resetSenderTimer()`-Äquivalent = Timer stop/start für synchronen
Gruppen-Neustart.

**Rationale**: Erhält die V2-Architektur (ISR setzt Flag, Loop arbeitet ab — Constitution-
Codestandard „volatile"), vermeidet FreeRTOS-Task-Komplexität; ±1 s/240 s (SC-003) ist mit
esp_timer (µs-Auflösung) trivial erfüllt. Sowohl Sender als auch Empfänger nutzen dasselbe Muster.

**Alternatives considered**: FreeRTOS-Task mit `vTaskDelayUntil` (zweiter Ausführungskontext,
Locking nötig); `millis()`-Polling (Drift bei langen Loop-Durchläufen, V2 hatte das als Bug).

## R-8: LED-Strip-Ansteuerung (Empfänger)

**Decision**: FastLED (RMT-Backend) auf dem XIAO ESP32C3, Pin D10 (GPIO10); `DisplayManager`
aus V2 wird unverändert übernommen (Segment-Mapping, Farben, `displayTimer`, `setGroup`).

**Rationale**: FastLED unterstützt den C3 über RMT stabil; das komplette 158-LED-Layout inkl.
7-Segment-Mapping ist getestete V2-Logik (FR-003) — kein Grund für Änderungen.
Helligkeit weiterhin global 64–255 vom Poti (FR-022).

**Alternatives considered**: NeoPixelBus (API-Wechsel im DisplayManager nötig, kein Mehrwert);
Adafruit NeoPixel (blockierend, kein globales Brightness-Scaling im V2-Stil).

## R-9: Piezo mit Lautstärkeregelung (Empfänger)

**Decision**: LEDC-PWM auf D3 (GPIO5) am BC337 (J6, 12-V-Transducer; umverdrahtet von D2 am
2026-06-10 — GPIO5 ist zwar ADC2, wird aber rein digital genutzt): Tonfrequenz =
LEDC-Frequenz (2,7 kHz wie V2), Lautstärke = Duty-Cycle 0 … 50 %, vom Lautstärke-Poti (D0/GPIO2,
ADC1) auf eine wahrnehmungsfreundliche Kurve (quadratisch) abgebildet; Minimum hörbar begrenzt
(FR-021). `BuzzerManager` behält seine API (`beep(n)`), intern `tone()`→LEDC ersetzt.

**Rationale**: Duty-Cycle-Steuerung ist die einzige lautstärkewirksame Größe an einem
Transducer hinter einem Schalttransistor; LEDC erlaubt Frequenz+Duty unabhängig. Poti wird
zyklisch (≥10 Hz) gelesen und wirkt live (US4).

**Alternatives considered**: DAC (C3 hat keinen); zweistufige Lautstärke per Vorwiderstand
(Hardware-Änderung, nicht nötig).

## R-10: Lüfter (Empfänger)

**Decision** (aktualisiert 2026-06-10): D6 (GPIO21) als LEDC-PWM-Ausgang über den 2N7002 —
Drehzahl folgt dem neuen Lüfter-Poti an D2 (GPIO4, ADC1_CH4), zyklisch ≥ 10 Hz gelesen wie die
anderen Potis (FR-023 neu; ursprüngliche Minimal-Lösung „an, solange das Gerät läuft" vom
Projektverantwortlichen durch Poti-Regelung ersetzt). Der Lüfter-Tacho wird **nicht
angeschlossen** (Entscheidung 2026-06-10): Das Low-Side-PWM (2N7002 schaltet den GND-Pfad)
zerhackt das Open-Collector-Tachosignal — zuverlässig nur per Pulse-Stretching auswertbar —
und D8/GPIO8 ist ein Strapping-Pin; der Verzicht spart Pull-up, Flash-Störungen und
Komplexität. PWM-Frequenz ≥ 25 kHz (außerhalb des Hörbereichs), damit der Lüfter nicht pfeift.

**Rationale**: LEDC liefert PWM ohne Zusatzaufwand; das Poti macht die Drehzahl ohne Menü
einstellbar (Constitution II). Hinweis: R5 ist Gate-Pull-up an 3V3 → Lüfter läuft
hardware-default voll, bis die Firmware D6 übernimmt (Befund 6 in
`contracts/hardware-pins.md`).

## R-11: Akkumessung (Sender)

**Decision**: `analogReadMilliVolts(GPIO10)` (ADC1_CH9, werkskalibriert), Umrechnung
`V_BAT = V_ADC × (150k + 100k) / 100k = V_ADC × 2,5`; Median-5-Filter und Schwellen
(3,0/3,3/4,2 V) unverändert aus V2 `Battery`-Namespace. Der High-Side-Schalter (Q1/Q2 laut
Schaltplan, „stromlos wenn aus") hängt an +3V3 und braucht keine GPIO-Steuerung.

**Rationale**: GPIO10 liegt auf ADC1 → koexistiert mit ESP-NOW (ADC2 wäre blockiert).
`analogReadMilliVolts` nutzt die eFuse-Kalibrierung → genauer als der V2-5V-Referenz-Ansatz.

## R-12: Lader-Status (Sender)

**Decision**: C_ST1 (GPIO11), C_ST2 (GPIO12), C_PG (GPIO17) als `INPUT_PULLUP` (MCP73837:
Open-Drain-Status), Decodierung laut Datenblatt: ST1/ST2 → laden / fertig / Fehler; PG → gültige
Eingangsspannung. USB_CON (GPIO8, über Spannungsteiler vom VBUS, aktiv HIGH) als zusätzliche
USB-Erkennung. C_PRG (GPIO18, Lade-Strom-Umschaltung über R20) bleibt initial hochohmig
(INPUT) = Default-Ladestrom; Umschaltung auf Schnellladen ist ein dokumentierter, späterer
Opt-in und nicht Teil des Ports.

**Rationale**: Erfüllt FR-017 mit reiner Eingangs-Logik; keine Schreibzugriffe auf den Lader
nötig → keine Risiken für die Ladeschaltung.

## R-13: Taster und Power-Latch (Sender) — inkl. Hardware-Befund

**Decision**:
- **Boot-Reihenfolge**: erste Anweisungen in `setup()`: `pinMode(LATCH=16, OUTPUT)` +
  `digitalWrite(HIGH)` — vor Serial, WiFi und Display (FR-014). Power-Off: Config-Save ist
  bereits erfolgt (R-6), Abschalt-Screen zeichnen, `display.hibernate()`, LOAD-Rail aus,
  `LATCH=LOW`.
- **BTN2** (GPIO9, SW1 gegen GND, kein externer Pullup in der Netzliste): `INPUT_PULLUP`,
  aktiv LOW.
- **BTN1** (GPIO15, aktiv HIGH über Teiler R2/R7 von +BATT, 100 nF Hardware-Entprellung):
  digitaler Eingang ohne interne Pulls (externer 47k-Pulldown R7 vorhanden).
- Gesten im `ButtonManager`: kurz/lang (2 s Alarm im Schießbetrieb, 3 s Power-Off sonst),
  Boot-Lockout, bis BTN1 nach dem Einschalten einmal losgelassen wurde (Edge Case
  „Boot press duration").

**⚠ Hardware-Befund (an Nutzer berichtet, Fix empfohlen)**: BTN1-Pegel = VBAT × R7/(R2+R7) =
VBAT/2 ≈ **1,5 V (leer) … 2,1 V (voll)**. Garantiertes V_IH des ESP32-S3 = 0,75 × 3,3 V =
**2,48 V** → der Pegel liegt im undefinierten Bereich; analoges Lesen scheidet aus, weil GPIO15
auf **ADC2** liegt (mit aktivem Funk unbrauchbar). **Empfohlene Korrektur im Schaltplan: R2 =
47k belassen, R7 47k → 240k** ⇒ Pegel 0,836 × VBAT = 2,51 … 3,51 V (Reserve zu V_IH bei leerem
Akku und zum abs. Maximum 3,6 V beim Laden; zulässiges Fenster R7 ≈ 220k–280k, 220k akzeptabel).
Der Power-On-Pfad (R3 → D1 → EN) ist von der Änderung unberührt.
Gemäß Constitution V wird die Änderung zuerst im KiCad-Schaltplan dokumentiert. Die Firmware ist
von der Widerstandsänderung unabhängig (digitale Logik bleibt gleich).

**Alternatives considered**: BTN1 per ADC2 lesen (kollidiert mit ESP-NOW); RTC-GPIO-Umweg
(GPIO15 ist kein sauberer Workaround wert); Pegel akzeptieren (funktioniert „typisch", verletzt
aber Datenblatt-Spezifikation → gegen Constitution I/III).

## R-14: Build-Setup PlatformIO (aktualisiert 2026-06-11: PlatformIO-only)

**Decision** (aktualisiert 2026-06-11 — Arduino-IDE-Pfad auf Entscheidung des
Projektverantwortlichen gestrichen, vollständige Migration auf PlatformIO + ESP32,
Constitution v2.2.0): Pro Firmware ein `platformio.ini` im Sketch-Ordner mit
`[platformio] src_dir = .`:
- `SenderV3`: `board = esp32-s3-devkitc-1`, `board_build.flash_size = 16MB`,
  PSRAM-Flags für N16R8 (`-DBOARD_HAS_PSRAM`, `board_build.arduino.memory_type = qio_opi`),
  USB-CDC on Boot (natives USB an GPIO19/20), `lib_deps = zinggjm/GxEPD2, adafruit/Adafruit GFX Library`.
- `EmpfaengerV3`: `board = seeed_xiao_esp32c3`, `lib_deps = fastled/FastLED`.

**Rationale**: `pio run` ist der einzige unterstützte und automatisiert verifizierbare
Build-Pfad (SC-011, CI-fähig); Libraries kommen versioniert über `lib_deps` statt aus dem
eingecheckten `libraries/`-Ordner (der nur noch V2 dient). Die flache Sketch-Struktur mit
`.ino` bleibt als Projekt-Konvention erhalten (Konsistenz mit V2), ist aber keine
IDE-Anforderung mehr. Die Versions-Guards im Code (ESP-NOW-Callback-Signaturen,
LEDC-API für arduino-esp32 2.x/3.x) bleiben als Robustheit gegen Core-Updates der
PIO-Platform bestehen.

**Alternatives considered**: Arduino IDE als zweiter Build-Pfad (gestrichen — doppelte
Pflege ohne Nutzen, kein automatisierter Check); ein gemeinsames PIO-Projekt mit zwei
`src_dir`-Envs (zwei getrennte, selbständige Firmware-Ordner sind übersichtlicher);
arduino-cli-Skripte (zweites Toolsystem ohne Mehrwert).

## Offene Punkte

Keine — alle Unbekannten aus dem Technical Context sind entschieden. Zwei Punkte erfordern
**Hardware-Aktionen des Nutzers** (vor dem ersten HIL-Test, beide im Schaltplan zu
dokumentieren, Constitution V):
1. Helligkeits-Poti von D5 auf **D1** umverdrahten (bereits in Spec festgeschrieben).
2. **R7 47k → 220k** am BTN1-Teiler (Befund R-13).
