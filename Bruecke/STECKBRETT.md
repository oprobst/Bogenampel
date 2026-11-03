# Bluetooth-Brücke — Steckbrett-Aufbau

Stand 2026-11-03. Ziel: prüfen, ob ESP-NOW-Empfang und Bluetooth-Audio (A2DP)
auf **einem** ESP32 zuverlässig nebeneinander laufen, bevor es eine Platine gibt.

## Teile

- ESP32-DevKitC V4 (ESP32-WROOM-32 — **nicht** S3/C3, die können kein A2DP)
- LCD 16x2 HD44780, 5-V-Variante, parallel (16 Pins)
- 10-kΩ-Trimmer für den Kontrast
- KY-040-Drehgeber
- Bluetooth-Box
- Versorgung zunächst über USB (Akku folgt später)

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

Der **Empfänger** braucht ebenfalls die neue Firmware (er sendet die
Signal-Frames), am einfachsten per OTA:

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

Beim allerersten Start (noch keine Box gespeichert) startet die Box-Suche
automatisch. Nach dem Einschalten wartet die Bluetooth-Bibliothek etwa 10 s,
bevor sie sucht oder neu verbindet („BT startet ...").

## Was getestet werden soll

1. **Koexistenz**: Der serielle Monitor zeigt pro Signal
   `Signal 3x (Signale N, Kopien K)`. Der Empfänger sendet jedes Signal 3×;
   bei störungsfreiem Empfang ist K ≈ 3·N. Fehlt ein ganzes Signal, sieht man
   das an einer Lücke in N gegenüber den Piepern des Empfängers.
2. **Tonqualität**: Aussetzer oder Knacken während ESP-NOW-Frames ankommen?
3. **Latenz**: Wie weit liegt die Box hinter dem Piezo? Erwartet 150–250 ms.
4. **Wiederverbinden**: Box aus- und wieder einschalten — verbindet die
   Brücke von selbst neu (Heartbeat alle 10 s)?
5. **Box-Abschaltung**: Bleibt die Box an, obwohl die meiste Zeit Stille
   gestreamt wird?
6. **Stromaufnahme** am USB-Messgerät, mit und ohne LCD-Hintergrundlicht —
   Grundlage für die Akku-Auslegung.
