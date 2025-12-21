/**
 * @file TestSender.ino
 * @brief Einfacher NRF24L01 Sender-Test
 *
 * Sendet alle 1 Sekunde einen Zähler (1 Byte).
 * Verwende dieses Programm um die Funk-Hardware zu testen.
 */

#include <SPI.h>
#include <RF24.h>

// NRF24L01 Pins (Arduino Nano)
#define NRF_CE  9
#define NRF_CSN 8

RF24 radio(NRF_CE, NRF_CSN);

// Pipe-Adresse (identisch mit Empfänger!)
const uint8_t address[5] = {'B', 'A', 'M', 'P', 'L'};

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("================================="));
  Serial.println(F("  NRF24L01 Sender Test"));
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

  // TX-Modus
  radio.stopListening();
  radio.openWritingPipe(address);

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
  Serial.println(F("Sende alle 1 Sekunde..."));
  Serial.println(F("---------------------------------"));
}

uint8_t counter = 0;

void loop() {
  counter++;

  // Sende 1 Byte
  bool ok = radio.write(&counter, 1);

  // Status ausgeben
  Serial.print(F("TX #"));
  Serial.print(counter);
  Serial.print(F(": "));
  Serial.println(ok ? F("OK") : F("FAIL"));

  delay(1000);
}
