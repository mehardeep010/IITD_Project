#include <WiFi.h>
#include <Wire.h>
#include <VL53L1X.h>
#include <WebServer.h>

VL53L1X sensor;
const int ledPin = 2;
const int triggerDistance = 200; // 200 mm = 20 cm

// WiFi credentials
const char* ssid = "Redmi Note 13 5G";
const char* password = "abc123xyzmds";

// Web server on port 80
WebServer server(80);

// Variable to hold latest distance
int distance = 0;

// Serve the HTML page
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='1'/><title>VL53L1X Distance</title></head><body>";
  html += "<h1>VL53L1X Sensor</h1>";
  html += "<p>Distance: " + String(distance) + " mm</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi.");
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Connect to Wi-Fi
  connectToWiFi();

  // Start web server
  server.on("/", handleRoot);
  server.begin();

  // Initialize the VL53L1X sensor
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("Sensor not detected!");
    while (1);
  }

  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(50000);
  sensor.startContinuous(50);
}

void loop() {
  distance = sensor.read();

  if (distance > 0 && distance < triggerDistance) {
    digitalWrite(ledPin, HIGH);
  } else {
    digitalWrite(ledPin, LOW);
  }

  server.handleClient(); // Handle web requests

  delay(100);
}
