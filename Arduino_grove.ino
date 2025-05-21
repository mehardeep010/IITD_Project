// Define the GPIO connected to the SIG pin of the Grove Ultrasonic Sensor
const int signalPin = 8;  // Use any free GPIO on ESP32-C6-M1

long duration;
float distanceCm;

void setup() {
  Serial.begin(115200);
  pinMode(signalPin, OUTPUT);
  digitalWrite(signalPin, LOW);  // Ensure pin is LOW initially
}

void loop() {
  // Trigger the ultrasonic pulse
  pinMode(signalPin, OUTPUT);
  digitalWrite(signalPin, LOW);
  delayMicroseconds(2);
  digitalWrite(signalPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(signalPin, LOW);

  // Listen for echo
  pinMode(signalPin, INPUT);
  duration = pulseIn(signalPin, HIGH);

  // Calculate the distance in cm
  distanceCm = duration * 0.0343 / 2;

  // Output the result
  Serial.print("Distance: ");
  Serial.print(distanceCm);
  Serial.println(" cm");

  delay(500);  // Wait before next reading
}
