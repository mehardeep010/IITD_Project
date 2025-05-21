#include <WiFi.h>

// ====== Pin Definitions ======
#define SIG_PIN 6  // GPIO connected to Grove SIG pin

// ====== Distance Measurement Variables ======
float distance_cm;
float readings[4] = {0};  // Moving average buffer
int bufIndex = 0;
bool filled = false;
bool measuring = false;

// ====== WiFi Configuration ======
const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

WiFiServer server(80);

// ====== Setup ======
void setup() {
  Serial.begin(115200);

  // Set up initial state of the SIG pin
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delay(2);
  pinMode(SIG_PIN, INPUT);

  // WiFi setup
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

// ====== Main Loop ======
void loop() {
  WiFiClient client = server.available();

  if (client) {
    String request = "";
    unsigned long timeout = millis() + 1000;
    while (client.connected() && millis() < timeout) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (request.indexOf("\r\n\r\n") >= 0) break;
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

    // Serve the HTML UI
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
}, 50); // update every 50ms
</script>
</body></html>
)rawliteral");

    client.stop();
  }
}

// ====== Fast Distance Measurement ======
void triggerUltrasonic() {
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delayMicroseconds(1);
  digitalWrite(SIG_PIN, HIGH);
  delayMicroseconds(5);  // Shorter pulse
  digitalWrite(SIG_PIN, LOW);
  pinMode(SIG_PIN, INPUT);
}

float measureDistance() {
  triggerUltrasonic();

  // Wait for echo to start
  unsigned long startTime = micros();
  while (digitalRead(SIG_PIN) == LOW) {
    if (micros() - startTime > 10000) return -1; // timeout
  }

  // Measure pulse duration
  unsigned long echoStart = micros();
  while (digitalRead(SIG_PIN) == HIGH) {
    if (micros() - echoStart > 10000) return -1; // timeout
  }

  unsigned long duration = micros() - echoStart;
  return duration * 0.0343 / 2;
}

float getAverageDistance() {
  float sum = 0;
  int count = filled ? 4 : bufIndex;
  for (int i = 0; i < count; i++) sum += readings[i];
  return count > 0 ? sum / count : -1;
}
