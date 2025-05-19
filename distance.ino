#include <Wire.h>
#include <VL53L1X.h>

VL53L1X sensor;

const int SDA_PIN = 8;      // Change if needed
const int SCL_PIN = 9;      // Change if needed
const int ALERT_PIN = 2;    // GPIO for output alert (LED/Buzzer)

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  pinMode(ALERT_PIN, OUTPUT);
  digitalWrite(ALERT_PIN, LOW); // Turn off alert initially

  delay(100);

  sensor.setTimeout(500);

  if (!sensor.init()) {
    Serial.println("Failed to detect VL53L1X.");
    while (1);
  }

  // Optional: increase range (Short, Medium, Long)
  sensor.setDistanceMode(VL53L1X::Long); 
  sensor.setMeasurementTimingBudget(50000); // in microseconds

  sensor.startContinuous(100); // read every 100 ms

  Serial.println("Sensor initialized.");
}

void loop() {
  uint16_t distance = sensor.read();

  Serial.print("Distance (mm): ");
  Serial.println(distance);

  if (distance < 200) { // 200 mm = 20 cm
    Serial.println("⚠️ Obstacle Detected!");
    digitalWrite(ALERT_PIN, HIGH); // Turn on alert (e.g., LED/Buzzer)
  } else {
    digitalWrite(ALERT_PIN, LOW); // Turn off alert
  }

  delay(100);
}
