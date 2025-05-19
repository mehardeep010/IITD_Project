#include <WiFi.h>

#define ESC_PIN 13
#define ESC_PWM_CHANNEL 0
#define ESC_PWM_FREQ 50 // 50 Hz for servo/ESC control
#define ESC_PWM_RESOLUTION 16 // 16-bit resolution (0 - 65535)

int speedValue = 1000; // microseconds
WiFiServer server(80);

// Replace with your WiFi credentials
const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

// Convert microseconds (1000-2000) to PWM duty cycle
int microsecondsToDuty(int us) {
  // 5% of 65535 = ~3276, 10% = ~6553
  return map(us, 1000, 2000, 3276, 6553);
}

void setup() {
  Serial.begin(115200);

  // PWM setup
  ledcSetup(ESC_PWM_CHANNEL, ESC_PWM_FREQ, ESC_PWM_RESOLUTION);
  ledcAttachPin(ESC_PIN, ESC_PWM_CHANNEL);
  ledcWrite(ESC_PWM_CHANNEL, microsecondsToDuty(1000)); // Arm ESC at min throttle
  delay(3000); // ESC arming delay

  // WiFi setup
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
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
    String currentLine = "";
    String request = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Send web page
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            client.println("<!DOCTYPE html><html><head><title>ESC Control</title></head><body>");
            client.println("<h2>ESC Speed Control</h2>");
            client.println("<input type='range' min='40' max='130' value='" + String(speedValue) + "' id='speedSlider' oninput='setSpeed(this.value)'/>");
            client.println("<p>Speed: <span id='val'>" + String(speedValue) + "</span></p>");
            client.println("<br><a href=\"/stop\">Stop Motor</a>");

            client.println("<script>");
            client.println("function setSpeed(val) {");
            client.println("document.getElementById('val').innerText = val;");
            client.println("var xhr = new XMLHttpRequest();");
            client.println("xhr.open('GET', '/set?speed=' + val, true);");
            client.println("xhr.send();");
            client.println("}");
            client.println("</script>");

            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }

    // Handle speed set
    if (request.indexOf("GET /set?speed=") >= 0) {
      int index = request.indexOf("speed=") + 6;
      int endIndex = request.indexOf(" ", index);
      String valueStr = request.substring(index, endIndex);
      speedValue = valueStr.toInt();
      if (speedValue >= 40 && speedValue <= 130) {
        int pulse = map(speedValue, 40, 130, 1000, 2000);
        ledcWrite(ESC_PWM_CHANNEL, microsecondsToDuty(pulse));
        Serial.printf("Speed set to %d (PWM: %d µs)\n", speedValue, pulse);
      }
    }

    if (request.indexOf("GET /stop") >= 0) {
      ledcWrite(ESC_PWM_CHANNEL, microsecondsToDuty(1000));
      Serial.println("Motor stopped.");
    }

    client.stop();
    Serial.println("Client Disconnected.");
  }
}
