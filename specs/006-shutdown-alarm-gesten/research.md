# Phase 0 — Research: Entkopplung von Ausschalten und Alarm

**Feature**: 006-shutdown-alarm-gesten
**Datum**: 2026-08-02
**Status**: abgeschlossen — keine offenen NEEDS CLARIFICATION

## R-1: Tastendrücke gehen während des e-Paper-Refreshs verloren

**Problem**: `ButtonManager::update()` pollt `digitalRead()` und wird nur aus
`loopNormal()` aufgerufen. Ein Partial-Refresh des e-Papers blockiert laut
`EpaperDisplay.h` **300–400 ms**, ein Voll-Refresh rund 2 s. Im Schießbetrieb
läuft der 1-Hz-Countdown, d. h. das Gerät steht **etwa ein Drittel der Zeit** in
einem blockierenden Refresh. Ein Tastendruck, der vollständig in dieses Fenster
fällt (Drücken + Loslassen in < 300 ms — genau das Tempo eines Dreifachklicks),
wird nie gesehen.

Für die alte Alarm-Geste (2 s halten) war das egal: nach dem Refresh war der
Taster immer noch gedrückt. Für den Dreifachklick mit 400 ms Klickabstand ist es
tödlich — die sicherheitskritische Geste würde unzuverlässig (Constitution I:
„Alle Zustandsübergänge MÜSSEN vorhersehbar und deterministisch sein").

**Decision**: `GxEPD2_EPD::setBusyCallback()` nutzen. GxEPD2 ruft den
registrierten Callback in `_waitWhileBusy()` kontinuierlich auf; dort wird
`ButtonManager::update()` aufgerufen. Damit läuft die Tastenabfrage auch
während jedes Refreshs weiter — der blockierende Anteil ist fast vollständig
BUSY-Warten, die reine SPI-Übertragung (5 000 Byte) liegt im einstelligen
Millisekundenbereich.

Verifiziert in `.pio/libdeps/sender/GxEPD2/src/GxEPD2_EPD.h:96`:

```cpp
// register a callback function to be called during _waitWhileBusy continuously.
void setBusyCallback(void (*busyCallback)(const void*), const void* busy_callback_parameter = 0);
```

**Rationale**:
- ~10 Zeilen Code (Passthrough in `EpaperDisplay` + statische Callback-Funktion
  in `Sender.cpp`), keine Interrupts, keine IRAM-Attribute, keine
  volatile/ISR-Sicherheitsfragen → Constitution II (Einfachheit).
- Reentranz unkritisch: Der Callback läuft, während die Hauptschleife im
  Refresh steht; `ButtonManager::update()` liest nur GPIOs und setzt Flags. Die
  StateMachine wird nicht reentrant aufgerufen.
- Nutzt gleichzeitig der Ausschalt-Geste (Halte-Erkennung tickt weiter) und dem
  Überspringen des Splash-Screens.

**Alternatives considered**:
- **GPIO-Interrupts mit Zeitstempel-Ringpuffer**: robust gegen *jede* blockierende
  Stelle (auch `sendAlarmWithRetry()`, 600 ms). Verworfen: deutlich mehr
  Komplexität (ISR-Sicherheit, `IRAM_ATTR`, volatile Ringpuffer) für einen
  Gewinn, der nur Codepfade betrifft, in denen die Alarmfolge ohnehin nicht mehr
  ausgewertet wird (der Alarm ist dann schon ausgelöst). Bleibt der Fallback,
  falls der HIL-Test T-05 die Klickerkennung als unzuverlässig zeigt.
- **Refresh in Häppchen / nicht-blockierendes BUSY-Warten**: Umbau des gesamten
  Display-Layers, weit außerhalb des Feature-Scopes.
- **Klickabstand auf 800 ms aufmachen**: würde die Folge von normaler Bedienung
  nicht mehr unterscheidbar machen (widerspricht SC-004).

**Zwei Restlücken, bewusst offen gelassen** (in T-23 zu beobachten):

1. **Entprellung als Untergrenze**: `DEBOUNCE_MS` = 80 ms. Eine Betätigung, die
   kürzer als 80 ms Kontakt hat, wird gar nicht erst als Flanke registriert —
   das war schon vor diesem Feature so, fiel aber nicht auf, weil niemand
   schnell klicken musste. Ein sehr hektischer Dreifachklick kann damit einzelne
   Klicks verlieren. Bewusst **nicht** angefasst: 80 ms ist der erprobte V2-Wert,
   und ein Senken erhöht das Prellrisiko an allen anderen Stellen. Fällt SC-001a
   (9 von 10 Auslösungen) durch, ist das die zweite Stellschraube nach
   `MULTI_CLICK_GAP_MS`.
2. **Blockierende Funkaufrufe**: `RadioManager::sendCommand()` wartet bis zu
   100 ms je Sendeversuch (max. 3 Versuche + Retry-Delays). Steht ein Empfänger
   zur Verfügung, dauert das ~10 ms und ist irrelevant; fehlt er, kann ein
   Zustandswechsel ~450 ms ohne Tastenabfrage blockieren und einen Klick der
   Folge schlucken. Ohne Empfänger gäbe es aber ohnehin niemanden zu alarmieren,
   deshalb kein eigener Callback-Haken im `RadioManager`.

## R-2: Der auslösende Klick darf nicht doppelt wirken

**Problem**: Der dritte Klick der Folge löst den Alarm aus — und ist gleichzeitig
ein ganz normaler Klick. In `STATE_ALARM` quittiert ein OK-Klick den Alarm
sofort wieder (`handleAlarm()` → `CMD_STOP` → Pfeile holen). Ohne Gegenmaßnahme
wäre der Alarm nach wenigen Millisekunden von selbst wieder weg.

Die Rollen feuern zudem unterschiedlich: CONFIG setzt `clickedFlag` beim
**Drücken**, OK beim **Loslassen**.

**Decision**: Die Folge wird **auf der Loslass-Flanke** abgeschlossen, und die
auslösende Betätigung erzeugt dabei keinen Klick:
- Erreicht der Zähler beim Drücken die Schwelle, wird die Betätigung nur
  `multiClickArmed` markiert. Für die Rolle CONFIG (die sonst beim Drücken
  klickt) wird das `clickedFlag` dabei **zurückgehalten**.
- Beim Loslassen entscheidet die Haltedauer: kürzer als
  `MULTI_CLICK_GAP_MS` → Alarm ohne Klick; länger → die Folge verfällt und der
  zurückgehaltene CONFIG-Klick wird nachgeliefert.

**Nachtrag aus der Umsetzung (2026-08-02)**: Ursprünglich war das Auslösen auf
der Drückflanke geplant. Das kollidiert mit dem Edge Case „nach zwei Klicks beim
dritten Mal halten" (FR-011): Auf der Drückflanke steht der Alarm bereits fest,
bevor sich zeigt, ob die Betätigung ein Klick oder der Beginn eines Haltens ist
— das Gerät hätte alarmiert und danach abgeschaltet. Kosten der Umstellung: der
Alarm feuert rund 50–100 ms später (beim Loslassen des dritten Klicks), was
gegen SC-001 (< 1,5 s) unkritisch ist.

**Rationale**: Erfüllt FR-013 („Endzustand nach erkannter Folge MUSS Alarm
sein") an der einzigen Stelle, an der beide Rollen zusammenlaufen. Die
Alternative — die StateMachine räumt nach `setState(STATE_ALARM)` die Flags auf —
verteilt die Verantwortung über zwei Klassen und bricht, sobald ein weiterer
Zustand die Folge auswertet.

**Alternatives considered**: Klicks generell erst nach Ablauf des
Mehrfachklick-Fensters ausführen (800 ms Verzögerung auf *jeden* Klick) —
verworfen, weil das e-Paper ohnehin träge ist und die Bedienung dann als defekt
wahrgenommen würde.

**Zweiter Nachtrag aus dem Feldtest (2026-08-02)**: Den auslösenden Klick zu
unterdrücken reicht nicht. Beobachtet wurde: Der Alarmbildschirm erschien kurz
und das Gerät fiel sofort nach „Pfeile holen" zurück. Ursache sind die
**vorangegangenen** Klicks der Folge:

1. Klick 1 (OK) beendet im Schießbetrieb die Passe → `enterPfeileHolen()`
   blockiert mit Refresh und Gruppen-Kommando.
2. Klick 2 setzt währenddessen ein Klick-Ereignis, das niemand abholt — die
   StateMachine läuft ja nicht, nur der Busy-Callback pollt.
3. Klick 3 löst den Alarm aus. In `STATE_ALARM` quittiert der erste OK-Klick,
   und das ist genau das liegengebliebene Ereignis aus Schritt 2.

Dasselbe passiert ein zweites Mal *innerhalb* von `enterAlarm()`: Senden mit
Retries (bis 600 ms) plus Refresh (~500 ms) blockieren rund eine Sekunde, in der
weitere Klicks eingesammelt werden.

**Fix**: `ButtonManager::discardPendingClicks()` verwirft die Klick-Ereignisse
beider Taster. Aufgerufen wird sie an genau zwei Stellen — beim Auslösen der
Folge und am Ende von `enterAlarm()`. Alles, was während des Auslösens getippt
wurde, gehört zur Geste und nicht zur Quittierung.

## R-3: Zähler über Zustandswechsel hinweg

**Problem**: FR-012 verlangt, dass eine begonnene Folge den Wechsel
Schießbetrieb → Pfeile holen überlebt (der erste OK-Klick beendet die Passe).
FR-011 verlangt gleichzeitig, dass der Zähler beim Wechsel in einen nicht
alarmfähigen Zustand verworfen wird — sonst löst ein im Konfigurationsmenü
aufgebauter Zählerstand später im Schießbetrieb einen Fehlalarm aus.

**Decision**: Genau eine Regel in `StateMachine::setState()`:

```text
alarmfähig := { STATE_SCHIESS_BETRIEB, STATE_PFEILE_HOLEN }
wenn NICHT (alter Zustand alarmfähig UND neuer Zustand alarmfähig)
    → buttons.resetMultiClick()
```

**Rationale**: Eine einzige Stelle, deterministisch, deckt FR-011 und FR-012
zusammen ab. Insbesondere setzt der Eintritt in `STATE_ALARM` den Zähler zurück
(Alarm ist nicht alarmfähig) — damit ist FR-015 („erneute Folge im Alarm
wirkungslos") ohne Sonderfall erfüllt, und beim Quittieren nach „Pfeile holen"
startet die Zählung ebenfalls sauber bei null.

**Alternatives considered**: Zähler nur per Zeitfenster verfallen lassen —
verworfen, weil ein Zählerstand von 2 aus dem Konfigurationsmenü dann bis zu
400 ms in den Schießbetrieb hineinreicht.

## R-4: Klick-Schwelle beim Loslassen von OK

**Problem**: `ButtonManager::update()` erzeugt den OK-Klick beim Loslassen nur,
wenn die Betätigung kürzer als `ALARM_THRESHOLD_MS` (2 s) war. Diese Konstante
entfällt mit FR-016. Ohne Ersatz würde jede Betätigung als Klick gelten — auch
die abgebrochene Ausschalt-Geste.

**Decision**: Schwelle auf `POWER_OFF_HOLD_MS` (3 s) umstellen.

**Rationale**: Deckt sich exakt mit dem geforderten Verhalten (Spec, US2
Szenario 3: „kürzer als 3 s gehalten → wirkt als normaler Klick"), und es bleibt
bei genau einer Schwelle pro Taster. `ALARM_THRESHOLD_MS` wird ersatzlos
gelöscht, damit kein toter Wert zurückbleibt (Constitution: Magic Numbers /
Wartbarkeit).

## R-5: 60-Minuten-Abschaltung vs. Batterielaufzeit

**Problem**: Constitution fordert ≥ 8 h Batterielaufzeit; die Verdreifachung der
Idle-Frist erhöht den Verbrauch im Fall „vergessen eingeschaltet".

**Decision**: `IDLE_POWER_OFF_MS` von 20 auf 60 Minuten anheben, DEBUG-Wert
(`DEBUG_SHORT_TIMES` → 60 s) unverändert lassen.

**Rationale**: Der gemessene Betriebsstrom liegt bei 0,20 W (≈ 54 mA bei 3,7 V).
Die zusätzlichen 40 Minuten Nachlauf kosten rund **36 mAh** — bei einem
LiPo-Akku in der verbauten Größenordnung deutlich unter 5 % der Kapazität und
ohne Einfluss auf die geforderten 8 h *aktiver* Nutzung. Der Zweck der
Abschaltung („im Koffer vergessenes Gerät", Kommentar in `Config.h`) bleibt
erhalten. Der DEBUG-Wert bleibt bei 60 s, sonst wird der HIL-Test unbrauchbar.

**Alternatives considered**: 45 Minuten als Kompromiss — verworfen, weil die
Vorgabe „eine Stunde" explizit ist und Mittagspausen bei Turnieren regelmäßig
länger als 45 Minuten dauern.

## R-6: Ausschalten im Schießbetrieb — Sicherheitsbetrachtung

**Problem**: FR-002 macht die Ausschalt-Geste erstmals auch im laufenden
Schießbetrieb verfügbar. Constitution I verlangt einen sicheren Zustand bei
Kommunikationsausfall.

**Decision**: Keine zusätzliche Absicherung, kein Rückfragedialog. Der Sender
schaltet ab, der Empfänger führt die laufende Passe autonom zu Ende (FR-004 der
V3-Spec, im Empfänger unverändert implementiert).

**Rationale**: Das ist genau der Fall, für den das autonome Passenende gebaut
wurde — ein abgeschalteter Sender ist funktional identisch zu einem Sender außer
Funkreichweite. Ein Bestätigungsdialog widerspräche der Ein-Knopf-Bedienung
(Constitution II) und würde 3 s Halten + Bestätigung erfordern. Zusätzlich ist
die Fehlbedienung unwahrscheinlich: 3 s ununterbrochenes Halten passiert nicht
versehentlich.

## R-7: Keine Hardware-Änderung

Geprüft gegen `specs/004-v3-esp32-port/contracts/hardware-pins.md` und die
KiCad-Quelle `Schaltung-Sender/`: Das Feature ändert **keine** Pin-Belegung,
keine Beschaltung und keine Bauteilwerte. Beide Taster sind bereits als
Eingänge mit definierten Pegeln vorhanden (BTN1/GPIO15 aktiv HIGH über Teiler
R2/R7, BTN2/GPIO9 aktiv LOW mit internem Pullup). Constitution III/V
(„Änderungen an Pin-Belegungen zuerst im KiCad") ist damit nicht berührt.

## R-8: Empfänger bleibt unberührt

Das Funkprotokoll (`Commands.h`, byte-identisch auf beiden Seiten) und das
`CMD_ALARM`-Verhalten des Empfängers bleiben unverändert. Geändert wird
ausschließlich, **wodurch** der Sender `CMD_ALARM` auslöst. `Empfaenger/` wird in
diesem Feature nicht angefasst — auch nicht neu geflasht.
