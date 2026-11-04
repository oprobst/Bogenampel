# Bluetooth-Brücke — Steckbrett-Aufbau

Stand 2026-11-04. Ziel: prüfen, ob die Brücke direkt neben dem Empfänger im
Ampel-Gehäuse laufen kann, ohne dessen ESP-NOW-Empfang zu stören, bevor es eine
Platine gibt. Die Signaltöne kommen per Kabel (UART), nicht mehr per Funk — der
ESP32 braucht also kein ESP-NOW neben Bluetooth.

## Teile

- ESP32-DevKitC V4 (ESP32-WROOM-32 — **nicht** S3/C3, die können kein A2DP)
- LCD 16x2 HD44780, 5-V-Variante, parallel (16 Pins)
- 10-kΩ-Trimmer für den Kontrast
- KY-040-Drehgeber
- Bluetooth-Box
- Versorgung am Steckbrett über USB (im Gehäuse später aus der Ampel)

## Verdrahtung

### LCD (4-Bit-Modus)

| LCD-Pin | Signal | DevKitC |
|---|---|---|
| 1 VSS | GND | GND |
| 2 VDD | +5 V | **5V** |
| 3 V0 | Kontrast | Schleifer Trimmer (Enden an 5V und GND) |
| 4 RS | | GPIO25 |
| 5 R/W | | **GND** (nur schreiben — schützt den ESP32 vor 5 V) |
| 6 E | | GPIO26 |
| 7–10 D0–D3 | | frei |
| 11 D4 | | GPIO27 |
| 12 D5 | | GPIO14 |
| 13 D6 | | GPIO32 |
| 14 D7 | | GPIO33 |
| 15 A | Licht + | 5V (Vorwiderstand nur, wenn auf der LCD-Platine keiner sitzt) |
| 16 K | Licht − | GND |

### KY-040

| KY-040 | DevKitC |
|---|---|
| GND | GND |
| + | **3V3** (nicht 5V — die Pull-ups auf dem Modul hängen an +) |
| CLK | GPIO18 |
| DT | GPIO19 |
| SW | GPIO23 |

Läuft die Lautstärke verkehrt herum: CLK und DT tauschen. Springt sie pro
Rastung um zwei: `Ui::ENC_STEPS_PER_DETENT` in `Config.h` auf 2.

### Signalleitung zum Empfänger

| Empfänger (XIAO) | | DevKitC |
|---|---|---|
| D5 / GPIO7 | ca. 330 Ω in Reihe | GPIO4 |
| GND | | GND (**ohne gemeinsame Masse kein Signal**) |

UART, 9600 Baud, nur Empfänger → Brücke. Telegramm: `0xA5`, Anzahl Töne (1–8),
`0xA5 ^ Anzahl`. Im KiCad-Schaltplan ist das Netz `BT` (Blatt `BluetoothBridge`).

## Flashen

```bash
pio run -t upload -e bruecke
pio device monitor -e bruecke
```

**Beim Flashen LCD-Hintergrundlicht (Pin 15) abziehen.** Mit Licht brach der
Upload mehrfach bei 6–35 % ab („No more data to read from the serial port"),
ohne lief er beim ersten Versuch durch. Außerdem bringt die Auto-Reset-Schaltung
dieses DevKitC den Chip nicht zuverlässig in den Download-Modus: **BOOT halten**,
bis „Writing at …" läuft.

**Per OTA** (ab dem ersten USB-Flash mit dieser Version): Drehgeber-Taster beim
Einschalten halten, bis das LCD „Wartungsmodus" und die IP zeigt. Dann:

```bash
pio run -t upload -e bruecke-ota   # weicht die IP ab: --upload-port <IP>
```

Im Wartungsmodus läuft kein Bluetooth und im Normalbetrieb kein WLAN. Die
Zugangsdaten kommen aus `wifi_credentials.h`. Bootloader und Partitionstabelle
lassen sich per OTA nicht ändern; dafür bleibt der USB-/UART-Weg nötig.

Der **Empfänger** braucht ebenfalls die neue Firmware (er schickt die Telegramme
auf die Leitung), am einfachsten per OTA:

```bash
pio run -t upload -e empfaenger-debug-ota
```

Mit dem Debug-Build (und `sender-debug-ota`) läuft eine Passe in unter einer
Minute durch, und es kommen in kurzer Zeit viele Signale zusammen.

## Bedienung

| Modus | Drehen | Kurz drücken | Lang drücken (1 s) |
|---|---|---|---|
| Normal | Lautstärke (mit Vorhörton) | Testton | Box-Suche (Neustart) |
| Box-Suche | durch die Geräte blättern | Gerät wählen | Abbruch (Neustart) |

Taster beim Einschalten gehalten = Wartungsmodus (OTA).

Beim allerersten Start (noch keine Box gespeichert) startet die Box-Suche
automatisch. Nach dem Einschalten wartet die Bluetooth-Bibliothek etwa 10 s,
bevor sie sucht oder neu verbindet („BT startet ...").

## Versorgung

Im Gehäuse (V3.1): eigener TSR0.5-2433 aus den 12 V für die Brücke, die 5 V fürs LCD
kommen provisorisch von TP3 des Empfängers. In V4 sauber lösen, ebenso UART-Zugang
und BOOT/EN. Der Piezo piept parallel zur Box weiter; stumm per Lautstärke-Poti.

**5 V nur an LCD Pin 2, Pin 15 und das obere Ende des Kontrast-Trimmers. ESP32 und
KY-040 bekommen 3,3 V.** Liegen 5 V auf der 3V3-Schiene, stürzt die Brücke beim Start
ab: Guru Meditation, und im seriellen Monitor erscheint nur Salat aus `00`/`E0`/`F0`.
Außerdem sollten USB und ein zweiter 3,3-V-Regler (TSR0.5-2433) nicht gleichzeitig
die Schiene speisen.

## AFH-Kanalsperre

Die Brücke sperrt für Bluetooth die Kanäle 0–21 (2402–2423 MHz), damit der Sender
den ESP-NOW-Kanal 1 auslässt. Der Log bestätigt nach dem Start
`AFH-Sperre aktiv: BT-Kanaele 0-21 gesperrt`. Das heißt nur, dass der eigene
Controller den Befehl angenommen hat — ob die Box und der Funkbetrieb davon
profitieren, zeigt erst der Vergleichstest unten.

## Was getestet werden soll

1. **Signalleitung**: Der serielle Monitor zeigt pro Signal
   `Signal 3x (Signale N, verworfen V)`. N muss zu den Piepern des Empfängers
   passen. V darf beim Einschalten des Empfängers kurz steigen, im Betrieb nicht —
   sonst streut etwas ein (LED-Datenleitung, Lüfter-PWM).
2. **Tonqualität**: Aussetzer oder Knacken, auch mit der AFH-Sperre?
3. **Latenz**: Wie weit liegt die Box hinter dem Piezo? Erwartet 150–250 ms.
4. **Wiederverbinden**: Box aus- und wieder einschalten — verbindet die
   Brücke von selbst neu (Heartbeat alle 10 s)?
5. **Box-Abschaltung**: Bleibt die Box an, obwohl die meiste Zeit Stille
   gestreamt wird?
6. **Stromaufnahme** am USB-Messgerät, mit und ohne LCD-Hintergrundlicht —
   Grundlage für die Versorgung aus der Ampel.
7. **Einbau-Test**: Brücke im Ampel-Gehäuse neben dem Empfänger, Dauerton läuft,
   Sender in größter realer Entfernung. Kommen alle Befehle an, wie viele
   Wiederholungen braucht es? Vergleich: Brücke aus, Brücke an ohne und mit
   AFH-Sperre (zum Abschalten den Aufruf in `AudioManager::update()` auskommentieren).
   Stand 2026-11-04: im Einbauabstand ohne Auffälligkeiten gelaufen, Befehle nicht gezählt.
