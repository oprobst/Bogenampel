/**
 * @file PowerManager.cpp
 * @brief Implementierung des Power-Managements
 */

#include "PowerManager.h"

PowerManager::PowerManager()
    : filteredMv(3700)  // plausibler Startwert bis zur ersten Messung
    , usbConnected(false)
    , charge(ChargeState::NO_INPUT)
    , lastUpdateMs(0) {
}

void PowerManager::begin() {
    // Lader-Status: Open-Drain-Ausgänge des MCP73837 → interne Pullups (R-12)
    pinMode(Pins::C_ST1, INPUT_PULLUP);
    pinMode(Pins::C_ST2, INPUT_PULLUP);
    pinMode(Pins::C_PG, INPUT_PULLUP);
    // C_PRG MUSS hochohmig bleiben (INPUT): R20 definiert den Pegel.
    // NICHT auf OUTPUT HIGH setzen — 3,3 V liegen im Shutdown-Fenster des
    // MCP73837 (VIH = 0,8 × 5 V = 4,0 V), der Lader wuerde abschalten. Siehe
    // Kopfkommentar in PowerManager.h.
    pinMode(Pins::C_PRG, INPUT);

    // USB-Erkennung (VBUS-Teiler, aktiv HIGH)
    pinMode(Pins::USB_CON, INPUT);

    // Erste Messung sofort
    update();
    lastUpdateMs = millis();
}

uint16_t PowerManager::readBatteryMv() {
    // analogReadMilliVolts nutzt die eFuse-Kalibrierung (R-11);
    // Teiler R4/R8 = 150k/100k → V_BAT = V_ADC × 2,5
    uint32_t adcMv = analogReadMilliVolts(Pins::ADC_BAT);
    return (uint16_t)((adcMv * 5) / 2);
}

bool PowerManager::update() {
    uint32_t now = millis();
    if (lastUpdateMs != 0 && (now - lastUpdateMs) < Battery::UPDATE_INTERVAL_MS) {
        return false;
    }
    lastUpdateMs = now;

    // Median-5-Filter (V2-Verfahren) gegen Ausreißer
    uint16_t samples[Battery::FILTER_SIZE];
    for (uint8_t i = 0; i < Battery::FILTER_SIZE; i++) {
        samples[i] = readBatteryMv();
        delayMicroseconds(200);
    }
    // Insertion-Sort (5 Werte)
    for (uint8_t i = 1; i < Battery::FILTER_SIZE; i++) {
        uint16_t key = samples[i];
        int8_t j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
    filteredMv = samples[Battery::FILTER_SIZE / 2];

    usbConnected = (digitalRead(Pins::USB_CON) == HIGH);
    readChargerStatus();

    DEBUG_PRINTF("Akku: %u mV (%u%%), USB: %d, Lader: %d\n",
                 filteredMv, batteryPercent(), usbConnected, (int)charge);
    return true;
}

void PowerManager::readChargerStatus() {
    // MCP73837: Open-Drain, LOW = aktiv, HIGH = Hi-Z (interner Pullup zieht hoch)
    bool pgOk = (digitalRead(Pins::C_PG) == LOW);    // Eingangsspannung gültig
    bool st1 = (digitalRead(Pins::C_ST1) == LOW);    // STAT1 aktiv
    bool st2 = (digitalRead(Pins::C_ST2) == LOW);    // STAT2 aktiv

    if (!pgOk) {
        // PG Hi-Z = Shutdown (kein Eingang über UVLO)
        charge = ChargeState::NO_INPUT;
    } else if (st1 && st2) {
        // Einziger Zustand mit beiden STAT aktiv — nicht "Fehler", sondern der
        // LDO-Testmodus (THERM über VDD-100 mV). Im Betrieb unerwartet.
        charge = ChargeState::TEST_MODE;
    } else if (st1) {
        charge = ChargeState::CHARGING;     // Precondition / CC / CV
    } else if (st2) {
        charge = ChargeState::COMPLETE;     // Charge Complete – Standby
    } else {
        // PG aktiv, beide STAT Hi-Z: Standby (PROG1/PROG2 floating),
        // Temperature Fault oder Timer Fault. Table 5-1 unterscheidet die drei
        // nicht — gemeinsam ist ihnen, dass NICHT geladen wird. Früher wurde
        // dieser Zweig als COMPLETE ("voll") angezeigt und hat damit einen
        // offenen THERM-Pin als vollen Akku kaschiert.
        charge = ChargeState::SUSPENDED;
    }

    DEBUG_PRINTF("Lader: ST1=%c ST2=%c PG=%c -> %d\n",
                 st1 ? 'L' : 'Z', st2 ? 'L' : 'Z', pgOk ? 'L' : 'Z', (int)charge);
}

uint8_t PowerManager::batteryPercent() const {
    // Linear zwischen VOLTAGE_MIN und VOLTAGE_MAX (data-model.md §5)
    if (filteredMv <= Battery::VOLTAGE_MIN_MV) return 0;
    if (filteredMv >= Battery::VOLTAGE_MAX_MV) return 100;
    return (uint8_t)(((uint32_t)(filteredMv - Battery::VOLTAGE_MIN_MV) * 100)
                     / (Battery::VOLTAGE_MAX_MV - Battery::VOLTAGE_MIN_MV));
}

void PowerManager::latchOff() {
    DEBUG_PRINTLN("Power-Off: LATCH -> LOW");
    delay(50);  // Debug-Ausgabe noch rausschieben
    digitalWrite(Pins::LATCH, LOW);
    // Ab hier fällt die Versorgung — Endlosschleife für den Restweg
    while (true) {
        delay(100);
    }
}
