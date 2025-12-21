/**
 * @file TestEmpfaenger.ino
 * @brief Einfacher NRF24L01 Empfänger-Test
 *
 * Empfängt Pakete vom Sender und zeigt sie an.
 * Verwende dieses Programm um die Funk-Hardware zu testen.
 */

#include <SPI.h>
#include <RF24.h>

// NRF24L01 Pins (Arduino Nano)
#define NRF_CE  7
#define NRF_CSN 8

RF24 radio(NRF_CE, NRF_CSN);

// Pipe-Adresse (identisch mit Sender!)
const uint8_t address[5] = {'B', 'A', 'M', 'P', 'L'};

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("================================="));
  Serial.println(F("  NRF24L01 Empfaenger Test"));
  Serial.println(F("================================="));
  Serial.println();

  // SPI initialisieren
  SPI.begin();
  delay(100);

  // NRF24L01 initialisieren
  Serial.print(F("Initialisiere Radio... "));
  if (!radio.begin()) {
    Serial.println(F("FEHLER!"));
    Serial.println(F("Radio nicht gefunden! Pruefe Verkabelung."));
    while(1) {
      delay(1000);
    }
  }
  Serial.println(F("OK"));

  // Konfiguration
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_250KBPS);
  radio.setChannel(76);
  radio.setPayloadSize(1);  // 1 Byte
  radio.setAutoAck(false);

  // RX-Modus
  radio.openReadingPipe(1, address);
  radio.startListening();

  // Status ausgeben
  Serial.println();
  Serial.println(F("Konfiguration:"));
  Serial.print(F("  Kanal: "));
  Serial.println(radio.getChannel());
  Serial.print(F("  Power: "));
  Serial.println(radio.getPALevel());
  Serial.print(F("  Rate: "));
  Serial.println(radio.getDataRate());
  Serial.print(F("  Variant: "));
  Serial.println(radio.isPVariant() ? F("Plus") : F("Standard"));
  Serial.println();
  Serial.println(F("Warte auf Empfang..."));
  Serial.println(F("---------------------------------"));
}

void loop() {
  // Prüfe ob Daten verfügbar
  if (radio.available()) {
    uint8_t data;
    radio.read(&data, 1);

    // Empfangenes Byte ausgeben
    Serial.print(F("RX: "));
    Serial.println(data);
  }

  delay(10);
}
