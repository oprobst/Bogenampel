# Feature Specification: Entkopplung von Ausschalten und Alarm

**Feature Branch**: `006-shutdown-alarm-gesten`
**Created**: 2026-08-02
**Status**: Draft
**Input**: User description: "Aktuell liegen shutdown und alarm auf der selben Nutzerinteraktion. Künftig soll folgendes gelten: Bei einem langen Tastendruck (> 3sek), egal welche Taste, soll das Gerät herunterfahren. Wenn eine Taste sehr schnell hintereinander mehr als drei mal gedrückt wird, soll der Alarm Modus gelten. Weiterhin soll der timeout shutdown erst nach einer Stunde erfolgen."

## Ausgangslage (Problem)

An der Bedieneinheit liegen Ausschalten und Not-Alarm heute auf derselben
Nutzerinteraktion — dem Halten einer Taste. Unterschieden wird allein über den
Betriebszustand: im Schießbetrieb löst Halten den Alarm aus, in allen anderen
Zuständen schaltet Halten das Gerät ab. Daraus folgen drei Probleme:

1. **Nicht vorhersagbar**: Dieselbe Handbewegung hat je nach Bildschirm eine
   völlig andere Wirkung. Wer die Ampel abschalten will, muss wissen, in welchem
   Zustand das Gerät gerade steht.
2. **Fehlbedienung mit Folgen**: Wer im Schießbetrieb ausschalten will, löst
   stattdessen einen Alarm für die ganze Schießlinie aus — umgekehrt kann im
   Schießbetrieb gar nicht ausgeschaltet werden.
3. **Zu kurze Abschaltzeit**: Die automatische Abschaltung nach Inaktivität
   greift bereits nach 20 Minuten. Bei Turnieren mit längeren Pausen
   (Scheibenwechsel, Wertung, Mittagspause) schaltet sich die Bedieneinheit ab,
   während der Betrieb noch läuft.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Not-Alarm zuverlässig auslösen (Priority: P1)

Ein Schütze betritt unerwartet die Schießlinie oder es kommt zu einem Zwischen-
fall. Der Bediener drückt die OK-Taste dreimal schnell hintereinander;
die Anlage geht sofort in den Alarmzustand und stoppt den laufenden Durchgang.
Das gilt in den Phasen, in denen Menschen an der Schießlinie sein können:
Schießbetrieb und „Pfeile holen".

**Why this priority**: Der Alarm ist die sicherheitsrelevante Funktion der
Anlage. Er muss unter Stress ohne Nachdenken und ohne Wartezeit auslösbar sein
und darf niemals versehentlich zum Ausschalten führen.

**Independent Test**: Vollständig testbar, indem im Schießbetrieb und in „Pfeile
holen" je die Alarmfolge auf beiden Tasten ausgeführt wird und geprüft wird,
dass die Anzeigeeinheit den Alarm meldet und die Bedieneinheit den
Alarmbildschirm zeigt — ohne dass das Gerät abschaltet; im Konfigurationsmenü
darf dieselbe Folge keinen Alarm auslösen.

**Acceptance Scenarios**:

1. **Given** das Gerät ist im Schießbetrieb, **When** der Bediener die OK-Taste
   dreimal mit höchstens 400 ms Abstand drückt, **Then** wechselt die
   Bedieneinheit in den Alarmzustand und die Anzeigeeinheit signalisiert Alarm.
1a. **Given** das Gerät ist im Schießbetrieb, **When** der Bediener die
   CONFIG-Taste dreimal schnell drückt, **Then** wird **kein** Alarm ausgelöst.
2. **Given** das Gerät ist im Zustand „Pfeile holen", **When** der Bediener die
   Alarmfolge ausführt, **Then** wechselt die Anlage ebenfalls in den
   Alarmzustand.
3. **Given** das Gerät ist im Schießbetrieb, **When** der Bediener eine Taste
   ununterbrochen hält, **Then** wird **kein** Alarm ausgelöst (das Halten ist
   jetzt ausschließlich die Ausschalt-Geste).
4. **Given** der Bediener drückt die Taste zwar dreimal, aber mit mehr als
   400 ms Abstand, **When** die Folge abgeschlossen ist, **Then** wird kein
   Alarm ausgelöst, sondern jeder Druck wirkt als normaler Klick.
5. **Given** das Gerät steht im Konfigurationsmenü, **When** der Bediener
   dreimal schnell klickt (zügiges Hochzählen eines Werts), **Then** wird kein
   Alarm ausgelöst und die Werte zählen normal weiter.
6. **Given** die Anlage steht im Alarmzustand, **When** der Bediener die
   Alarmfolge erneut ausführt, **Then** bleibt der Alarmzustand unverändert
   bestehen (keine Doppelauslösung, kein Zustandssprung).
7. **Given** der erste Klick der Folge hat im Schießbetrieb bereits die Passe
   beendet (Zustandswechsel nach „Pfeile holen"), **When** die beiden weiteren
   Klicks rechtzeitig folgen, **Then** wird der Alarm trotzdem ausgelöst — der
   Zählerstand überlebt den Zustandswechsel.

---

### User Story 2 - Gerät jederzeit vorhersagbar ausschalten (Priority: P2)

Der Bediener ist mit dem Schießen fertig oder will das Gerät verstauen. Er hält
eine beliebige Taste länger als drei Sekunden gedrückt; das Gerät verabschiedet
sich und schaltet ab — aus jedem Betriebszustand heraus, auch aus dem laufenden
Schießbetrieb und aus dem Alarmzustand.

**Why this priority**: Löst das eigentliche Bedienproblem (gleiche Geste, zwei
Bedeutungen), ist aber ohne die neue Alarm-Geste aus User Story 1 nicht sinnvoll
einführbar — das Halten muss erst frei werden.

**Independent Test**: Vollständig testbar, indem in jedem der fünf
Betriebszustände nacheinander jede Taste > 3 s gehalten und geprüft wird, dass
das Gerät jedes Mal abschaltet.

**Acceptance Scenarios**:

1. **Given** das Gerät ist eingeschaltet und in einem beliebigen
   Betriebszustand, **When** eine beliebige Taste länger als 3 s gehalten wird,
   **Then** zeigt die Bedieneinheit den Abschaltbildschirm und schaltet ab.
2. **Given** das Gerät ist im Schießbetrieb mit laufendem Countdown, **When**
   eine Taste > 3 s gehalten wird, **Then** schaltet das Gerät ab (bisher: Alarm
   statt Abschalten).
3. **Given** eine Taste wird kürzer als 3 s gehalten und dann losgelassen,
   **When** die Taste losgelassen wird, **Then** schaltet das Gerät nicht ab,
   sondern der Druck wirkt als normaler Klick.
4. **Given** das Gerät wird gerade über die Einschalt-Taste eingeschaltet und
   der Bediener hält diese länger als 3 s, **When** das Gerät hochfährt,
   **Then** schaltet es sich **nicht** sofort wieder ab.

---

### User Story 3 - Längere Pausen ohne Selbstabschaltung (Priority: P3)

Bei einem Turnier liegt zwischen zwei Passen eine längere Pause (Wertung,
Scheibenwechsel, Mittagspause). Die Bedieneinheit bleibt eingeschaltet und
betriebsbereit, statt sich nach 20 Minuten abzuschalten.

**Why this priority**: Reiner Komfortgewinn ohne Sicherheitsbezug; unabhängig
von den beiden Gesten umsetzbar und testbar.

**Independent Test**: Vollständig testbar, indem das Gerät in einem Wartezustand
ohne Bedienung stehen bleibt und geprüft wird, dass es nach 20 und nach 55
Minuten noch läuft und nach 60 Minuten abschaltet.

**Acceptance Scenarios**:

1. **Given** das Gerät steht unbedient in einem Wartezustand, **When** 59
   Minuten ohne Bedienung vergangen sind, **Then** ist das Gerät noch
   eingeschaltet und bedienbar.
2. **Given** das Gerät steht unbedient in einem Wartezustand, **When** 60
   Minuten ohne Bedienung vergangen sind, **Then** schaltet es mit dem Hinweis
   auf die automatische Abschaltung ab.
3. **Given** das Gerät steht 45 Minuten unbedient, **When** der Bediener eine
   Taste drückt, **Then** beginnt die Stunde von vorn.

---

### Edge Cases

- **Mehrfachklick trifft auf normale Aktionen**: Jeder einzelne Tastendruck der
  Alarmfolge wirkt zunächst als normaler Klick (Passe beenden, weiter …). Beim
  Erreichen der Alarm-Schwelle überschreibt der Alarmzustand alles zuvor
  Ausgelöste. Der Bediener darf am Ende der Folge in keinem anderen Zustand als
  „Alarm" landen.
- **Zustandswechsel mitten in der Folge**: Löst der erste Klick einen Wechsel
  zwischen zwei alarmfähigen Zuständen aus (Schießbetrieb → Pfeile holen), läuft
  die Zählung weiter. Wechselt das Gerät dagegen in einen nicht alarmfähigen
  Zustand (z. B. Konfigurationsmenü), wird die Folge verworfen.
- **Übergang Mehrfachklick → Halten**: Wird nach zwei schnellen Klicks die Taste
  beim dritten Mal gehalten, gilt allein die Halte-Geste — nach 3 s schaltet das
  Gerät ab, ein Alarm wird nicht ausgelöst.
- **Alarmfolge im Konfigurationsmenü**: Dort wird nicht gezählt; schnelles
  Hochzählen von Werten bleibt folgenlos. Ausschalten per Halten funktioniert
  dort weiterhin.
- **Beide Tasten gleichzeitig gehalten**: Das ist die Einstiegsgeste in den
  Wartungsmodus beim Einschalten. Im laufenden Betrieb löst gleichzeitiges
  Halten beider Tasten genau eine Abschaltung aus (nicht zwei konkurrierende).
- **Klicks auf zwei verschiedene Tasten**: Abwechselnde schnelle Klicks auf OK
  und CONFIG zählen nicht als Alarmfolge — gezählt wird ausschließlich OK. Drei
  OK-Klicks im Zeitfenster lösen den Alarm aber auch dann aus, wenn dazwischen
  CONFIG gedrückt wurde.
- **Einschalt-Druck**: Der Tastendruck, mit dem das Gerät eingeschaltet wird,
  zählt weder als Klick der Alarmfolge noch als Beginn der Halte-Geste.
- **Alarm während des Abschaltbildschirms**: Ist die Abschaltsequenz einmal
  gestartet, ist sie nicht mehr abbrechbar; weitere Tastendrücke bleiben ohne
  Wirkung.
- **Automatische Abschaltung im laufenden Schießbetrieb**: Während einer
  laufenden Passe und im Alarmzustand greift die Inaktivitäts-Abschaltung
  weiterhin nicht — nur in den Wartezuständen.
- **Alarm im Wartungsmodus**: Im Wartungsmodus (OTA) gibt es keinen
  Schießbetrieb und keine Alarmfolge; dort bleibt die Tastenbelegung wie bisher.

## Requirements *(mandatory)*

### Functional Requirements

**Ausschalten per langem Tastendruck**

- **FR-001**: Das System MUSS abschalten, wenn eine beliebige Taste länger als
  3 Sekunden ununterbrochen gedrückt gehalten wird.
- **FR-002**: Die Ausschalt-Geste MUSS in **allen** Betriebszuständen wirken —
  einschließlich laufendem Schießbetrieb und Alarmzustand.
- **FR-003**: Das System MUSS beide Tasten für die Ausschalt-Geste gleichwertig
  behandeln; die Wirkung darf nicht davon abhängen, welche Taste gehalten wird.
- **FR-004**: Das System MUSS die Ausschalt-Geste ignorieren, solange die
  gehaltene Taste seit dem Einschalten noch nicht einmal losgelassen wurde
  (Schutz vor sofortigem Wiederausschalten beim Einschalten).
- **FR-005**: Das System MUSS vor dem Abschalten einen Abschalthinweis anzeigen
  und die Anzeige geordnet stilllegen (unverändert zum heutigen Verhalten).
- **FR-006**: Das System MUSS die Abschaltung genau einmal ausführen, auch wenn
  die Geste auf beiden Tasten gleichzeitig erfüllt wird.

**Alarm per schneller Mehrfachbetätigung**

- **FR-007**: Das System MUSS den Alarmzustand auslösen, wenn dieselbe Taste
  dreimal so gedrückt wird, dass zwischen zwei aufeinanderfolgenden Klicks
  höchstens 400 ms liegen (Dreifachklick, Gesamtdauer damit höchstens 800 ms).
- **FR-008**: Das System MUSS ausschließlich OK-Klicks zur Folge zählen; ein
  zwischengeschobener CONFIG-Klick darf die Folge weder weiterzählen noch
  abbrechen.
- **FR-009**: Das System MUSS die Alarmfolge **ausschließlich** auf der OK-Taste
  auswerten. Ein Dreifachklick auf CONFIG DARF keinen Alarm auslösen —
  CONFIG blättert durch Menüs und zählt Werte hoch, dort ist zügiges Tippen
  normale Bedienung. *(Geändert am 2026-08-02 nach dem Feldtest; ursprünglich
  waren beide Tasten vorgesehen.)*
- **FR-010**: Das System MUSS die Alarmfolge ausschließlich in den Zuständen
  „Schießbetrieb" und „Pfeile holen" auswerten. In allen übrigen Zuständen
  (Startbildschirm, Konfigurationsmenü, Alarm, Wartungsmodus) MUSS die Folge
  wirkungslos bleiben — insbesondere darf zügiges Blättern oder Hochzählen im
  Konfigurationsmenü keinen Alarm auslösen.
- **FR-011**: Das System MUSS den Alarmzähler zurücksetzen, sobald das
  Zeitfenster überschritten wird, die Taste gehalten statt geklickt wird oder
  das Gerät in einen nicht alarmfähigen Zustand wechselt.
- **FR-012**: Das System MUSS eine begonnene Alarmfolge über einen Wechsel
  zwischen den beiden alarmfähigen Zuständen hinweg weiterzählen (Schießbetrieb
  → Pfeile holen durch den ersten Klick).
- **FR-013**: Das System MUSS beim Auslösen des Alarms den Alarmzustand über
  alle zuvor durch die Einzelklicks ausgelösten Zustandswechsel stellen; der
  Endzustand nach einer erkannten Alarmfolge MUSS „Alarm" sein.
- **FR-014**: Das System MUSS den Alarm wie bisher an die Anzeigeeinheit senden
  (inklusive der bestehenden Wiederholversuche) und das Ergebnis der Übertragung
  auf der Bedieneinheit anzeigen.
- **FR-015**: Das System MUSS eine erneut erkannte Alarmfolge im bereits
  stehenden Alarmzustand wirkungslos lassen.
- **FR-016**: Das System DARF die bisherige Alarm-Geste (Taste ≥ 2 s halten)
  NICHT mehr auswerten.

**Quittieren und übrige Bedienung**

- **FR-017**: Der Bediener MUSS den Alarm weiterhin mit einem kurzen Klick
  quittieren können; die Anlage geht danach in den Zustand „Pfeile holen".
- **FR-018**: Das System MUSS alle übrigen Kurzklick-Funktionen (Menü weiter,
  Wert ändern, bestätigen, Passe beenden) unverändert beibehalten.

**Automatische Abschaltung**

- **FR-019**: Das System MUSS die automatische Abschaltung wegen Inaktivität
  erst nach 60 Minuten ohne Bedienung auslösen (bisher 20 Minuten).
- **FR-020**: Das System MUSS jeden Tastendruck und jeden Zustandswechsel als
  Aktivität werten und die Frist neu starten (unverändert zum heutigen
  Verhalten).
- **FR-021**: Das System MUSS die automatische Abschaltung weiterhin nur in den
  Wartezuständen anwenden, nicht im laufenden Schießbetrieb und nicht im
  Alarmzustand.
- **FR-022**: Das System MUSS die automatische Abschaltung als solche kenntlich
  machen (Hinweistext auf dem Abschaltbildschirm), damit sie nicht als Defekt
  wahrgenommen wird.

**Dokumentation**

- **FR-023**: Die Bedienanleitung/Projektdokumentation MUSS die neue
  Tastenbelegung beschreiben (langer Druck = Aus, schnelle Mehrfachbetätigung =
  Alarm, 60 Minuten Inaktivitätsabschaltung).

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Ein Bediener löst den Alarm aus dem laufenden Schießbetrieb in
  unter 1,5 Sekunden ab dem ersten Tastendruck aus.
- **SC-001a**: Die Alarmfolge gelingt ungeübten Bedienern in mindestens 9 von 10
  Versuchen im ersten Anlauf (Nachweis, dass die 400-ms-Schwelle nicht zu eng
  gewählt ist).
- **SC-002**: In 100 % der geprüften Betriebszustände (alle fünf) schaltet das
  Gerät durch langes Halten jeder der beiden Tasten ab — 10 von 10 Versuchen je
  Kombination.
- **SC-003**: Kein einziger Ausschaltversuch löst einen Alarm aus und keine
  Alarmauslösung schaltet das Gerät ab (0 Verwechslungen in 20 Testdurchläufen).
- **SC-004**: Normale Bedienung erzeugt keinen Fehlalarm: 30 Minuten Bedienung
  im üblichen Tempo — inklusive zügigem Hochzählen der Werte im
  Konfigurationsmenü und wiederholtem Passe-Beenden — ohne eine einzige
  ungewollte Alarmauslösung.
- **SC-005**: Das Gerät bleibt bei Inaktivität mindestens 59 Minuten
  eingeschaltet und schaltet spätestens nach 61 Minuten ab.
- **SC-006**: Ein Bediener, der die Anlage zum ersten Mal benutzt, kann nach
  einer Erklärung von unter 30 Sekunden sowohl Alarm auslösen als auch
  ausschalten, ohne die Anleitung erneut zu lesen.
- **SC-007**: Die Änderung erhöht die gemessene Leistungsaufnahme im
  Normalbetrieb nicht messbar gegenüber dem heutigen Stand (0,20 W).

## Assumptions

- **A-001**: „Lang" bedeutet mehr als 3 Sekunden ununterbrochenes Halten; die
  bisherige Schwelle von 3 s wird beibehalten, da sie sich bewährt hat.
- **A-002**: „Sehr schnell hintereinander mehr als drei Mal" wird als
  Dreifachklick mit höchstens 400 ms Klickabstand umgesetzt (Entscheidung vom
  2026-08-02). Drei statt vier Klicks, weil die Geste unter Stress sicher
  ausführbar sein muss; dafür ein engeres Zeitfenster, damit sie sich klar von
  normaler Bedienung unterscheidet.
- **A-002a**: Die Beschränkung auf „Schießbetrieb" und „Pfeile holen"
  (Entscheidung vom 2026-08-02) deckt genau die Phasen ab, in denen Personen an
  der Schießlinie sein können, und hält das Konfigurationsmenü fehlalarmfrei.
- **A-002b**: Die Alarmfolge liegt allein auf OK (Entscheidung vom 2026-08-02
  nach dem Feldtest, ersetzt die ursprüngliche Festlegung „beide Tasten"). Eine
  Taste für die Sicherheitsfunktion ist eindeutiger zu vermitteln, und CONFIG
  bleibt vollständig frei für die Navigation.
- **A-003**: Die Hardware bleibt unverändert; es gibt weiterhin genau zwei
  Taster an der Bedieneinheit, und eingeschaltet wird hardwareseitig weiterhin
  über den Config-Taster.
- **A-004**: Der Wartungsmodus (beide Tasten beim Einschalten) bleibt
  unverändert erreichbar und ist von dieser Änderung nicht betroffen.
- **A-005**: Das Funkprotokoll zur Anzeigeeinheit bleibt unverändert — geändert
  wird ausschließlich, wie der Alarm an der Bedieneinheit ausgelöst wird.
- **A-006**: Die längere Abschaltfrist (60 statt 20 Minuten) ist ein bewusster
  Kompromiss zulasten der Akkulaufzeit; sie schützt weiterhin vor dem im Koffer
  vergessenen Gerät.
- **A-007**: Der Alarm bleibt an der Bedieneinheit ohne akustische Rückmeldung
  (kein Summer verbaut); die Rückmeldung erfolgt über den Bildschirm und die
  Anzeigeeinheit.

## Out of Scope

- Änderungen an der Anzeigeeinheit (Alarmsignalisierung, Lautstärke,
  Quittierung dort)
- Änderungen am Funkprotokoll oder an den Kommandos
- Zusätzliche Tasten, Hardwareänderungen oder ein Summer an der Bedieneinheit
- Konfigurierbarkeit der Schwellenwerte über das Menü (feste Werte)
- Weitere Stromsparmaßnahmen
