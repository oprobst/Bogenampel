/**
 * @file Commands.h
 * @brief RF-Kommando-Definitionen für Bogenampel Sender ↔ Empfänger
 *
 * Definiert das Protokoll für die Funkübertragung zwischen Sender und Empfänger.
 * WICHTIG: Diese Datei MUSS identisch im Sender und Empfänger sein!
 *
 * Protokoll (2 Bytes):
 * - Byte 0: Kommando-Typ (RadioCommand)
 * - Byte 1: XOR-Checksumme (command ^ 0xFF)
 *
 * @date 2025-12-21
 * @version 2.1 - Halbe Passe (11 Kommandos)
 */

#pragma once

#include <Arduino.h>

/**
 * @brief Radio-Kommando-Codes (11 Kommandos für Benutzerführung)
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
 * @brief Radio-Paket-Struktur (2 Bytes, für nRF24L01+ Übertragung)
 */
#pragma pack(push, 1)
struct RadioPacket {
    uint8_t command;    // Kommando-Code (RadioCommand)
    uint8_t checksum;   // XOR-Checksumme (command ^ 0xFF)
};
#pragma pack(pop)

// Compile-Zeit-Prüfung: RadioPacket muss exakt 2 Bytes sein
static_assert(sizeof(RadioPacket) == 2, "RadioPacket must be exactly 2 bytes");

/**
 * @brief Berechnet XOR-Checksumme für Kommando
 * @param command Kommando-Code
 * @return Checksumme (command XOR 0xFF)
 */
inline uint8_t calculateChecksum(uint8_t command) {
    return command ^ 0xFF;
}

/**
 * @brief Validiert RadioPacket-Checksumme
 * @param packet Zeiger auf RadioPacket
 * @return true wenn Checksumme korrekt, false sonst
 */
inline bool validateChecksum(const RadioPacket* packet) {
    return (packet->checksum == (packet->command ^ 0xFF));
}

/**
 * @brief Hilfsfunktion: Kommando als String (für Debugging)
 * @param cmd RadioCommand
 * @return String-Repräsentation (PROGMEM)
 */
inline const __FlashStringHelper* commandToString(RadioCommand cmd) {
    switch (cmd) {
        case CMD_STOP:       return F("STOP");
        case CMD_START_120:  return F("START_120");
        case CMD_START_240:  return F("START_240");
        case CMD_INIT:       return F("INIT");
        case CMD_ALARM:      return F("ALARM");
        case CMD_PING:       return F("PING");
        case CMD_GROUP_AB:   return F("GROUP_AB");
        case CMD_GROUP_CD:   return F("GROUP_CD");
        case CMD_GROUP_NONE: return F("GROUP_NONE");
        case CMD_GROUP_FINISH_AB: return F("GROUP_FINISH_AB");
        case CMD_GROUP_FINISH_CD: return F("GROUP_FINISH_CD");
        default:             return F("UNKNOWN");
    }
}

/**
 * @brief Transmission Result (für sendCommand-Rückgabewert)
 */
enum TransmissionResult {
    TX_SUCCESS,   // ACK empfangen, Kommando zugestellt
    TX_TIMEOUT,   // Kein ACK nach Retries (Empfänger nicht erreichbar)
    TX_ERROR      // Radio-Hardware-Fehler
};
