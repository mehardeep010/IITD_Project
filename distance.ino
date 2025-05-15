#include <Wire.h>
#include <VL53L1X.h>

VL53L1X sensor;
const int ledPin = 2; // GPIO25 (P25 on your board)
const int triggerDistance = 200; // 200 mm = 20 cm

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // GPIO21 = P21 (SDA), GPIO22 = P22 (SCL)

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("Sensor not detected!");
    while (1); // Stop here if not found
  }

  sensor.setDistanceMode(VL53L1X::Long); // Long range mode
  sensor.setMeasurementTimingBudget(50000); // 50 ms
  sensor.startContinuous(50); // Read every 50 ms
}

void loop() {
  int distance = sensor.read(); // Read distance in mm
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" mm");

  if (distance > 0 && distance < 200) {
    digitalWrite(ledPin, HIGH); // Object detected — turn on LED
  } else {
    digitalWrite(ledPin, LOW); // No object — turn off LED
  }

  delay(100);
}
