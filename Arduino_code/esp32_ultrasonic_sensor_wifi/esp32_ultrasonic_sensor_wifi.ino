#include <WiFi.h>

#define SIG_PIN 6  // Only 1-pin Grove ultrasonic

long duration;
float distance_cm;
bool measuring = false;

float readings[4] = {0};
int bufIndex = 0;
bool filled = false;

const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delay(2);
  pinMode(SIG_PIN, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.begin();
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    Serial.println("New Client.");
    String request = "";
    unsigned long timeout = millis() + 1000;
    while (client.connected() && millis() < timeout) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (request.indexOf("\r\n\r\n") >= 0) break;
      }
    }

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
        float d = measureDistance();
        if (d > 0) {
          readings[bufIndex] = d;
          bufIndex = (bufIndex + 1) % 4;
          if (bufIndex == 0) filled = true;
        }
        client.println(String(getAverageDistance(), 1));
      } else {
        client.println("Not measuring");
      }

      client.stop();
      return;
    }

    // Send UI HTML
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();

    client.println(R"rawliteral(
<!DOCTYPE html><html><head><title>Fast Grove Sensor</title></head><body>
<h2>Grove Distance Sensor</h2>
<p><button onclick="fetch('/start')">Start</button>
<button onclick="fetch('/stop')">Stop</button></p>
<p>Distance: <span id="distance">--</span> cm</p>
<script>
setInterval(() => {
  fetch('/distance').then(r => r.text()).then(d => {
    document.getElementById('distance').innerText = d;
  });
}, 100); // faster update every 100ms
</script>
</body></html>
)rawliteral");

    client.stop();
    Serial.println("Client disconnected.");
  }
}

float measureDistance() {
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(SIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SIG_PIN, LOW);
  pinMode(SIG_PIN, INPUT);

  // reduce timeout from 30ms to 10ms (max ~1.7m range)
  duration = pulseIn(SIG_PIN, HIGH, 10000);
  if (duration == 0) return -1;
  return duration * 0.0343 / 2;
}

float getAverageDistance() {
  float sum = 0;
  int count = filled ? 4 : bufIndex;
  for (int i = 0; i < count; i++) sum += readings[i];
  return count > 0 ? sum / count : -1;
}