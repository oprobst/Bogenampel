/**
 * @file SignalLink.cpp
 * @brief Implementierung des UART-Empfangs der Signaltöne
 */

#include "SignalLink.h"

#include <driver/gpio.h>

namespace {
    // Ein Byte dauert bei 9600 Baud rund 1 ms. Bleibt ein Telegramm länger
    // stecken, war der Anfang eine Störung — nicht auf den Rest warten.
    constexpr uint32_t FRAME_GAP_MS = 20;
}

void SignalReceiver::begin() {
    Serial2.begin(SignalLink::BAUD, SERIAL_8N1, Pins::SIGNAL_RX, -1);
    // Hält die Leitung auf Ruhepegel, solange der Empfänger bootet oder das Kabel
    // ab ist. Bewusst nicht pinMode(): das würde den Pin vom UART trennen.
    gpio_pullup_en((gpio_num_t)Pins::SIGNAL_RX);
}

void SignalReceiver::update() {
    if (position != 0 && millis() - lastByteMs > FRAME_GAP_MS) {
        position = 0;
        rejectCounter++;
    }

    while (Serial2.available() > 0) {
        parse((uint8_t)Serial2.read());
        lastByteMs = millis();
    }
}

void SignalReceiver::parse(uint8_t byte) {
    switch (position) {
        case 0:
            if (byte == SignalLink::FRAME_START) {
                position = 1;
            } else {
                rejectCounter++;
            }
            break;

        case 1:
            count = byte;
            position = 2;
            break;

        default:
            position = 0;
            if (count >= 1 && count <= SignalLink::MAX_COUNT
                && byte == (uint8_t)(SignalLink::FRAME_START ^ count)) {
                pendingCount = count;
                signalCounter++;
                lastSignalAt = millis();
            } else {
                rejectCounter++;
            }
            break;
    }
}

uint8_t SignalReceiver::takeSignal() {
    uint8_t c = pendingCount;
    pendingCount = 0;
    return c;
}
