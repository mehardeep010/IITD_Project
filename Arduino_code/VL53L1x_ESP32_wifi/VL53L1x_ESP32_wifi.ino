#include <WiFi.h>
#include <Wire.h>
#include <VL53L1X.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 2

VL53L1X sensor;
bool measuring = false;

// Circular buffer for averaging
int readings[4] = {0};
int bufIndex = 0;
bool filled = false;

const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  sensor.setTimeout(500);
  if (!sensor.init()) {
    Serial.println("VL53L1X sensor not detected!");
    while (1); // Halt if sensor not found
  }

  sensor.setDistanceMode(VL53L1X::Long);
  sensor.setMeasurementTimingBudget(50000);
  sensor.startContinuous(50);

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client.");
    String request = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (c == '\n' && request.indexOf("\r\n\r\n") != -1) {
          break;
        }
      }
    }

    // Handle commands
    if (request.indexOf("GET /start") >= 0) {
      measuring = true;
    } else if (request.indexOf("GET /stop") >= 0) {
      measuring = false;
    } else if (request.indexOf("GET /distance") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println("Connection: close");
      client.println();

      if (measuring) {
        int d = sensor.read();
        if (sensor.timeoutOccurred()) d = -1;

        if (d > 0) {
          readings[bufIndex] = d;
          bufIndex = (bufIndex + 1) % 4;
          if (bufIndex == 0) filled = true;
        }
        client.println(String(getAverageDistance()));
      } else {
        client.println("Not measuring");
      }

      client.stop();
      return;
    }

    // HTML page
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();

    client.println("<!DOCTYPE html><html><head><title>VL53L1X Sensor</title></head><body>");
    client.println("<h2>VL53L1X Distance Sensor</h2>");
    client.println("<p><button onclick=\"fetch('/start')\">Start</button>");
    client.println("<button onclick=\"fetch('/stop')\">Stop</button></p>");
    client.println("<p>Distance: <span id='distance'>--</span> mm</p>");
    client.println("<script>");
    client.println("setInterval(() => {");
    client.println("fetch('/distance').then(r => r.text()).then(d => {");
    client.println("document.getElementById('distance').innerText = d;");
    client.println("});");
    client.println("}, 300);");
    client.println("</script>");
    client.println("</body></html>");
    client.println();

    client.stop();
    Serial.println("Client disconnected.");
  }

  // Optional: basic LED feedback (e.g., turn on when object is within 200 mm)
  int dist = sensor.read();
  if (!sensor.timeoutOccurred() && dist > 0 && dist < 200) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

int getAverageDistance() {
  int sum = 0;
  int count = filled ? 4 : bufIndex;
  for (int i = 0; i < count; i++) {
    sum += readings[i];
  }
  return count > 0 ? sum / count : -1;
}
