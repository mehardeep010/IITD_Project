q#include <WiFi.h>
#include <ESP32Servo.h>

#define SIG_PIN 6  // Grove ultrasonic sensor
#define ESC_PIN 5  // BLDC ESC control pin

// PID constants
float kp = 1.5;
float ki = 0.4;
float kd = 0.8;

float desired_height = 50.0; // cm
float current_height = 0;
float pid_output = 0;

float previous_error = 0;
float integral = 0;

Servo esc;
int motorPWM = 1000; // microseconds (1000 = min throttle)

float readings[4] = {0};
int bufIndex = 0;
bool filled = false;
bool measuring = false;

const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delay(2);
  pinMode(SIG_PIN, INPUT);

  esc.setPeriodHertz(50);
  esc.attach(ESC_PIN, 1000, 2000);
  esc.writeMicroseconds(1000); // Arm ESC
  delay(3000);

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  server.begin();
}

void loop() {
  if (measuring) {
    current_height = getAverageDistance();
    applyPID();
  }

  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected");
    String request = "";
    unsigned long timeout = millis() + 1000;
    while (client.connected() && millis() < timeout) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (request.indexOf("\r\n\r\n") >= 0) break;
      }
    }

    if (request.indexOf("GET /start") >= 0) measuring = true;
    else if (request.indexOf("GET /stop") >= 0) {
      measuring = false;
      esc.writeMicroseconds(1000); // stop
    } else if (request.indexOf("GET /set?height=") >= 0) {
      int i = request.indexOf("height=") + 7;
      int j = request.indexOf(" ", i);
      desired_height = request.substring(i, j).toFloat();
    }

    // Send webpage
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html\nConnection: close\n\n");
    client.println("<!DOCTYPE html><html><head><title>Height PID</title></head><body>");
    client.println("<h2>Height Control</h2>");
    client.println("<form>Set Desired Height (cm): <input type='number' name='height' step='0.1' min='0' max='100'><input type='submit' value='Set'></form>");
    client.println("<p><a href='/start'>Start</a> | <a href='/stop'>Stop</a></p>");
    client.println("<p>Current Height: " + String(current_height, 1) + " cm</p>");
    client.println("<p>Motor PWM: " + String(motorPWM) + " µs</p>");
    client.println("</body></html>");
    client.stop();
  }
}

void applyPID() {
  float error = desired_height - measureDistance();
  integral += error;
  float derivative = error - previous_error;
  pid_output = kp * error + ki * integral + kd * derivative;

  motorPWM = constrain(1000 + pid_output, 1000, 2000);
  esc.writeMicroseconds(motorPWM);
  previous_error = error;
}

float measureDistance() {
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(SIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SIG_PIN, LOW);
  pinMode(SIG_PIN, INPUT);

  long duration = pulseIn(SIG_PIN, HIGH, 6000);
  if (duration == 0) return -1;
  float dist = duration * 0.0343 / 2;
  readings[bufIndex] = dist;
  bufIndex = (bufIndex + 1) % 4;
  if (bufIndex == 0) filled = true;
  return dist;
}

float getAverageDistance() {
  float sum = 0;
  int count = filled ? 4 : bufIndex;
  for (int i = 0; i < count; i++) sum += readings[i];
  return count > 0 ? sum / count : -1;
}
