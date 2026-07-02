#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>

Adafruit_AHTX0 aht;

// CE -> PB0, CSN -> PA4
RF24 radio(PB0, PA4);

// Address "node1" must be exactly 5 bytes
const byte address[6] = "NODE1";

void setup() {
  Serial.begin(9600);
  
  // Give the serial monitor time to open safely
  while (!Serial) { delay(10); } 
  Serial.println("Starting Node Setup...");

  // Explicitly initialize SPI pins for the Blue Pill (SPI1) before starting radio
  SPI.setMOSI(PA7);
  SPI.setMISO(PA6);
  SPI.setSCLK(PA5);
  SPI.begin();

  if (!radio.begin()) {
    Serial.println("Radio hardware not responding! Check SPI wiring, CE, and CSN.");
    while (1);
  }

  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.enableDynamicPayloads();
  radio.stopListening();
  
  // WARNING: radio.printDetails() removed because it crashes STM32 cores without printf routing.
  
  Serial.println("Transmitter Initialized on Channel 76 with address 'NODE1'");

  // Explicitly declare your I2C pins. 
  // Change these to PB11 (SDA) and PB10 (SCL) if you are using the second I2C bus!
  Wire.setSDA(PB7); 
  Wire.setSCL(PB6);
  Wire.begin(); 
  
  Serial.println("Initializing AHT sensor...");
  if (!aht.begin()) {
    Serial.println("Could not find AHT sensor. Check I2C wiring and pull-up resistors!");
    while (1) delay(10);
  }
  Serial.println("AHT Sensor Found!");
}

void loop() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  // Read actual live data from your sensor instead of just a random number
  float liveTemp = temp.temperature;

  Serial.print("Sensor Temp: ");
  Serial.print(liveTemp);
  Serial.print(" C | Humidity: ");
  Serial.print(humidity.relative_humidity);
  Serial.println("%");

  // Send the live float over the air
  bool success = radio.write(&liveTemp, sizeof(liveTemp));
  
  if (success) {
    Serial.println("Sent successfully: " + String(liveTemp));
  } else {
    Serial.println("Transmission failed (No ACK received from listener)");
  }
  
  delay(1000);
}