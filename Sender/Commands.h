/**
 * @file Commands.h
 * @brief ESP-NOW-Protokoll-Definitionen für Bogenampel V3 (Sender ↔ Empfänger)
 *
 * Definiert das Funkprotokoll zwischen Sender und Empfänger.
 * WICHTIG: Diese Datei MUSS byte-identisch in Sender/ und Empfaenger/ sein!
 * Verbindliche Quelle: specs/004-v3-esp32-port/contracts/espnow-protocol.md
 *
 * Frame (6 Bytes, packed):
 * - Byte 0-1: magic 'B','3' (Filter gegen fremde ESP-NOW-Frames)
 * - Byte 2:   FrameType (CMD/HELLO/HELLO_ACK/PING/PING_ACK)
 * - Byte 3:   Sequenznummer (Dedup bei Retries)
 * - Byte 4:   RadioCommand (nur bei FT_CMD, sonst 0x00)
 * - Byte 5:   XOR-Checksumme (command ^ 0xFF, V2-Semantik)
 *
 * @date 2026-06-11
 * @version 3.0 - ESP-NOW statt NRF24 (11 Kommandos unverändert aus V2)
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Radio-Kommando-Codes (11 Kommandos, Semantik unverändert aus V2)
 */
enum RadioCommand : uint8_t {
    CMD_STOP = 0x01,       // Timer stoppen, rote Ampel
    CMD_START_120 = 0x02,  // Timer starten: 120 Sekunden (inkl. 10s Vorbereitung)
    CMD_START_240 = 0x03,  // Timer starten: 240 Sekunden (inkl. 10s Vorbereitung)
    CMD_INIT = 0x04,       // Empfänger initialisieren (Turnier-Start)
    CMD_ALARM = 0x05,      // Not-Alarm auslösen
    CMD_PING = 0x06,       // Connection Quality Test (ACK-basiert)
    CMD_GROUP_AB = 0x08,   // Gruppe A/B aktiv - Komplette Passe (+ Stop/Rot)
    CMD_GROUP_CD = 0x09,   // Gruppe C/D aktiv - Komplette Passe (+ Stop/Rot)
    CMD_GROUP_NONE = 0x0A, // Keine Gruppe aktiv (beide aus, 1-2 Schützen Modus)
    CMD_GROUP_FINISH_AB = 0x0B,  // Halbe Passe: Start bei zweiter Gruppe nach A/B
    CMD_GROUP_FINISH_CD = 0x0C   // Halbe Passe: Start bei zweiter Gruppe nach C/D
};

/**
 * @brief Frame-Typen (Transport-Ebene, neu in V3)
 */
enum FrameType : uint8_t {
    FT_CMD       = 0x01,  // Kommando Sender → Empfänger
    FT_HELLO     = 0x02,  // Discovery-Broadcast Sender → alle
    FT_HELLO_ACK = 0x03,  // Discovery-Antwort Empfänger → Sender (unicast)
    FT_PING      = 0x04,  // Qualitätstest Sender → Empfänger
    FT_PING_ACK  = 0x05   // optionale App-Antwort (Link-ACK genügt für Qualität)
};

// Magic-Bytes (Frame-Filter)
constexpr uint8_t PACKET_MAGIC0 = 'B';  // 0x42
constexpr uint8_t PACKET_MAGIC1 = '3';  // 0x33

/**
 * @brief Radio-Paket-Struktur (6 Bytes, für ESP-NOW-Übertragung)
 */
struct RadioPacketV3 {
    uint8_t magic0;    // 'B' (0x42)
    uint8_t magic1;    // '3' (0x33)
    uint8_t type;      // FrameType
    uint8_t seq;       // Sequenznummer, pro Boot zufällig initialisiert, ++ pro Sendung
    uint8_t command;   // RadioCommand bei type==FT_CMD, sonst 0x00
    uint8_t checksum;  // command ^ 0xFF
} __attribute__((packed));

// Compile-Zeit-Prüfung: RadioPacketV3 muss exakt 6 Bytes sein
static_assert(sizeof(RadioPacketV3) == 6, "RadioPacketV3 must be exactly 6 bytes");

/**
 * @brief Berechnet XOR-Checksumme für Kommando
 * @param command Kommando-Code
 * @return Checksumme (command XOR 0xFF)
 */
inline uint8_t calculateChecksum(uint8_t command) {
    return command ^ 0xFF;
}

/**
 * @brief Validiert ein empfangenes Paket (magic + Checksumme)
 * @param packet Zeiger auf RadioPacketV3
 * @return true wenn magic und Checksumme korrekt, false sonst
 */
inline bool validatePacket(const RadioPacketV3* packet) {
    return (packet->magic0 == PACKET_MAGIC0)
        && (packet->magic1 == PACKET_MAGIC1)
        && (packet->checksum == (packet->command ^ 0xFF));
}

/**
 * @brief Hilfsfunktion: Kommando als String (für Debugging)
 * @param cmd RadioCommand
 * @return String-Repräsentation
 */
inline const char* commandToString(RadioCommand cmd) {
    switch (cmd) {
        case CMD_STOP:       return "STOP";
        case CMD_START_120:  return "START_120";
        case CMD_START_240:  return "START_240";
        case CMD_INIT:       return "INIT";
        case CMD_ALARM:      return "ALARM";
        case CMD_PING:       return "PING";
        case CMD_GROUP_AB:   return "GROUP_AB";
        case CMD_GROUP_CD:   return "GROUP_CD";
        case CMD_GROUP_NONE: return "GROUP_NONE";
        case CMD_GROUP_FINISH_AB: return "GROUP_FINISH_AB";
        case CMD_GROUP_FINISH_CD: return "GROUP_FINISH_CD";
        default:             return "UNKNOWN";
    }
}

/**
 * @brief Transmission Result (für sendCommand-Rückgabewert, V2-API erhalten)
 */
enum TransmissionResult {
    TX_SUCCESS,   // Link-Layer-ACK empfangen, Kommando zugestellt
    TX_TIMEOUT,   // Kein ACK nach Retries (Empfänger nicht erreichbar)
    TX_ERROR      // esp_now-Fehler (Hardware/API)
};
