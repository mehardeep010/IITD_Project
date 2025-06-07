#include <ESP32Servo.h>

// === Pin Configuration ===
#define SIG_PIN 6            // Grove ultrasonic SIG pin
#define ESC_PIN 5       // ESC control pin (PWM via Servo library)

// === Control Parameters ===
float Kp = 20.0;             // Proportional gain (tune for stability)
float desiredHeight = 0.0;   // Target hover height in cm
int minPWM = 1100;
int maxPWM = 1800;

// === ESC Object ===
Servo esc_signal;

// === Distance Buffer for Averaging ===
float readings[4] = {0};
int bufIndex = 0;
bool filled = false;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Unicopter Hover P Controller");
  Serial.println("Enter target height (in cm):");

  // Wait for user input
  while (Serial.available() == 0) {}
  desiredHeight = Serial.parseFloat();
  Serial.print("Target height set to: ");
  Serial.print(desiredHeight);
  Serial.println(" cm");

  // Initialize ESC
  esc_signal.setPeriodHertz(50);               // Standard ESC frequency
  esc_signal.attach(ESC_PIN, minPWM, maxPWM);  // Attach to pin
  esc_signal.writeMicroseconds(minPWM);        // Arm the ESC
  delay(3000);
}

void loop() {
  static unsigned long lastTime = 0;
  if (millis() - lastTime >= 100) {  // Run control loop every 100ms
    lastTime = millis();

    float currentHeight = getAverageDistance();
    if (currentHeight < 0) return;  // Invalid reading

    float error = desiredHeight - currentHeight;
    float pwmValue = constrain(minPWM + Kp * error, minPWM, maxPWM);

    esc_signal.writeMicroseconds((int)pwmValue);

    // Print info (Serial Plotter-friendly)
    Serial.print("Height(cm): ");
    Serial.print(currentHeight, 1);
    Serial.print("\tError: ");
    Serial.print(error, 1);
    Serial.print("\tPWM: ");
    Serial.println(pwmValue, 1);
  }

  // Update distance buffer
  float d = measureDistance();
  if (d > 0) {
    readings[bufIndex] = d;
    bufIndex = (bufIndex + 1) % 4;
    if (bufIndex == 0) filled = true;
  }
}

// === Ultrasonic Sensor Reading ===
float measureDistance() {
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(SIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SIG_PIN, LOW);
  pinMode(SIG_PIN, INPUT);

  long duration = pulseIn(SIG_PIN, HIGH, 10000);  // Max 10 ms (~1.7 m)
  if (duration == 0) return -1;
  return duration * 0.0343 / 2.0;
}

float getAverageDistance() {
  float sum = 0;
  int count = filled ? 4 : bufIndex;
  for (int i = 0; i < count; i++) sum += readings[i];
  return (count > 0) ? sum / count : -1;
}
