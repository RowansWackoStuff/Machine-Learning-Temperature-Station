#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// CE -> PB0, CSN -> PA4
RF24 radio(PB0, PA4);

// Address "node1" must be exactly 5 bytes
const byte address[6] = "NODE1";

void setup() {
  Serial.begin(9600);
  delay(2000);
  Serial.println("Hello");

  if (!radio.begin()) {
    Serial.println("Radio hardware not responding!");
    while (1);
  }

  // Set the frequency channel to 74 (2.474 GHz)
  radio.setChannel(76);
  
  // Set the data rate to 1Mbps (balance of range and speed)
  radio.setDataRate(RF24_1MBPS);
  
  // Open the writing pipe with our new node address
  radio.openWritingPipe(address);
  
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();
  
  Serial.println("Transmitter Initialized on Channel 76 with address 'NODE1'");
}

void loop() {
  const char text[] = "Node1 Active";
  radio.write(&text, sizeof(text));
  delay(1000);
}