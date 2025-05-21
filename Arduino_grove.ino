#include "Ultrasonic.h"

// Create Ultrasonic object on D7 (adjust if needed)
Ultrasonic ultrasonic(7);  // Pin 7 is connected to SIG pin of the Grove sensor

void setup() {
  Serial.begin(9600);  // Initialize Serial Monitor
  Serial.println("Grove Ultrasonic Sensor Initialized.");
}

void loop() {
  long distance_cm = ultrasonic.MeasureInCentimeters();  // Get distance
  Serial.print("Distance: ");
  Serial.print(distance_cm);
  Serial.println(" cm");

  delay(10);  // Wait a bit before next reading
}
