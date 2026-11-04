# Contract: ESP-NOW-Funkprotokoll Bogenampel V3

**Feature**: 004-v3-esp32-port | **Status**: verbindlich für `SenderV3/Commands.h` und
`EmpfaengerV3/Commands.h` (beide Kopien MÜSSEN identisch sein)

## Transport

| Parameter | Wert |
|-----------|------|
| Verfahren | ESP-NOW (connectionless), beide Geräte `WIFI_STA`, kein AP-Connect |
| Kanal | **1** (fest, `esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)`) |
| Adressierung | Discovery zur Laufzeit (HELLO-Broadcast → HELLO_ACK unicast), danach nur Unicast |
| Verschlüsselung | keine (Validierung über magic + checksum + seq) |
| Zustellbestätigung | ESP-NOW Send-Callback (`esp_now_send_status_t`) auf Unicast-Frames |
| App-Retry | max. 3 Versuche, 50 ms Abstand, Gesamtbudget < 500 ms pro Kommando |
| Power Save (nur Sender) | Connectionless PS, Wake-Interval 100 ms; Empfangsfenster duty-cycled (Regel 7) |

## Frame-Format (6 Bytes, packed)

```c
struct RadioPacketV3 {
    uint8_t magic0;    // 'B' (0x42)
    uint8_t magic1;    // '3' (0x33)
    uint8_t type;      // FrameType
    uint8_t seq;       // Sequenznummer, pro Boot zufällig initialisiert, ++ pro Sendung
    uint8_t command;   // RadioCommand bei type==FT_CMD, sonst 0x00
    uint8_t checksum;  // command ^ 0xFF
};
```

```c
enum FrameType : uint8_t {
    FT_CMD       = 0x01,  // Kommando Sender → Empfänger
    FT_HELLO     = 0x02,  // Discovery-Broadcast Sender → alle
    FT_HELLO_ACK = 0x03,  // Discovery-Antwort Empfänger → Sender (unicast)
    FT_PING      = 0x04,  // Qualitätstest Sender → Empfänger
    FT_PING_ACK  = 0x05   // optionale App-Antwort (Link-ACK genügt für Qualität)
};
```

## Kommandos (Semantik unverändert aus V2, FR-002)

| Code | Kommando | Empfänger-Verhalten |
|------|----------|---------------------|
| 0x01 | CMD_STOP | Timer stoppen, rot, 3 Pieptöne (nur bei laufendem Timer) |
| 0x02 | CMD_START_120 | 10 s Vorbereitung (rot, 2 Pieptöne), dann 120 s Countdown |
| 0x03 | CMD_START_240 | 10 s Vorbereitung (rot, 2 Pieptöne), dann 240 s Countdown |
| 0x04 | CMD_INIT | Anzeige initialisieren (Turnier-Start), rot |
| 0x05 | CMD_ALARM | Countdown abbrechen, 8× alles rot blinken, dann gestoppt |
| 0x06 | CMD_PING | keine sichtbare Aktion (Qualitätstest) |
| 0x08 | CMD_GROUP_AB | Gruppe A/B anzeigen (ganze Passe) |
| 0x09 | CMD_GROUP_CD | Gruppe C/D anzeigen (ganze Passe) |
| 0x0A | CMD_GROUP_NONE | keine Gruppenanzeige (1-2 Schützen) |
| 0x0B | CMD_GROUP_FINISH_AB | halbe Passe: zweite Gruppe nach A/B |
| 0x0C | CMD_GROUP_FINISH_CD | halbe Passe: zweite Gruppe nach C/D |

## Regeln

1. **Validierung (Constitution I)**: Frame verwerfen, wenn Länge ≠ 6, magic ≠ "B3" oder
   `checksum != (command ^ 0xFF)`. Kein Logging-Zwang, aber Debug-Zähler empfohlen.
2. **Dedup (FR-008)**: Empfänger merkt sich die `seq` des letzten akzeptierten Frames pro
   Sender-MAC; gleicher Wert → verwerfen (Retries sind dadurch idempotent).
3. **Autonomie (FR-004/FR-004a)**: Es existiert KEIN Kommando für das reguläre Passenende.
   CMD_STOP wird ausschließlich bei manuellem Abbruch/Alarm-Quittierung gesendet. Der Sender
   sendet bei Zeitablauf nichts; das CMD_START_* für die zweite Gruppe (3-4 Schützen) ist ein
   Sync-Signal, das der Empfänger ignorieren MUSS, wenn seine Vorbereitungsphase bereits läuft.
4. **Discovery**: Sender sendet FT_HELLO als Broadcast (max. 1 Hz, bis HELLO_ACK empfangen);
   Empfänger antwortet FT_HELLO_ACK unicast und registriert die Sender-MAC als Peer; der Sender
   registriert die Empfänger-MAC. Verbindungsverlust erfordert keine neue Discovery (MACs bleiben
   gültig); nach Sender-Reboot läuft die Discovery erneut. Zusätzlich verwirft der Sender den
   Peer nach `Radio::RELINK_AFTER_FAILURES` (3) erfolglosen Kommandos in Folge und sucht neu —
   nur so wird ein **ausgetauschter** Empfänger überhaupt gefunden.
5. **Qualitätstest (FR-009)**: 10 × FT_PING im 250-ms-Raster während des Splash; Qualität = %
   der per Send-Callback bestätigten Frames. Ohne HELLO_ACK: „keine Verbindung" anzeigen,
   Bedienung trotzdem freigeben.
6. **Sende-API (V2-kompatibel)**: `sendCommand(RadioCommand) → TransmissionResult`
   (`TX_SUCCESS` = Callback OK ≤ Retries, `TX_TIMEOUT` = alle Retries NACK, `TX_ERROR` =
   esp_now-Fehler). Alarm nutzt wie V2 zusätzlich 3 App-Level-Wiederholungen mit 200 ms Abstand:
   jede App-Wiederholung ist ein **neuer Frame (neue `seq`)** und erfolgt nur, wenn der Transport
   `TX_TIMEOUT`/`TX_ERROR` gemeldet hat. Der Empfänger MUSS ein CMD_ALARM während eines bereits
   laufenden Alarm-Musters ignorieren (idempotent — Dedup per `seq` greift hier nicht). Das
   500-ms-Budget (FR-007/SC-002) gilt pro Transport-Sendung; die App-Wiederholungen dürfen es
   in Summe überschreiten (Worst Case ~1,5 s bei gestörtem Funk — bewusster Trade-off zugunsten
   der Zustellwahrscheinlichkeit des sicherheitskritischen Alarms).
7. **Empfangsfenster des Senders (Stromsparen)**: Der Sender ist nach der Discovery ein reiner
   Sender und hält sein ESP-NOW-Empfangsfenster geschlossen (`esp_now_set_wake_window(0)`); das
   802.11-ACK auf eigene Frames kommt im Sendefenster zurück und bleibt davon unberührt. Während
   der Discovery wird das Fenster nur um den HELLO-Broadcast herum geöffnet
   (`Radio::HELLO_LISTEN_MS` = 500 ms), damit das HELLO_ACK ankommt. Ohne diese Regel läuft das
   Radio dauerhaft auf Empfang — gemessen ~0,35 W statt ~0,20 W.
   Voraussetzung ist `CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE=1` (in den vorkompilierten
   arduino-esp32-Libs erfüllt) plus aktives `WIFI_PS_MIN_MODEM`; schlagen die Treiberaufrufe
   fehl, meldet der Sender das im Log als `WARNUNG: Power Save nicht aktiv`.
   **Der Empfänger ist ausgenommen** — er muss jederzeit empfangsbereit sein und hängt ohnehin
   am Netzteil.
