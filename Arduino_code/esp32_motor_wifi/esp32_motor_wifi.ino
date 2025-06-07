#include <WiFi.h>
#include <ESP32Servo.h>

Servo esc_signal;
int speedValue = 1000;

const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);

  esc_signal.setPeriodHertz(50);
  esc_signal.attach(5, 1000, 2000); // PWM pin 5
  esc_signal.writeMicroseconds(1000); // Arm ESC
  delay(3000);

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
            // Send web page with slider
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            client.println("<!DOCTYPE html><html><head><title>ESC Control</title></head><body>");
            client.println("<h2>ESC Speed Control</h2>");
            client.println("<input type='range' min='0' max='200' value='" + String(speedValue) + "' id='speedSlider' oninput='setSpeed(this.value)'/>");
            client.println("<p>Speed: <span id='val'>" + String(speedValue) + "</span></p>");
            client.println("<br><a href=\"/stop\">Stop Motor</a>");

            // JavaScript to send GET request and update value display
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

    // Handle slider speed setting
    if (request.indexOf("GET /set?speed=") >= 0) {
      int index = request.indexOf("speed=") + 6;
      int endIndex = request.indexOf(" ", index);
      String valueStr = request.substring(index, endIndex);
      speedValue = valueStr.toInt();
      if (speedValue >= 0 && speedValue <= 200) {
      int pulse = map(speedValue, 0, 200, 1000, 1700);
      esc_signal.writeMicroseconds(pulse);
      Serial.printf("Speed set to %d (PWM: %d µs)\n", speedValue, pulse);
    }

    }

    if (request.indexOf("GET /stop") >= 0) {
      esc_signal.writeMicroseconds(1000);
      Serial.println("Motor stopped.");
    }

    client.stop();
    Serial.println("Client Disconnected.");
  }
  
}