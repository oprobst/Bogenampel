# Bogenampel

Eine funkgesteuerte Timer-Anzeige für Bogenschießplätze.
Sie funktioniert möglichst einfach, mit minimalen Benutzereingaben:
An der Bedieneinheit wird der Modus (120 oder 240 Sekunden, 1-2 oder 3-4 Schützen)
vorausgewählt, danach steuern zwei Taster den kompletten Turnierablauf.

Die Anlage besteht aus zwei Geräten:

- **Sender (Bedieneinheit)** — liegt an der Schießlinie, e-Paper-Display, akkubetrieben
- **Empfänger (Anzeigeeinheit)** — steht am Ziel, große 7-Segment-LED-Anzeige mit Ampelfarben

![Schaltplan Sender](schaltplan-sender.png)

## Hardware

| | Sender (Bedieneinheit) | Empfänger (Anzeigeeinheit) |
|---|---|---|
| **Controller** | ESP32-S3-WROOM-1U-N16R8 (16 MB Flash, 8 MB PSRAM) | Seeed XIAO ESP32C3 |
| **Anzeige** | 1.54″ e-Paper, 200×200 (SSD1681) | LED-Strip WS2811 12 V, 158 LEDs |
| **Funk** | ESP-NOW, Kanal 1 (im Chip integriert) | ESP-NOW, Kanal 1 |
| **Versorgung** | LiPo-Akku + MCP73837-Lader, USB-C | 12 V extern oder 5 V USB (JP1) |
| **Bedienelemente** | 2 Taster (CONFIG, OK) | Debug-Taster, 3 Potis |
| **Sonstiges** | Soft-Power-Latch (TPS62742) | Piezo 12 V, geregelter Lüfter |
| **Firmware** | [`Sender/`](Sender/) | [`Empfaenger/`](Empfaenger/) |
| **Schaltplan** | [`Schaltung-Sender/`](Schaltung-Sender/) | [`Schaltung-Empfaenger/`](Schaltung-Empfaenger/) |

Die verbindliche Pin-Belegung steht in
[`specs/004-v3-esp32-port/contracts/hardware-pins.md`](specs/004-v3-esp32-port/contracts/hardware-pins.md)
und wird aus den KiCad-Netzlisten abgeleitet. Bei Abweichungen gilt der Schaltplan —
Änderungen gehören zuerst dorthin, dann in den Code.

Die 158 LEDs verteilen sich auf 16 LEDs Gruppe A/B, 16 LEDs Gruppe C/D und
3 × 42 LEDs für die dreistellige 7-Segment-Anzeige.

## Bedienung

Der Sender hat zwei Taster. Welcher Taster einschaltet, legt der Power-Latch in der
Hardware fest — das ist die **CONFIG**-Taste.

| Taste | kurz | 3× schnell klicken | 3 s halten |
|---|---|---|---|
| **CONFIG** | Weiter / Wert ändern | — | Ausschalten |
| **OK** | Bestätigen / Passe beenden | Alarm | Ausschalten |

Einschalten: CONFIG drücken. Ausschalten: eine der beiden Tasten 3 Sekunden halten.
Die Konfiguration (Schießzeit, Schützenzahl) bleibt im NVS erhalten und steht beim
nächsten Einschalten wieder zur Verfügung.

**Alarm**: die **OK**-Taste dreimal schnell hintereinander drücken (höchstens 0,4 s
zwischen den Klicks). Das geht im Schießbetrieb und beim Pfeileholen — also überall
dort, wo Leute an der Schießlinie stehen können. Im Konfigurationsmenü und im
Startbildschirm bleibt die Folge bewusst wirkungslos. CONFIG löst grundsätzlich
keinen Alarm aus: Diese Taste blättert durch Menüs und zählt Werte hoch, dort ist
zügiges Tippen normale Bedienung. Quittiert wird ein Alarm mit einem kurzen OK.

Ausschalten und Alarm liegen absichtlich auf **verschiedenen** Bewegungen: Halten
schaltet immer aus, egal in welchem Zustand das Gerät gerade ist. Früher hing die
Bedeutung des Haltens am Betriebszustand — wer im Schießbetrieb ausschalten wollte,
löste stattdessen einen Alarm aus.

**Automatische Abschaltung**: Wird im Konfigurationsmenü oder in „Pfeile holen" 60 Minuten
lang keine Taste gedrückt, schaltet sich der Sender selbst ab und zeigt dabei
„Automatische Abschaltung". Im Schießbetrieb und bei stehendem Alarm greift das
bewusst **nicht** — dort läuft eine Passe bzw. ein Sicherheitszustand. Gedacht ist es
gegen das im Koffer vergessene Gerät; dagegen hilft keine Stromsparmaßnahme, nur
Ausschalten.

## Ablauf einer Passe

| Phase | Anzeige | Dauer |
|---|---|---|
| **Stopp** | Rot, `000` | bis zum Start |
| **Vorbereitung** | Rot, Countdown | 10 s |
| **Schießzeit** | Grün, Countdown | 120 s oder 240 s |
| **letzte 30 s** | Orange, Countdown | 30 s |
| **Ende** | Rot, `000` | — |

Der Piezo quittiert die Phasenwechsel mit kurzen Tonfolgen (3250 Hz), das Passenende
mit drei Tönen. Ein ausgelöster Alarm blinkt und piept achtmal.

**Wichtig für die Sicherheit**: Der Empfänger zählt eine gestartete Passe **autonom**
zu Ende. Fällt der Funk aus oder geht der Sender aus, läuft der Timer korrekt ab und
schaltet danach auf Rot — er bleibt nicht in Grün stehen.

### Gruppen und halbe Passe

Bei 3-4 Schützen wird zwischen den Gruppen A/B und C/D umgeschaltet; die Anzeige folgt
einem 4er-Zyklus. Zusätzlich lässt sich eine halbe Passe starten, wenn nur noch die
zweite Gruppe schießt.

## Funk

ESP-NOW auf Kanal 1, ohne externes Funkmodul und ohne Pairing: Der Sender sucht den
Empfänger beim Start per Broadcast und merkt sich dessen MAC-Adresse zur Laufzeit.

Jeder Frame ist 6 Byte groß und trägt Magic-Bytes, eine Prüfsumme und eine
Sequenznummer; der Empfänger verwirft doppelte und fehlerhafte Pakete. Übertragen
werden 11 Kommandos (Start 120/240, Stopp, Init, Alarm, Ping, Gruppenwahl, halbe Passe).
Beim Start misst der Sender die Verbindungsqualität mit 10 Pings und zeigt das Ergebnis
im Splash-Screen.

> **Ein Radio, ein Kanal.** Der ESP32 kann nicht gleichzeitig ESP-NOW auf Kanal 1 und
> WLAN auf einem anderen Kanal betreiben. Deshalb läuft im Normalbetrieb **kein** WiFi —
> weder ein Accesspoint noch eine Netzwerkverbindung. Für Updates gibt es den
> Wartungsmodus (siehe unten).

## Stromverbrauch des Senders

Gemessen am laufenden Gerät: **0,39 W → 0,20 W**, also etwa doppelte Akkulaufzeit.
Zwei Maßnahmen, beide in der Firmware:

| Maßnahme | Ersparnis |
|---|---|
| CPU-Takt 240 → 80 MHz (`System::CPU_FREQ_NORMAL_MHZ`) | ~10 mA |
| ESP-NOW-Empfangsfenster duty-cycled statt dauerhaft offen | ~38 mA |

Der Löwenanteil war das Funkmodul: ESP-NOW hält den Empfänger per Default dauerhaft an.
Der Sender ist nach der Discovery aber ein reiner Sender — das ACK auf eigene Frames
kommt im Sendefenster zurück, also darf das Empfangsfenster zu bleiben. Während der
Discovery wird es nur um den HELLO-Broadcast herum geöffnet. Details und Voraussetzungen
stehen in [`espnow-protocol.md`](specs/004-v3-esp32-port/contracts/espnow-protocol.md),
Regel 7.

Das **e-Paper ist für den Verbrauch praktisch irrelevant** (unter 2 %) — der
Sekundencountdown im Schießbetrieb ist bewusst nicht angetastet. Nicht möglich ist das
Abschalten des PSRAM: `CONFIG_SPIRAM=1` steckt in allen vorkompilierten
arduino-esp32-Varianten und die Initialisierung hängt nicht an `-DBOARD_HAS_PSRAM`.
Weiter runter käme man nur mit abgeschaltetem Radio zwischen den Kommandos und
manuellem Light-Sleep — die automatische Variante scheidet aus, weil `CONFIG_PM_ENABLE`
in den vorkompilierten Libs fehlt.

## Bauen und Flashen

PlatformIO ist der einzige unterstützte Build-Pfad. Die `platformio.ini` liegt **zentral
im Repo-Root**, nicht in den Firmware-Ordnern — das Repo-Root ist das Arbeitsverzeichnis.
Bibliotheken kommen versioniert über `lib_deps`, es muss nichts manuell installiert werden.

```bash
pio run -e sender                 # bauen
pio run -e empfaenger

pio run -t upload -e sender       # per USB flashen
pio device monitor -e sender      # 115200 Baud

pio run -e sender-release         # Feld-Build: ohne Debug-Ausgaben und CDC-Task
```

Drei Stolpersteine, die alle wie Hardwaredefekte aussehen:

**Beim Sender muss CONFIG während des gesamten Uploads gedrückt bleiben.** Der Reset von
esptool lässt sonst den Power-Latch fallen; das Gerät schaltet sich mitten im Flashen ab
und der USB-Port verschwindet. Zu beachten: `pio run -t upload` startet esptool erst nach
gut einer Minute Build- und Dependency-Scan — so lange muss der Taster gehalten werden.
Praktischer ist, das Gerät **mit gehaltenem CONFIG einzuschalten** (dann greift der
Boot-Lockout und das Halten löst keine Power-Off-Geste aus) und esptool direkt mit den
vier Images aufzurufen.

**Die Upload-Baudrate darf nicht hochgesetzt werden** (`upload_speed = 115200` in der
`platformio.ini`). Der Sender flasht über die native USB-Serial/JTAG-Peripherie, wo die
Baudrate bedeutungslos ist — esptool wechselt beim Board-Default 460800 trotzdem, der Port
re-enumeriert dabei und der Upload stirbt mitten im Stub-Flasher mit
`PermissionError(13) ... Cannot configure port`.

**Unter Windows vor jedem Upload `$env:PYTHONIOENCODING="utf-8"` setzen.** Sonst bricht
der Vorgang mit `UnicodeEncodeError` und `[upload] Error 4294967295` ab — esptool schreibt
Unicode-Fortschrittsbalken, die eine cp1252-Konsole nicht kodieren kann. Auf den Chip
wurde zu diesem Zeitpunkt noch nichts geschrieben.

### OTA-Wartungsmodus

Beide Geräte lassen sich drahtlos aktualisieren, aber nur in einem eigenen Betriebsmodus —
im Normalbetrieb ist WiFi aus (siehe Kasten oben).

- **Sender**: beide Taster gleichzeitig gedrückt halten und einschalten
- **Empfänger**: mit gehaltenem Debug-Taster (D7) einschalten

Der Sender zeigt daraufhin einen Wartungs-Screen mit WLAN-Status und **seiner eigenen
IP-Adresse**; beim Empfänger signalisiert die Status-LED durch langsames Blinken, dass er
bereit ist. Die IP wird als `upload_port` in der `platformio.ini` eingetragen (eine feste
DHCP-Reservierung ist empfehlenswert, mDNS-Namen lösen über Subnetzgrenzen nicht
zuverlässig auf).

```bash
pio run -t upload -e sender-ota            # Sender, Debug-Build (serieller Monitor bleibt)
pio run -t upload -e sender-release-ota    # Sender, Feld-Build (ohne Debug-Ausgaben/CDC)
pio run -t upload -e empfaenger-ota
```

`sender-release-ota` ist der Auslieferungsweg: gleicher Wartungsmodus, aber das
Image aus `sender-release`. Danach meldet sich kein serieller Port mehr — der
Rückweg auf den Debug-Build geht weiterhin per OTA über `sender-ota`, solange der
Wartungsmodus erreichbar bleibt.

Voraussetzung ist eine `wifi_credentials.h` im Repo-Root — Vorlage:
[`wifi_credentials.h.example`](wifi_credentials.h.example). Die Datei ist bewusst nicht
eingecheckt. Fehlt sie, meldet der Wartungsmodus das auf dem Display und wartet; einen
Accesspoint-Fallback gibt es absichtlich nicht, weil ein zweites Netz auf fremdem Kanal
ESP-NOW stilllegen würde.

Zur Fehlersuche: **ArduinoOTA lauscht auf UDP 3232, nicht TCP.** Ein TCP-Portscan meldet den
Port folgerichtig als geschlossen, auch wenn der Wartungsmodus einwandfrei läuft — daraus
lässt sich also nichts über die Erreichbarkeit ableiten. espota schickt eine UDP-Einladung,
woraufhin das Gerät eine TCP-Rückverbindung zum Host aufbaut.

Der Zugang ist nicht durch ein Passwort geschützt, sondern dadurch, dass der Modus nur mit
physisch gedrückten Tastern erreichbar ist. Grund: Das ArduinoOTA-Passwort (PBKDF2) ist mit
der `espota.py` von PlatformIO nicht kompatibel, die Authentifizierung scheitert stumm.

## Projektstruktur

```text
platformio.ini          zentrale Build-Konfiguration (beide Geräte)
Sender/                 Firmware Bedieneinheit (ESP32-S3)
Empfaenger/             Firmware Anzeigeeinheit (XIAO ESP32C3)
Schaltung-Sender/       KiCad-Projekt Sender
Schaltung-Empfaenger/   KiCad-Projekt Empfänger
schaltplan-*.png        Schaltplan-Exporte, ohne KiCad lesbar
specs/                  Feature-Spezifikationen, Pin-Contract, Abnahme-Checkliste
```

Weiterführend: [`HARDWARE.md`](HARDWARE.md) für die Hardware-Spezifikation,
[`specs/004-v3-esp32-port/quickstart.md`](specs/004-v3-esp32-port/quickstart.md) für die
vollständige Inbetriebnahme- und Abnahme-Checkliste.

## Bekannte offene Punkte

- **Funk mit geschlossenem Empfangsfenster noch nicht im Turnierablauf verifiziert**: Dass
  das Link-ACK bei `esp_now_set_wake_window(0)` zuverlässig zurückkommt, ist durch die
  Strommessung und den Treiber-Kontrakt gestützt, aber ein vollständiger Durchlauf
  (Start, Gruppenwechsel, Alarm, Stop, Empfänger im Betrieb ausschalten → Relink) steht
  aus. Falls Kommandos zicken: `Radio::PS_WINDOW_CLOSED_MS` in `Sender/Config.h` von 0 auf
  10 setzen — kostet ~10 mA, hält das Fenster aber zu 10 % offen.
- **Pegelwandler am LED-Strip**: Die 12-V-WS2811-LEDs erwarten 5-V-Datenpegel, der XIAO
  liefert 3,3 V. Bei hellen Mischfarben (Gelb, Weiß) kippen dadurch vereinzelt Bits.
  Abhilfe ist ein 74AHCT125 am Daten-Pin — Bauteil vorhanden, Einbau steht aus.
- **Ladestrom**: Der Sender lädt mit 80–100 mA. Auf 500 mA umschalten geht **nicht per
  Firmware**, auch wenn die Leitung dafür vorbereitet aussieht: Der PROG2-Eingang des
  MCP73837 verlangt für „High" mindestens 0,8 × VDD = 4,0 V (VDD = VUSB = 5 V), ein
  ESP32-GPIO liefert nur 3,3 V — und landet damit im Shutdown-Fenster, der Lader schaltet
  ab. Schnellladen braucht eine Hardware-Änderung (R20 als Pull-up nach VUSB, ggf. mit
  MOSFET zum Umschalten). Details in
  [`hardware-pins.md`](specs/004-v3-esp32-port/contracts/hardware-pins.md), Befund 4.

## Historie

**Version 2** (bis 2026-08-01) basierte auf zwei Arduino Nanos mit nRF24L01+-Funkmodulen,
einem ST7789-TFT am Sender und einer EEPROM-Konfiguration. Timer-Ablauf, Ampelfarben,
Gruppenlogik und die 11 Funk-Kommandos wurden unverändert nach V3 übernommen.

Firmware, Bibliotheken und KiCad-Projekt von V2 wurden aus dem Arbeitsverzeichnis
entfernt und sind über die Git-Historie zugänglich (letzter Stand: Commit `e632bfb`).
Beachte dabei: Die Ordnernamen `Sender/` und `Empfaenger/` bezeichnen **vor** diesem
Zeitpunkt die V2-, danach die V3-Firmware.

```bash
git show e632bfb:Sender/Sender.ino      # V2-Quelltext ansehen
```
