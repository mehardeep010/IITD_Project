#include <WiFi.h>
#include <ESP32Servo.h>
#include <pidautotuner.h>

// WiFi credentials
const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

WiFiServer server(80);

// Ultrasonic Sensor pins
#define TRIG_PIN 14
#define ECHO_PIN 12

// ESC4
Servo esc_signal;
int pwm_us = 1000;

// PID variables
double setpoint = 50.0;  // Target height in cm
double input = 0.0;      // Measured distance
double output = 0.0;     // PID output (mapped to PWM)

// PID gains (will be updated after autotune)
double kp = 1.0, ki = 0.5, kd = 0.1;

// Loop timing
const unsigned long loopInterval = 100000; // 100 ms in microseconds
unsigned long lastLoopTime = 0;

// PID autotuner
PIDAutotuner tuner;

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // ESC Setup
  esc_signal.setPeriodHertz(50);
  esc_signal.attach(13, 1000, 2000);
  esc_signal.writeMicroseconds(1000);
  delay(3000);

  // WiFi Setup
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP Address: ");
  Serial.println(WiFi.localIP());
  server.begin();

  // PID Autotune
  autotunePID();

  // Start control loop timer
  lastLoopTime = micros();
}

void loop() {
  // Basic web control interface
  handleClient();

  // PID control loop every 100ms
  unsigned long now = micros();
  if (now - lastLoopTime >= loopInterval) {
    input = measureDistance();
    double error = setpoint - input;
    static double integral = 0, lastError = 0;

    integral += error * (loopInterval / 1000000.0);
    double derivative = (error - lastError) / (loopInterval / 1000000.0);
    output = kp * error + ki * integral + kd * derivative;
    lastError = error;

    // Map output to ESC PWM range
    output = constrain(output, -500, 500);
    pwm_us = constrain(1500 + output, 1000, 2000);
    esc_signal.writeMicroseconds(pwm_us);

    Serial.printf("Dist: %.2f cm | PWM: %d µs | Setpoint: %.2f cm\n", input, pwm_us, setpoint);
    lastLoopTime = now;
  }
}

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;
  return duration * 0.0343 / 2;
}

void handleClient() {
  WiFiClient client = server.available();
  if (!client) return;

  String request = "";
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      request += c;
      if (request.endsWith("\r\n\r\n")) break;
    }
  }

  if (request.indexOf("GET /inc") >= 0) setpoint += 5;
  if (request.indexOf("GET /dec") >= 0) setpoint -= 5;
  if (request.indexOf("GET /stop") >= 0) esc_signal.writeMicroseconds(1000);

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close\n");

  client.println("<!DOCTYPE html><html><body><h2>Unicopter Altitude Control</h2>");
  client.println("<p>Current Height: " + String(input) + " cm</p>");
  client.println("<p>Target Height: " + String(setpoint) + " cm</p>");
  client.println("<a href='/inc'>Increase Setpoint</a><br>");
  client.println("<a href='/dec'>Decrease Setpoint</a><br>");
  client.println("<a href='/stop'>Stop Motor</a>");
  client.println("</body></html>");
  client.stop();
}

void autotunePID() {
  Serial.println("Starting PID Autotune...");
  tuner.setTargetInputValue(setpoint);
  tuner.setLoopInterval(loopInterval);
  tuner.setOutputRange(0, 255);
  tuner.setZNMode(PIDAutotuner::ZNModeLessOvershoot);
  tuner.startTuningLoop(micros());

  long microseconds = micros();
  while (!tuner.isFinished()) {
    long now = micros();
    double currentInput = measureDistance();
    double outputValue = tuner.tunePID(currentInput, now);

    esc_signal.writeMicroseconds(map(outputValue, 0, 255, 1000, 2000));

    while (micros() - now < loopInterval) delayMicroseconds(1);
  }

  esc_signal.writeMicroseconds(1000); // turn off motor

  kp = tuner.getKp();
  ki = tuner.getKi();
  kd = tuner.getKd();

  Serial.printf("Tuned PID values: Kp=%.3f, Ki=%.3f, Kd=%.3f\n", kp, ki, kd);
}
