# Hardware-Übersicht: Bogenampel V3

Diese Übersicht ist aus den KiCad-Projekten abgeleitet (Stand 2026-10-26):

| Gerät | KiCad-Projekt | Titel im Schaltplan | Rev. / Datum |
|---|---|---|---|
| Sender (Bedieneinheit) | `Schaltung-Sender/BogenampelV3.kicad_sch` / `.kicad_pcb` | „Universal-Fernbedienung ESP32" | 3.0 / 2026-05-29 |
| Empfänger (Anzeigeeinheit) | `Schaltung-Empfaenger/Empfaenger.kicad_sch` / `.kicad_pcb` | „Bogenampel Empfänger — ESP32 Version 12V per USB-DP" | 3 / 2026-08-03 |

Autoritativ ist immer der KiCad-Schaltplan (Constitution V: „Schaltplan zuerst, dann
Code"). Die verbindliche Pin-Tabelle für beide Firmwares steht in
[`specs/004-v3-esp32-port/contracts/hardware-pins.md`](specs/004-v3-esp32-port/contracts/hardware-pins.md).
Exporte ohne KiCad: `doc/Sender_Schaltplan.png`, `doc/Sender_PCB.png`,
`doc/Empfänger_Schaltplan.png`, `doc/Empfänger_PCB.png`.

Die beiden Geräte sind nur über Funk verbunden (ESP-NOW, Kanal 1) — es gibt keine
Kabelverbindung zwischen Sender und Empfänger.

---

## 1. Sender (Bedieneinheit)

Akkubetriebene Handeinheit mit e-Paper-Display und zwei Tastern.

### 1.1 Blockschaltbild

```text
USB-C (J2) ──FB1──► VCC (5 V) ──► MCP73837 (IC3) ──► +BATT ◄── LiIon-Zelle (J4), NTC (J3)
   │  D+/D- über USBLC6 (U2) ──► ESP32-S3 IO19/IO20 (natives USB)
   └─ VCC/2 ──► USB_CON (IO8)

+BATT ──► TPS62742 (IC1, EN = BTN1 ODER LATCH) ──► +3V3 ──► ESP32-S3-WROOM-1U (IC2)
                                               └─ LOAD-Schalter (CTRL = IO7) ──► +3V3_LOAD
                                                                                  │
                                                     e-Paper-Boost (Q3, L2, D2–D4)┘──► VGH / VGL
                                                                                       │
                                     ESP32 SPI (IO13/14/21/47/48/38) ──► 24-pol FPC (J1) ┘ e-Paper
```

### 1.2 Hauptbauteile

| Ref. | Bauteil | Funktion |
|---|---|---|
| IC2 | **ESP32-S3-WROOM-1U-N16R8** | MCU, 16 MB Flash, 8 MB Octal-PSRAM (GPIO35–37 dadurch belegt). Variante **-1U**: keine Leiterplattenantenne, **U.FL-Anschluss** für externe Antenne (verbaut: SMA-Pigtail) |
| IC1 | **TPS62742DSSR** | Step-down +BATT → **3,3 V** (VSEL1–4 alle an +BATT), 360 nA Ruhestrom, integrierter Lastschalter (LOAD-Ausgang → `+3V3_LOAD`) |
| IC3 | **MCP73837T-FCI/UN** | LiIon-Lader aus USB (VUSB), Ladestrom über R19 = 2 k → **500 mA**; PROG2 an IO18 (USB 100/500 mA, R20 100 k Pull-down); NTC-Eingang an J3 |
| J1 | Hirose **FH12-24S-0.5SH** | 24-pol FPC für das rohe e-Paper-Panel **Waveshare 1.54″ V2** (GDEH0154D67 / SSD1681, 200×200) |
| J2 | **UJ20-C-H-G-MSMT-4B-P16-TR** | USB-C-Buchse (USB 2.0), CC1/CC2 über R15/R16 = 5,1 k an GND (Sink, 5 V) |
| U2 | **USBLC6-2SC6** | ESD-Schutz für D+/D- |
| SW1, SW2 | Würth 430476085716 (12 mm) | Bedientaster OK bzw. CONFIG |
| SW3, SW4 | 6-mm-Taster | Reset (EN) bzw. Boot (IO0) — Service-Taster auf der Platine |
| J3 / J4 | Stiftleiste 2-pol | NTC / Akku |

Akku (nicht auf der Platine): eine LiIon-/LiPo-Zelle, verbaut ist eine **14500 mit 1500 mAh**.

### 1.3 Stromversorgung und Soft-Power

- **Einschalten**: SW2 (BTN1) legt +BATT über R2 47 k / R3 100 k und die Dioden-ODER-Schaltung
  D1 (**BAS40-05**) auf den EN-Pin des TPS62742 (R10 1 M Pull-down). Die Firmware setzt
  sofort **LATCH (IO16) = HIGH** — der zweite Zweig von D1 hält EN dann auch nach dem
  Loslassen. **LATCH = LOW schaltet das Gerät ab.** Deshalb kann nur CONFIG (GPIO15) das
  Gerät einschalten; das ist Hardware und per Firmware nicht verlegbar.
- **LOAD-Rail**: Der LOAD-Ausgang des TPS62742 (CTRL an IO7, R11 1 M Pull-down) speist
  `+3V3_LOAD` für den e-Paper-Boost. Display aus = Rail aus.
- **Akkumessung**: +BATT → Q1 (**IRLML9301**, P-Kanal) → Teiler R4 150 k / R8 100 k
  (C2 100 nF) → R5 1 k → **IO10** (ADC1, Faktor **×2,5**). Q1 wird über Q2 (**IRLML6346**,
  Gate an +3V3 über R6 47 k) nur durchgeschaltet, solange +3V3 anliegt — im
  ausgeschalteten Zustand zieht der Teiler keinen Strom aus dem Akku.
- **USB-Erkennung**: VCC über R17/R18 (je 100 k) → **IO8** (`USB_CON`, aktiv HIGH).
- **Lader-Status**: STAT1/STAT2/PG an IO11/IO12/IO17, PROG2 an IO18.
- Messpunkte: TP1 BAT, TP2 3V3, TP3 3V3L, TP4 GND, TP5 VGL, TP6 VGH.

### 1.4 e-Paper-Ansteuerung

Das Panel hängt **ohne fertiges Modul** direkt am FPC-Stecker J1; die Hochspannung für das
Panel erzeugt eine diskrete Boost-/Ladungspumpen-Schaltung nach Waveshare-Referenz:

- Schalt-MOSFET Q3 **Si1308EDL** (Gate = `GDR` vom SSD1681, R12 10 k Pull-down),
  Strommess-Shunt R13 = 3 Ω (`RESE`), Speicherdrossel L2 **10 µH** (Coilcraft LPS3314)
  aus `+3V3_LOAD`
- Dioden D2/D3/D4 **MBR0530T1G** und C6 4,7 µF/50 V → `VGH` / `VGL`
- Stützkondensatoren 1 µF/50 V an VSH1, VSH2, VSL, VGH, VGL, VCOM, VDD, VCI (C7–C14)
- SPI ist **write-only** (kein MISO)

### 1.5 Taster

| Taster | Net | GPIO | Beschaltung | Rolle (Firmware) |
|---|---|---|---|---|
| SW2 | BTN1 | 15 | an +BATT, Teiler R2 47 k / R7 240 k, C1 100 nF → **aktiv HIGH**, keine internen Pulls | **CONFIG** (Weiter/Ändern), schaltet das Gerät ein |
| SW1 | BTN2 | 9 | gegen GND, interner Pull-up → **aktiv LOW** | **OK** (Bestätigen, Dreifachklick = Alarm) |

Halten ≥ 3 s (beliebige Taste) = Ausschalten; beide beim Einschalten halten = OTA-Wartungsmodus.
Die Zuordnung Rolle → Pin steht ausschließlich in `ButtonManager::readRawState()`.

### 1.6 Pinbelegung ESP32-S3

| Funktion | GPIO | | Funktion | GPIO |
|---|---|---|---|---|
| LATCH (Power halten) | 16 | | e-Paper CS (`CS_D`) | 21 |
| LOAD (Display-Rail) | 7 | | e-Paper DC | 47 |
| BTN1 = CONFIG (aktiv HIGH) | 15 | | e-Paper RES | 48 |
| BTN2 = OK (aktiv LOW) | 9 | | e-Paper BUSY | 38 |
| ADC_BAT (ADC1, ×2,5) | 10 | | SPI CLK | 14 |
| USB_CON (aktiv HIGH) | 8 | | SPI MOSI | 13 |
| Lader STAT1 / STAT2 | 11 / 12 | | USB D- / D+ | 19 / 20 |
| Lader PG / PROG2 | 17 / 18 | | | |

**Strapping/Boot**: IO0 (R22 10 k Pull-up, SW4 Boot), EN (R21 10 k + C20 1 µF, SW3 Reset),
IO3 (R23 10 k Pull-up), IO45 und IO46 (R24/R25 10 k Pull-down).
**Frei**: IO1, IO2, IO4–IO6, IO39–IO42, RXD0/TXD0. **Belegt durch PSRAM**: IO35–IO37.

### 1.7 Platine

- 2 Lagen, Umriss **L-förmig**, Gesamtmaß **147 × 36 mm** (breiter Teil 89 × 36 mm,
  schmaler Ausleger 58 × 20 mm)
- 3 Befestigungslöcher M2,5
- Passive Bauteile überwiegend 1206 (Widerstände) bzw. 0805/0603 (Kondensatoren)

---

## 2. Empfänger (Anzeigeeinheit)

Netzbetriebene Anzeige mit WS2811-LED-Strip, Piezo, Lüfter und drei Reglern.
PD-12V-Platine, Rev. 2026-08-03.

### 2.1 Blockschaltbild

```text
USB-C-Buchse ──6-pol JST (J1)──► CH224K (U2): fordert 12 V per USB-PD an
                     │
                   VBUS ──► Q1 AO3401A (Verpolungsschutz) ──► +12V ──┬─► WS2811-Strip (J7)
                                                    D2 SMBJ13A (TVS) ┤─► Lüfter (J6)
                                                       C8 47 µF      ├─► Piezo (J8, über Q3)
                                                                     ├─► TSR0.5-2433 (U4) ──► +3V3 ──► XIAO ESP32C3 (U3)
                                                                     └─► L7805 (U1) ──► +5V ──► Pegelwandler U5
```

### 2.2 Hauptbauteile

| Ref. | Bauteil | Funktion |
|---|---|---|
| U3 | **Seeed XIAO ESP32C3** | MCU (4 MB Flash), Versorgung über den **3V3-Pin**; VUSB und Batt+/- unbeschaltet |
| U2 | **CH224K** | USB-PD-Sink-Trigger; CFG1 = GND, CFG2 = GND, CFG3 = VDD → **12 V**. VDD über R1 1 k (C3/C11), VBUS-Sense über R2 10 k |
| Q1 | **AO3401A** (P-Kanal) | Verpolungsschutz VBUS → +12V, Gate-Teiler R8 20 k / R7 10 k |
| D2 | **SMBJ13A** | TVS-Diode am +12V-Netz |
| U4 | **TRACO TSR0.5-2433** | Schaltregler 12 V → **3,3 V** für den XIAO |
| U1 | **L7805** (TO-220) | 12 V → **5 V**, versorgt **nur** den Pegelwandler U5 |
| U5 | **74AHCT1G125** | Pegelwandler 3,3 → 5 V für die WS2811-Datenleitung, /OE fest an GND; Ausgang über R14 330 Ω an J7 |
| Q2 | **BSS138** (N-Kanal) | Lüfter-PWM, Open-Drain auf die PWM-Leitung |
| Q3 | **BC337** (NPN, TO-92) | Piezo-Treiber |
| D1 | LED 5 mm | PD-OK-Anzeige: +3V3 → R6 470 Ω → D1 → PG-Pin des CH224K (leuchtet, wenn 12 V ausgehandelt) |
| D3 | LED 5 mm | Status-LED, **aktiv LOW**: +3V3 → D3 → R9 220 Ω → D9 |

Netzteil: USB-C-PD-Netzteil, das **12 V / ≥ 2 A** liefert.

### 2.3 Steckverbinder (alle JST-XH, 2,5 mm)

| Ref. | Polzahl | Belegung | Gegenstück |
|---|---|---|---|
| J1 | 6 | 1 VBUS, 2 CC1, 3 CC2, 4 D-, 5 D+, 6 GND | USB-C-Buchse (Gehäuse) |
| J2 | 3 | 1 +3V3, 2 Schleifer, 3 GND | Poti 47 k **Lüfter** |
| J3 | 3 | 1 +3V3, 2 Schleifer, 3 **POTI_GND (D4)** | Poti 47 k **Lautstärke** |
| J4 | 3 | 1 +3V3, 2 Schleifer, 3 GND | Poti 47 k **Helligkeit** |
| J5 | 2 | 1 GND, 2 BTN (D7) | Debug-Taster (Wartungsmodus beim Boot) |
| J6 | 4 | 1 GND, 2 +12V, 3 Tacho (**unbeschaltet**), 4 PWM | 4-Pin-PC-Lüfter 12 V |
| J7 | 3 | 1 +12V, 2 Data (über R14), 3 GND | WS2811-Strip |
| J8 | 2 | 1 +12V, 2 Kollektor Q3 | Piezo-Transducer 12 V |

### 2.4 Peripherie im Detail

- **WS2811-Strip**: 66 Pixel (2 × 12 Gruppen-Pixel + 3 × 14 Ziffern-Pixel), direkt am
  +12V-Netz. Daten von D10 über U5 auf 5-V-Pegel — behebt die früheren Farbkipper bei
  3,3-V-Pegel. U5 hat **keinen Eingangs-Pulldown**, deshalb legt die Firmware GPIO10 in
  `setup()` früh auf OUTPUT LOW.
- **Piezo**: 12-V-Transducer zwischen +12V und Kollektor Q3; R10 2k2 Basisvorwiderstand,
  R12 10 k Basis-Pulldown, R15 2k2 parallel zum Piezo als Entladepfad. LEDC-PWM mit
  3,25 kHz (Resonanz), Lautstärke = Duty 0–50 %.
- **Lüfter**: PWM-Leitung (J6 Pin 4) mit R13 10 k auf +3V3 hochgezogen, Q2 zieht sie
  nach GND. Dadurch **invertiert**: Gate HIGH = PWM LOW = langsam. R11 10 k zieht das Gate
  auf +3V3 → der Lüfter läuft beim Booten auf Minimaldrehzahl. LEDC-PWM 25 kHz. Die
  Invertierung `255 - fanDuty` im `FanManager` ist diese Hardware und muss bleiben.
- **Potis**: 3 × 47 k, Schleifer jeweils über 1 k (R3/R4/R5) an den ADC. Regulär zwischen
  GND und 3V3 verdrahtet — **Aufdrehen liefert den höheren ADC-Wert**. Die Drehrichtung
  steht in der Firmware als einziges Flag in `Poti::ASCENDING` (`Empfaenger/Config.h`) —
  nie einzeln in den Kennlinien invertieren.
- Messpunkte: TP1 +3V3, TP2 VPD (CH224K-Versorgung), TP3 +5V.

### 2.5 ESP32-C3-Strapping-Fixes (im Schaltplan umgesetzt)

- **GPIO2** (Lautstärke-Poti): Fußpunkt des Potis an D4/GPIO6 (`POTI_GND`) statt GND —
  die Firmware legt D4 in `setup()` früh auf OUTPUT LOW.
- **GPIO9** (Status-LED): aktiv LOW verdrahtet (3V3 → LED → R9 → Pin), damit der Pin beim
  Boot nicht nach unten geklemmt wird.
- **GPIO8**: frei (kein Tacho angeschlossen).

### 2.6 Pinbelegung XIAO ESP32C3

| Funktion | XIAO-Pin | GPIO | Net |
|---|---|---|---|
| Poti Lautstärke (J3, Fußpunkt an D4!) | D0 | 2 | über R3 |
| Poti Helligkeit (J4) | D1 | 3 | über R4 |
| Poti Lüfter-Drehzahl (J2) | D2 | 4 | über R5 |
| Piezo (J8, über Q3 BC337) | D3 | 5 | `SIG_BUZ` |
| POTI_GND (geschalteter Fußpunkt) | D4 | 6 | `GND_POTI` |
| Lüfter-PWM (J6 Pin 4, über Q2) | D6 | 21 | `SIG_FAN` |
| Debug-Taster (J5, aktiv LOW) | D7 | 20 | `BTN` |
| Status-LED D3 (**aktiv LOW**) | D9 | 9 | über R9 |
| WS2811 Data (J7, über U5) | D10 | 10 | `SIG_LED` |
| frei | D5, D8 | 7, 8 | — |

### 2.7 Platine

- 2 Lagen, **61 × 43 mm**, 3 Befestigungslöcher M2
- Gemischte Bestückung: SMD 0603/1206, THT für XIAO, L7805, TSR0.5, BC337, 47-µF-Elko, LEDs
- Weitere Dateien im Ordner: `Empfaenger-Lochraster.diy` / `Lochraster.png`
  (Lochraster-Layout, DIY Layout Creator), `Regler-Aufkleber.svg` (Beschriftung der
  drei Regler), `Empfaenger.csv` (BOM-Export, siehe 4.1)

---

## 3. Kosten (grobe Schätzung, Stand 2026-10-26)

Einzelstückpreise in Euro. **Fett** = tatsächlich bezahlt, Rest geschätzt
(LCSC/Mouser/Reichelt bzw. AliExpress). Kabel, Schrauben und Versand sind nicht enthalten.

### 3.1 Sender: ca. 55–80 €

| Posten | € |
|---|---|
| Platine | **20** |
| Antenne mit Kabel (U.FL-SMA-Pigtail) | 10–20 |
| e-Paper-Panel 1.54″ V2 | 6–12 |
| ESP32-S3-WROOM-1U-N16R8 | 4–6 |
| Akku 14500 | 3–6 |
| Restliche Bauteile (TPS62742, MCP73837, USB-C, Taster, FPC-Stecker, Kleinteile) | 12–17 |

### 3.2 Empfänger: ca. 105–130 €

| Posten | € |
|---|---|
| WS2811-Strip 12 V, 66 Pixel | **45** |
| Platine | **20** |
| USB-C-PD-Netzteil 12 V / ≥ 2 A | 12–25 |
| Lüfter 12 V, 4-Pin-PWM | 5–12 |
| XIAO ESP32C3 | 5–7 |
| TRACO TSR0.5-2433 | 4–6 |
| Restliche Bauteile (Potis, Piezo, JST-XH-Stecker, Kleinteile) | 10–15 |

### 3.3 Gehäuse: 25 €

| Posten | € |
|---|---|
| Filament für den 3D-Druck, 2 kg | **25** |

### 3.4 Gesamt: ca. 185–235 €

Platinen, LED-Strip und Filament machen zusammen 110 € und damit gut die Hälfte aus.
Die Bauteile auf den Platinen (ICs, Widerstände, Kondensatoren) liegen zusammen nur bei
etwa 30–40 €. Bei Einzelstücken fallen zusätzlich Mindestmengen und Versand pro Shop
(5–20 €) ins Gewicht.

---

## 4. Offene Punkte / Auffälligkeiten in den KiCad-Daten

### 4.1 MOSFET-Typen im Empfänger uneinheitlich

Im Schaltplan stehen im **Value**-Feld Q1 = **AO3401A** und Q2 = **BSS138**. Auf den
**alten** Typen stehen weiterhin: die `MPN`-Eigenschaft im `.kicad_sch` (Q2: `2N7002`),
die Felder im `.kicad_pcb` (IRLML9301 / 2N7002) und der BOM-Export `Empfaenger.csv`.
**Wer nach BOM oder MPN bestellt, bekommt IRLML9301/2N7002.** Vorgehen: klären, was
physisch bestückt ist, dann in KiCad die MPN nachziehen, „Update PCB from Schematic" und
die BOM neu exportieren.

Nicht verwechseln: Im **Sender** sind Q1 = IRLML9301 und Q2 = IRLML6346 korrekt.

### 4.2 Sender: e-Paper-Versorgung VCI ohne Anschluss an eine Versorgung

Das Netz `VCI` (FPC J1 Pin 15 und 16) ist in Schaltplan und Layout **nur mit C9** (1 µF)
verbunden — weder mit `+3V3` noch mit `+3V3_LOAD`. Bei einem SSD1681-Panel ist VCI der
Versorgungseingang. Da das Display im Betrieb funktioniert, ist die Verbindung am
gebauten Gerät vermutlich von Hand nachgerüstet. Vor einer Neubestellung der Platine
prüfen und im Schaltplan nachziehen (naheliegend: an `+3V3_LOAD`, damit das Panel mit der
LOAD-Rail abgeschaltet wird).

### 4.3 Sender: Pegel an BTN1 bei vollem Akku

BTN1 liegt über den Teiler R2 47 k / R7 240 k an +BATT. Bei einer voll geladenen Zelle
(4,2 V) ergeben sich am GPIO15 rund **3,5 V** — über VDD (3,3 V), aber noch unter dem
Absolute-Maximum-Wert des ESP32-S3 (VDD + 0,3 V). Funktioniert, hat aber wenig Reserve.
