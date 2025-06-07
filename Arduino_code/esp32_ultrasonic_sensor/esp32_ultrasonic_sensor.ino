#define TRIG_PIN 9   // TRIG connected to digital pin 9
#define ECHO_PIN 10  // ECHO connected to digital pin 10

long duration;
float distance_cm;

void setup() {
  Serial.begin(9600);              // Start serial communication
  pinMode(TRIG_PIN, OUTPUT);       // TRIG is an output
  pinMode(ECHO_PIN, INPUT);        // ECHO is an input
  Serial.println("Ultrasonic Sensor Test");
}

void loop() {
  // Send a 10 microsecond pulse to trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Read the duration of the echo pulse
  duration = pulseIn(ECHO_PIN, HIGH);

  // Convert duration to distance in cm (speed of sound = 343 m/s)
  distance_cm = duration * 0.0343 / 2;

  // Print the distance
  Serial.print("Distance: ");
  Serial.print(distance_cm);
  Serial.println(" cm");

  delay(500); // Wait half a second before next reading
}
