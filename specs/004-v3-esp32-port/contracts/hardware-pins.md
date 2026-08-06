# Contract: Pin-Belegung V3 (verbindlich für Config.h beider Firmwares)

**Feature**: 004-v3-esp32-port | **Quelle**: KiCad-Netzlisten `Schaltung-Sender/` bzw. `Schaltung-Empfaenger/`
(`BogenampelV3.kicad_sch` Rev 3.0, `Empfaenger.kicad_sch` — vormals `Zusatzplatine-Empfänger.kicad_sch`), extrahiert per
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
| C_ST1 (Lader STAT1) | 11 | IN | MCP73837 Open-Drain → `INPUT_PULLUP`; Decode nur zusammen mit ST2/PG sinnvoll (Table 5-1) |
| C_ST2 (Lader STAT2) | 12 | IN | MCP73837 Open-Drain → `INPUT_PULLUP`; **Hi-Z/Hi-Z/L ist mehrdeutig** — Standby, Temperature Fault und Timer Fault sind nicht unterscheidbar (Befund 2) |
| C_PG (Lader Power Good) | 17 | IN | MCP73837 Open-Drain → `INPUT_PULLUP`; L = gültige Eingangsspannung liegt an (sagt **nichts** darüber, ob geladen wird) |
| THERM (J3, kein GPIO) | — | — | IC3 Pin 9 an J3 Pin 2, J3 Pin 1 = GND. **Muss bestückt sein**: NTC oder ersatzweise 10 kΩ — offen ⇒ Temperature Fault, es wird nicht geladen (Befund 2) |
| C_PRG (Ladestrom) | 18 | IN (hochohmig) | direkt an PROG2, **R20 = 10 kΩ Pulldown** (nicht 100 k, Befund 3) → PROG2 Low = 80–100 mA USB-Ladestrom. ⚠ **Muss INPUT bleiben** — OUTPUT HIGH schaltet den Lader ab statt schneller zu laden (Befund 4) |
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

2. ✅ **THERM (IC3 Pin 9) MUSS beschaltet sein — sonst lädt der MCP73837 überhaupt nicht**
   (Befund 2026-08-01, im Schaltplan als J3 „NTC" vorgesehen, Bestückung nachgeholt).
   J3 ist ein PinHeader 1×02 (Pin 1 = GND, Pin 2 = THERM) für einen Akku-NTC. Bleibt er
   **offen**, sieht der Lader eine Temperatur außerhalb des zulässigen Fensters und geht in
   Temperature Fault: kein Ladestrom, aber PG weiterhin aktiv. Wird kein NTC verwendet, gehört
   laut Datenblatt DS20002071C ein **10 kΩ von THERM nach VSS** — genau das ist jetzt auf J3
   gesteckt (gemessen: 10 kΩ zwischen Pin 1 und 2).

   Diagnostisch heimtückisch: Table 5-1 gibt für Temperature Fault, Timer Fault **und**
   Standby dieselbe Signatur STAT1=Hi-Z / STAT2=Hi-Z / PG=L aus. Über die Statuspins allein
   ist „lädt nicht wegen fehlendem NTC" nicht von „Akku voll" zu unterscheiden — die Firmware
   fasst diese drei Zustände deshalb als `ChargeState::SUSPENDED` zusammen und zeigt
   „laedt nicht!" statt wie früher fälschlich „voll" (siehe `PowerManager::readChargerStatus()`).

3. ✅ **R20 = 10 kΩ, nicht 100 kΩ (funktionskritisch)** — geändert 2026-08-01.
   R20 zieht PROG2 (IC3 Pin 7) auf GND und wählt damit den USB-Ladestrom. Der Pin hat laut
   Datenblatt einen Leckstrom von typ. 7 µA / **max. 15 µA**. An 100 kΩ erzeugt das bis zu
   **1,5 V** — und damit einen Pegel im Shutdown-Fenster V_SD (0,2·VDD … 0,8·VDD), in dem der
   Lader abschaltet statt zu laden. Mit 10 kΩ bleiben max. 150 mV, sicher unter V_IL
   (0,2·VDD = 660 mV). Gemessen nach dem Tausch: **59 mV**.

   Beim nächsten Redesign nicht „aufrunden": Der Wert ist nicht als Pulldown-Stärke gewählt,
   sondern als Spannungsabfall-Grenze gegen den Leckstrom.

   PROG1 (IC3 Pin 6, R19 = 2 kΩ) setzt den Ladestrom für den **AC-Adapter-Eingang** (VAC,
   Pin 1). VAC ist auf dieser Platine **nicht angeschlossen** — PROG1 ist damit funktionslos,
   und ~0 V dort ist normal, kein Fehler.

4. ⚠ **Schnellladen per GPIO18 ist NICHT möglich — der Pin muss INPUT bleiben**
   (korrigiert 2026-08-01; frühere Fassungen dieses Dokuments, `PowerManager.h` und das
   README behaupteten das Gegenteil).

   Naheliegende, aber falsche Annahme: GPIO18 als OUTPUT HIGH überstimmt den 10-kΩ-Pulldown,
   PROG2 wird High und wählt statt „1 Unit Load" (100 mA) die „5 Unit Loads" (500 mA).
   Elektrisch überstimmt der Ausgang den Pulldown tatsächlich — nur ist der erreichte
   **Pegel zu niedrig**.

   Die PROG2-Schwellen sind relativ zur Versorgung definiert, und das Datenblatt legt fest:
   *„The supply voltage (VDD) = VAC when input power source is from AC adapter and the supply
   voltage (VDD) = VUSB [when from the USB port]."* Da VAC hier unbeschaltet ist, gilt
   **VDD = VUSB = 5 V** (gemessen an IC3 Pin 2).

   | Grenze | Formel | bei VDD = 5 V |
   |---|---|---|
   | V_IH (High) | ≥ 0,8 · VDD | **≥ 4,0 V** |
   | V_IL (Low) | ≤ 0,2 · VDD | ≤ 1,0 V |
   | V_SD (Shutdown) | 0,2 · VDD … 0,8 · VDD | 1,0 … 4,0 V |

   Ein ESP32-GPIO liefert 3,3 V. Das erreicht V_IH nicht und landet **im Shutdown-Fenster** —
   der Lader schaltet ab, statt schneller zu laden. Es ist derselbe Mechanismus, der den
   Lader schon beim ursprünglichen 100-kΩ-R20 stillgelegt hat (Befund 3), nur von der
   anderen Seite angefahren.

   Schnellladen erfordert daher eine **Hardware**-Änderung:
   - **einfach, fest**: R20 als Pull-up nach VUSB statt Pulldown nach GND → PROG2 dauerhaft
     5 V = 500 mA. Dann aber die Leiterbahn zu GPIO18 auftrennen, sonst sieht der ESP32-Pin
     5 V (absolutes Maximum 3,6 V; über 100 kΩ wären es ~11 µA durch die Schutzdiode —
     tolerierbar, aber unsauber). Nicht mehr umschaltbar.
   - **umschaltbar**: Pull-up nach VUSB plus N-MOSFET, den GPIO18 ansteuert und der PROG2
     bei Bedarf auf GND zieht. Default 500 mA, Firmware kann drosseln, der GPIO sieht nie
     mehr als 3,3 V. Kostet ein Bauteil im nächsten Layout.

## Empfänger — Seeed XIAO ESP32C3 (U3, PD-12V-Platine Rev. 2026-08-03)

> **Stand 2026-08-05**: Referenzbezeichner und Versorgung stammen aus der gefertigten
> PD-12V-Platine (`Empfaenger.kicad_pcb`/`.kicad_sch`, Commit `cdde8dc`) und lösen die
> Angaben des Lochraster-/USB-5V-Aufbaus ab. **Die Pin-Funktionen sind unverändert** —
> geändert haben sich nur Bauteilnummern, Versorgungstopologie und der neue Pegelwandler.

| Funktion | XIAO-Pin | GPIO | Richtung | Beschaltung / Hinweise |
|----------|----------|------|----------|------------------------|
| Poti Lautstärke (J3) | D0 | 2 | IN (ADC1) | Schleifer über R3 1k; Fußpunkt an D4 (POTI_GND) — Strapping-Fix Befund 2 |
| **Poti Helligkeit (J4)** | **D1** | **3** | IN (ADC1) | **Umverdrahtung von D5!** (GPIO7 hat keinen ADC; GPIO5/D3 wäre ADC2 = mit Funk unbrauchbar); über R4 1k |
| Piezo (J8, 12-V-Transducer) | **D3** | **5** | OUT (LEDC) | über R10 2k2 → BC337 (Q3), R12 10k Basis-Pulldown (stumm beim Boot); R15 2k2 vom Collector nach +12 V als Entlade-Pfad (vorher 470 Ω — falls zu leise, verkleinern). GPIO5 ist ADC2, wird aber rein digital genutzt |
| **Poti Lüfter-Drehzahl (J2)** | **D2** | **4** | IN (ADC1) | Schleifer über R5 1k, stellt die Lüfter-PWM (D6) ein; ADC1_CH4, kein Strapping-Pin |
| Lüfter PWM (J6 Pin 4) | D6 | 21 | OUT (LEDC) | direkt am Gate des 2N7002 (Q2, **invertiert**: Gate HIGH = PWM-Leitung LOW = langsam); R11 10k = Gate-Pull-up an 3V3 → Leitung beim Boot LOW (Minimaldrehzahl), R13 10k = Pull-up der PWM-Leitung an 3V3. J6 Pin 3 (Tacho) unbeschaltet |
| Taster (J5) | D7 | 20 | IN | gegen GND → `INPUT_PULLUP`, aktiv LOW (Debug-/Testtaster) |
| Status-LED (D3) | D9 | 9 | OUT | **aktiv LOW**: 3V3 → LED → R9 220 Ω → Pin (sinkt Strom) — Strapping-Fix Befund 3 |
| WS2811 Data (J7) | D10 | 10 | OUT | 66 Pixel, 12-V-Strip. **Neu**: über U5 (74AHCT1G125, 3,3 → 5 V, /OE fest an GND) → R14 330 Ω → J7 Pin 2. U5-Eingang ohne Pulldown → GPIO10 in `setup()` früh OUTPUT LOW |
| POTI_GND | D4 | 6 | OUT | geschalteter Fußpunkt des Lautstärke-Potis J3 Pin 3 (Befund 2); in `setup()` OUTPUT LOW |
| frei | D5, D8 | 7, 8 | — | Reserve (D5 nach Helligkeits-Poti-Umzug frei; D8 nach Tacho-Verzicht frei) |

Versorgung (PD-12V-Platine): USB-C (J1) → CH224K (U2) verhandelt **12 V** → Verpolungsschutz
Q1 (IRLML9301) + TVS D2 (SMBJ13A) → +12V-Netz. Daraus:
- **TSR0.5-2433 (U4)** → 3V3, speist den XIAO über den **3V3-Pin** (VUSB und Batt-Pads unbeschaltet)
- **L7805 (U1)** → 5 V, versorgt ausschließlich den Pegelwandler U5
- **+12 V direkt** an LED-Strip (J7), Piezo (J8) und Lüfter (J6)

Damit entfällt der frühere 5V→12V-Step-up samt Jumpern JP1/JP2. Der Strip belastet die
Logikversorgung nicht mehr → `LEDStrip::BRIGHTNESS_MAX` steht auf dem Design-Wert 255
(PD-Netzteil mit 12 V / ≥ 2 A vorsehen: ~1,3 A einfarbig bei 66 Pixeln, ~4 A bei Weiß).

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
5. ✅ **J1 — auf der PD-Platine 6-polig (USB-C-Buchse)**: VBUS / CC1 / CC2 / D− / D+ / GND
   an den CH224K (U2). Der alte 2-polige „5V USB"-Eingang und die Jumper JP1/JP2 sind
   entfallen; die 5-V-Schiene existiert nur noch als L7805-Ausgang für den Pegelwandler U5.
6. ℹ **Lüfter-Default**: R11 ist Gate-Pull-up an 3V3 → Q2 leitet, die PWM-Leitung liegt beim
   Boot/Flashen LOW, der Lüfter läuft also auf **Minimaldrehzahl**, bis die Firmware D6
   übernimmt (`fan.begin()` steht deshalb früh in `setup()`). Funktional unkritisch.

### Hinweis Alt-Belegung V2 (zur Abgrenzung)

V2 nutzte: LED-Strip D3, Buzzer D4, 3 Status-LEDs A2–A4, Debug-Jumper D2, Poti A7, NRF24 D8/D9.
Diese Belegung gilt für V3 NICHT mehr; `EmpfaengerV3/Config.h` wird ausschließlich aus der
obigen Tabelle befüllt.
