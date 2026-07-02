#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <SPI.h>
// #include <nRF24L01.h>
#include <RF24.h>

#include <ESP32Servo.h>

Servo myServo;

Adafruit_AHTX0 aht;

RF24 radio(2, 3);
const byte address[6] = "NODE1";


void setup() {
  Serial.begin(115200);
  delay(300);

  // servo code
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  myServo.setPeriodHertz(50);    // Standard 50hz servo
  myServo.attach(0, 500, 2400); // Attach with min/max pulse widths

  // AHT code
  Wire.begin(8, 9);
  aht.begin();

  SPI.begin(4, 6, 5);

  // Radio code

  if (!radio.begin()) {
    Serial.println("Radio hardware not responding!");
    while (1);
  }


  radio.setChannel(76);
  
  // Set the data rate to 1Mbps (balance of range and speed)
  radio.setDataRate(RF24_1MBPS);
  
  // Open the writing pipe with our new node address
  radio.openWritingPipe(address);
  
  radio.setPALevel(RF24_PA_LOW);
  radio.enableDynamicPayloads();
  radio.stopListening();
  radio.printDetails();
}

void loop() {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

  float t = temp.temperature;
  float h = humidity.relative_humidity;

  char payload[32];
  uint8_t retries = radio.getARC();

  // snprintf(payload, sizeof(payload), "%.2f,%.2f,%u", t, h, retries);
  // radio.write(&payload, sizeof(payload));

  // const float FAKEtemp = (float)random(0, 40);
  radio.write(&t, sizeof(t));
  Serial.println("Sent: " + String(t));

  Serial.println(payload);

  if(t > 25.0){

    for (int pos = 20; pos <= 180; pos += 20) {
      myServo.write(pos);
      delay(200);
    }

    for (int pos = 180; pos >= 20; pos--) {
      myServo.write(pos);
      delay(15);
    }
    

    myServo.write(20);
  }

  // radio.powerDown();
  delay(2000);
}