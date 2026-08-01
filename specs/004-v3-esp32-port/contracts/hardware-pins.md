# Contract: Pin-Belegung V3 (verbindlich für Config.h beider Firmwares)

**Feature**: 004-v3-esp32-port | **Quelle**: KiCad-Netzlisten `Schaltung-Sender/` bzw. `Schaltung-Empfaenger/`
(`BogenampelV3.kicad_sch` Rev 3.0, `Zusatzplatine-Empfänger.kicad_sch`), extrahiert per
`kicad-cli sch export netlist`. Bei Abweichungen gilt der Schaltplan; Änderungen zuerst dort
(Constitution V).

## Sender — ESP32-S3-WROOM-1U-N16R8 (IC2, „Universal-Fernbedienung")

| Funktion | GPIO | Richtung | Beschaltung / Hinweise |
|----------|------|----------|------------------------|
| LATCH (Power halten) | 16 | OUT | über D1 (BAS40-05) an EN des TPS62742; **erste Aktion in setup(): HIGH**; LOW = Selbstabschaltung |
| LOAD (3V3_LOAD-Rail Display) | 7 | OUT | TPS62742 LOAD-Eingang; HIGH vor Display-Init, LOW vor Power-Off |
| BTN1 (Power/OK, SW2) | 15 | IN | **aktiv HIGH**, externer Teiler R2/R7 an +BATT, 100 nF (C1); keine internen Pulls; ⚠ siehe Befund unten |
| BTN2 (Weiter, SW1) | 9 | IN | gegen GND, **kein externer Pullup** → `INPUT_PULLUP`, aktiv LOW |
| ADC_BAT | 10 | IN (ADC1_CH9) | Teiler R4/R8 = 150k/100k an +BATT → V_BAT = V_ADC × 2,5; Sensor stromlos wenn aus (Q1/Q2) |
| USB_CON | 8 | IN | VBUS-Teiler (R16/R18), aktiv HIGH bei USB |
| C_ST1 (Lader STAT1) | 11 | IN | MCP73837 Open-Drain → `INPUT_PULLUP` |
| C_ST2 (Lader STAT2) | 12 | IN | MCP73837 Open-Drain → `INPUT_PULLUP` |
| C_PG (Lader Power Good) | 17 | IN | MCP73837 Open-Drain → `INPUT_PULLUP` |
| C_PRG (Ladestrom) | 18 | IN (hochohmig) | über R20 an PROG2; Default-Ladestrom; Schnellladen = späterer Opt-in |
| e-Paper CS | 21 | OUT | J1 Pin 12 (CS_D) |
| e-Paper DC | 47 | OUT | J1 Pin 11 |
| e-Paper RES | 48 | OUT | J1 Pin 10 |
| e-Paper BUSY | 38 | IN | J1 Pin 9 |
| SPI CLK | 14 | OUT | J1 Pin 13 |
| SPI MOSI | 13 | OUT | J1 Pin 14 (kein MISO — e-Paper write-only) |
| USB D-/D+ | 19/20 | — | natives USB (CDC für Debug/Flash) |
| reserviert | 35–37 | — | Octal-PSRAM (N16R8) — NICHT benutzen |

Display: Waveshare 1.54″ e-Paper V2 (GDEH0154D67/SSD1681, 200×200) als rohes Panel an J1
(24-pol FPC) mit diskretem Boost (Q3 Si1308EDL, D2–D4 MBR0530) — Ansteuerung wie Standard-Modul.

### Hardware-Befunde Sender

1. ✅ **BTN1-Pegel (R-13) — im Schaltplan umgesetzt (verifiziert 2026-06-10: R2=47k, R7=240k;
   Bestückung auf der Platine → T003b)**: Der ursprüngliche Teiler R2/R7 = 47k/47k lieferte
   VBAT/2 = 1,5–2,1 V; garantiertes V_IH des ESP32-S3 = 2,48 V → undefinierter Bereich. Mit
   R7 = 240k liegt der Pegel bei 2,51–3,51 V — Reserve zu V_IH bei leerem Akku UND zum abs.
   Maximum 3,6 V beim Laden (zulässiges Fenster für R7 bei R2=47k: ca. 220k–280k). Der
   Power-On-Pfad (R3 → D1 → EN) ist von der Änderung unberührt. GPIO15 liegt auf ADC2 →
   analoges Lesen mit aktivem Funk keine Option.

## Empfänger — Seeed XIAO ESP32C3 (U2, Zusatzplatine/Lochraster)

| Funktion | XIAO-Pin | GPIO | Richtung | Beschaltung / Hinweise |
|----------|----------|------|----------|------------------------|
| Poti Lautstärke (J2) | D0 | 2 | IN (ADC1) | Schleifer über R1 1k; Fußpunkt an D4 (POTI_GND) — Strapping-Fix Befund 2, im Schaltplan umgesetzt |
| **Poti Helligkeit (J3)** | **D1** | **3** | IN (ADC1) | **Umverdrahtung von D5!** (GPIO7 hat keinen ADC; GPIO5/D3 wäre ADC2 = mit Funk unbrauchbar); über R2 1k |
| Piezo (J6, 12-V-Transducer) | **D3** | **5** | OUT (LEDC) | über R3 2k2 → BC337 (Q1), R4 10k Basis-Pulldown (stumm beim Boot); **umverdrahtet von D2 (2026-06-10)** — GPIO5 ist ADC2, wird aber rein digital genutzt |
| **Poti Lüfter-Drehzahl (J8, neu)** | **D2** | **4** | IN (ADC1) | neu 2026-06-10: Schleifer über R7 1k, stellt die Lüfter-PWM (D6) ein; ADC1_CH4, kein Strapping-Pin |
| Lüfter PWM (J9) | D6 | 21 | OUT (LEDC) | direkt am Gate des 2N7002 (Q2); R5 10k = **Pull-up an 3V3** → Lüfter läuft hardware-default voll, bis die Firmware übernimmt; Versorgung 12 V/5 V via JP1 |
| Taster (J5) | D7 | 20 | IN | gegen GND → `INPUT_PULLUP`, aktiv LOW (Debug-/Testtaster) |
| Status-LED (D1-LED) | D9 | 9 | OUT | **aktiv LOW**: 3V3 → LED → R6 220 Ω → Pin (sinkt Strom) — Strapping-Fix Befund 3, umgesetzt 2026-06-10 |
| WS2812B Data (J7) | D10 | 10 | OUT | 158 LEDs; Strip-Versorgung 12 V laut Stecker — Pegel/Typ beim Aufbau prüfen (12-V-Strips sind WS2815-kompatibel zum Protokoll) |
| POTI_GND | D4 | 6 | OUT | geschalteter Fußpunkt des Lautstärke-Potis J2 Pin 3 (Befund 2); in `setup()` OUTPUT LOW |
| frei | D5, D8 | 7, 8 | — | Reserve (D5 nach Helligkeits-Poti-Umzug frei; D8 nach Tacho-Verzicht frei) |

Versorgung: 5 V USB (J1) oder 12 V Boost (J4) → TSR 0.5-2433 → 3V3. Kein Akku am XIAO
(Batt-Pads unbeschaltet). LED-Strip-Versorgung immer extern (5 V/12 V via Jumper) — nie über
den USB-Port des XIAO.

### Hardware-Befunde Empfänger (Boot-Strapping ESP32-C3)

Der ESP32-C3 hat drei Strapping-Pins (GPIO2, GPIO8, GPIO9), deren Pegel beim Reset den
Boot-Modus bestimmen. Stand 2026-06-10 (per Netzlisten-Export verifiziert): **alle drei
Strapping-Befunde sind im Schaltplan gelöst.**

2. ✅ **GPIO2/D0 (Lautstärke-Poti) — umgesetzt**: GPIO2 muss beim Reset HIGH sein, ein
   Poti-Teiler gegen GND hätte das je nach Knopfstellung verhindert. Lösung im Schaltplan:
   Poti-Fußpunkt (J2 Pin 3) liegt an **D4 (GPIO6, POTI_GND)** statt GND. Beim Reset ist D4
   hochohmig → kein Strom durch das Poti → GPIO2 liegt über die Poti-Bahn an 3V3 (HIGH
   garantiert in jeder Knopfstellung). Die Firmware setzt D4 in `setup()` auf OUTPUT LOW,
   danach arbeitet der Teiler normal (Offset durch R_ON vernachlässigbar).
3. ✅ **GPIO9/D9 (Status-LED) — umgesetzt**: GPIO9 muss beim Reset HIGH sein (LOW =
   Bootloader-Modus); die ursprüngliche aktiv-HIGH-LED hätte den Pin auf ~1,7–2 V geklemmt
   (< V_IH ≈ 2,48 V). Lösung: LED aktiv LOW (3V3 → LED → R6 → Pin); Firmware invertiert.
4. ✅ **GPIO8/D8 (Lüfter-Tacho) — gegenstandslos**: Tacho wird nicht angeschlossen
   (Entscheidung 2026-06-10; Low-Side-PWM zerhackt das Open-Collector-Signal ohnehin, nur per
   Pulse-Stretching nutzbar). J9 Pin 3 bleibt offen, D8 ist frei.
5. ✅ **J1 „5V USB" — korrigiert (2026-06-10)**: war ein Schaltplanfehler (Pin 2 lag auf dem
   +5V-Netz, kein GND-Pin). Jetzt: J1 Pin 1 = GND, Pin 2 = +5V; das +5V-Netz speist den
   3V3-Regler (U1), den Boost-Eingang (J4 Pin 1) und JP1/JP2 (verifiziert per Netzliste).
6. ℹ **Lüfter-Default**: R5 ist Gate-Pull-up an 3V3 → Lüfter läuft beim Boot/Flashen voll,
   bis die Firmware D6 übernimmt. Funktional unkritisch; falls unerwünscht, R5 als Pull-down
   nach GND ausführen.

### Hinweis Alt-Belegung V2 (zur Abgrenzung)

V2 nutzte: LED-Strip D3, Buzzer D4, 3 Status-LEDs A2–A4, Debug-Jumper D2, Poti A7, NRF24 D8/D9.
Diese Belegung gilt für V3 NICHT mehr; `EmpfaengerV3/Config.h` wird ausschließlich aus der
obigen Tabelle befüllt.
