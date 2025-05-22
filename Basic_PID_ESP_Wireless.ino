#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// === WiFi Configuration ===
const char* ssid = "";     // Replace with your WiFi name
const char* password = ""; // Replace with your WiFi password

// === Pin Configuration ===
#define SIG_PIN 6            // Grove ultrasonic SIG pin
#define ESC_PIN 5            // ESC control pin (PWM via Servo library)

// === Control Parameters ===
float Kp = 20.0;             // Proportional gain (tune for stability)
float desiredHeight = 0.0;   // Target hover height in cm
int minPWM = 1100;
int maxPWM = 1800;
bool systemArmed = false;    // Safety flag

// === ESC Object ===
Servo esc_signal;

// === Web Server ===
WebServer server(80);

// === Distance Buffer for Averaging ===
float readings[4] = {0};
int bufIndex = 0;
bool filled = false;

// === Global Variables for Web Interface ===
float currentHeight = 0.0;
float currentError = 0.0;
int currentPWM = minPWM;
unsigned long lastUpdate = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("WiFi-Enabled Unicopter Hover Controller");
  
  // Initialize ESC
  esc_signal.setPeriodHertz(50);
  esc_signal.attach(ESC_PIN, minPWM, maxPWM);
  esc_signal.writeMicroseconds(minPWM);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Setup web server routes
  setupWebServer();
  
  // Start the server
  server.begin();
  Serial.println("Web server started");
  Serial.println("Open your browser and go to: http://" + WiFi.localIP().toString());
  
  delay(3000); // ESC arming delay
}

void loop() {
  server.handleClient(); // Handle web requests
  
  static unsigned long lastTime = 0;
  if (millis() - lastTime >= 100) { // Run control loop every 100ms
    lastTime = millis();
    lastUpdate = millis();
    
    currentHeight = getAverageDistance();
    if (currentHeight < 0) return; // Invalid reading
    
    currentError = desiredHeight - currentHeight;
    
    if (systemArmed && desiredHeight > 0) {
      float pwmValue = constrain(minPWM + Kp * currentError, minPWM, maxPWM);
      currentPWM = (int)pwmValue;
      esc_signal.writeMicroseconds(currentPWM);
    } else {
      currentPWM = minPWM;
      esc_signal.writeMicroseconds(minPWM);
    }
    
    // Print info for serial monitor
    Serial.print("Height(cm): ");
    Serial.print(currentHeight, 1);
    Serial.print("\tError: ");
    Serial.print(currentError, 1);
    Serial.print("\tPWM: ");
    Serial.print(currentPWM);
    Serial.print("\tArmed: ");
    Serial.println(systemArmed ? "YES" : "NO");
  }
  
  // Update distance buffer
  float d = measureDistance();
  if (d > 0) {
    readings[bufIndex] = d;
    bufIndex = (bufIndex + 1) % 4;
    if (bufIndex == 0) filled = true;
  }
}

void setupWebServer() {
  // Serve the main HTML page
  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><title>Unicopter Controller</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "body { font-family: Arial; margin: 40px; background: #f0f0f0; }";
    html += ".container { max-width: 600px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }";
    html += "h1 { color: #333; text-align: center; }";
    html += ".control-group { margin: 20px 0; padding: 15px; background: #f9f9f9; border-radius: 5px; }";
    html += ".status { background: #e8f5e8; border-left: 4px solid #4CAF50; padding: 15px; margin: 20px 0; }";
    html += ".emergency { background: #ffe8e8; border-left: 4px solid #f44336; padding: 15px; margin: 20px 0; }";
    html += "input[type='number'] { width: 100px; padding: 8px; margin: 5px; border: 1px solid #ddd; border-radius: 4px; }";
    html += "button { padding: 12px 20px; margin: 5px; border: none; border-radius: 5px; cursor: pointer; font-size: 16px; }";
    html += ".btn-primary { background: #4CAF50; color: white; }";
    html += ".btn-danger { background: #f44336; color: white; }";
    html += ".btn-warning { background: #ff9800; color: white; }";
    html += ".btn-secondary { background: #6c757d; color: white; }";
    html += ".data-display { font-family: monospace; background: #f5f5f5; padding: 15px; border-radius: 5px; margin: 10px 0; }";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>Unicopter Controller</h1>";
    html += "<div class='emergency'>";
    html += "<h3>Emergency Controls</h3>";
    html += "<button class='btn-danger' onclick='emergencyStop()'>EMERGENCY STOP</button>";
    html += "<button class='btn-warning' onclick='disarm()'>DISARM</button>";
    html += "</div>";
    html += "<div class='control-group'>";
    html += "<h3>System Control</h3>";
    html += "<button class='btn-primary' onclick='armSystem()'>ARM SYSTEM</button>";
    html += "<button class='btn-secondary' onclick='updateStatus()'>REFRESH STATUS</button>";
    html += "</div>";
    html += "<div class='control-group'>";
    html += "<h3>Height Control</h3>";
    html += "<label>Target Height (cm): </label>";
    html += "<input type='number' id='heightInput' value='0' min='0' max='200' step='0.1'>";
    html += "<button class='btn-primary' onclick='setHeight()'>Set Height</button>";
    html += "</div>";
    html += "<div class='control-group'>";
    html += "<h3>PID Tuning</h3>";
    html += "<label>Kp Gain: </label>";
    html += "<input type='number' id='kpInput' value='20.0' min='0' max='100' step='0.1'>";
    html += "<button class='btn-primary' onclick='setKp()'>Update Kp</button>";
    html += "</div>";
    html += "<div class='status'>";
    html += "<h3>System Status</h3>";
    html += "<div class='data-display' id='statusDisplay'>Loading...</div>";
    html += "</div></div>";
    html += "<script>";
    html += "function updateStatus() {";
    html += "fetch('/status').then(response => response.json()).then(data => {";
    html += "document.getElementById('statusDisplay').innerHTML = ";
    html += "'Armed: ' + (data.armed ? 'YES' : 'NO') + '<br>' +";
    html += "'Current Height: ' + data.height.toFixed(1) + ' cm<br>' +";
    html += "'Target Height: ' + data.target.toFixed(1) + ' cm<br>' +";
    html += "'Error: ' + data.error.toFixed(1) + ' cm<br>' +";
    html += "'PWM Output: ' + data.pwm + '<br>' +";
    html += "'Kp Gain: ' + data.kp.toFixed(1) + '<br>' +";
    html += "'Last Update: ' + new Date().toLocaleTimeString();";
    html += "});}";
    html += "function armSystem() { fetch('/arm', {method: 'POST'}).then(() => updateStatus()); }";
    html += "function disarm() { fetch('/disarm', {method: 'POST'}).then(() => updateStatus()); }";
    html += "function emergencyStop() {";
    html += "if(confirm('Are you sure you want to emergency stop?')) {";
    html += "fetch('/emergency', {method: 'POST'}).then(() => updateStatus());";
    html += "}}";
    html += "function setHeight() {";
    html += "const height = document.getElementById('heightInput').value;";
    html += "fetch('/setHeight', {";
    html += "method: 'POST',";
    html += "headers: {'Content-Type': 'application/json'},";
    html += "body: JSON.stringify({height: parseFloat(height)})";
    html += "}).then(() => updateStatus());";
    html += "}";
    html += "function setKp() {";
    html += "const kp = document.getElementById('kpInput').value;";
    html += "fetch('/setKp', {";
    html += "method: 'POST',";
    html += "headers: {'Content-Type': 'application/json'},";
    html += "body: JSON.stringify({kp: parseFloat(kp)})";
    html += "}).then(() => updateStatus());";
    html += "}";
    html += "setInterval(updateStatus, 2000);";
    html += "updateStatus();";
    html += "</script></body></html>";
    server.send(200, "text/html", html);
  });
  
  // API endpoints
  server.on("/status", HTTP_GET, []() {
    DynamicJsonDocument doc(1024);
    doc["armed"] = systemArmed;
    doc["height"] = currentHeight;
    doc["target"] = desiredHeight;
    doc["error"] = currentError;
    doc["pwm"] = currentPWM;
    doc["kp"] = Kp;
    doc["timestamp"] = lastUpdate;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  });
  
  server.on("/arm", HTTP_POST, []() {
    systemArmed = true;
    Serial.println("System ARMED via web interface");
    server.send(200, "text/plain", "System Armed");
  });
  
  server.on("/disarm", HTTP_POST, []() {
    systemArmed = false;
    esc_signal.writeMicroseconds(minPWM);
    Serial.println("System DISARMED via web interface");
    server.send(200, "text/plain", "System Disarmed");
  });
  
  server.on("/emergency", HTTP_POST, []() {
    systemArmed = false;
    desiredHeight = 0.0;
    esc_signal.writeMicroseconds(minPWM);
    Serial.println("EMERGENCY STOP activated via web interface");
    server.send(200, "text/plain", "Emergency Stop Activated");
  });
  
  server.on("/setHeight", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, server.arg("plain"));
      desiredHeight = doc["height"];
      Serial.print("Target height set to: ");
      Serial.print(desiredHeight);
      Serial.println(" cm via web interface");
      server.send(200, "text/plain", "Height Set");
    } else {
      server.send(400, "text/plain", "Invalid Request");
    }
  });
  
  server.on("/setKp", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, server.arg("plain"));
      Kp = doc["kp"];
      Serial.print("Kp gain set to: ");
      Serial.println(Kp);
      server.send(200, "text/plain", "Kp Updated");
    } else {
      server.send(400, "text/plain", "Invalid Request");
    }
  });
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
  long duration = pulseIn(SIG_PIN, HIGH, 10000);
  if (duration == 0) return -1;
  return duration * 0.0343 / 2.0;
}

float getAverageDistance() {
  float sum = 0;
  int count = filled ? 4 : bufIndex;
  for (int i = 0; i < count; i++) sum += readings[i];
  return (count > 0) ? sum / count : -1;
}
