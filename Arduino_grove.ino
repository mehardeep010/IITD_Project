#include <WiFi.h>
#include <ESP32Servo.h>

// Grove ultrasonic sensor using single SIG pin
#define SIG_PIN 6

// ESC PWM control pin
#define ESC_PIN 5

// Constants
#define MIN_THROTTLE 1000     // Minimum throttle value (motor off)
#define MAX_THROTTLE 2000     // Maximum throttle value
#define HOVER_THROTTLE 1700   // Higher base throttle value for hovering
#define TAKEOFF_THROTTLE 1850 // Initial throttle for takeoff

long duration;
float currentHeight = 0;
float desiredHeight = 120.0; // Default target height in cm (1.2 meters)

// PID control variables
float kp = 5.0;  // Proportional gain
float ki = 0.15; // Integral gain
float kd = 2.0;  // Derivative gain
float previousError = 0;
float integral = 0;
unsigned long lastTime = 0;
float deltaTime = 0;

int escPwm = MIN_THROTTLE; // ESC PWM in microseconds (1000-2000)

Servo esc_signal;

// WiFi credentials
const char* ssid = "Galaxy F23 5G5C9C";
const char* password = "sbgj0799";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  delay(500); // Short delay for serial to initialize
  
  Serial.println("\n\n***** Starting Height Control System *****");
  
  // ESC setup first
  Serial.println("Initializing ESC...");
  esc_signal.setPeriodHertz(50);
  esc_signal.attach(ESC_PIN, MIN_THROTTLE, MAX_THROTTLE);
  esc_signal.writeMicroseconds(MIN_THROTTLE); // Arm ESC at minimum throttle
  Serial.println("Arming ESC...");
  delay(3000); // Give time for ESC to initialize
  
  // Ultrasonic sensor setup
  Serial.println("Setting up ultrasonic sensor...");
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delay(2);
  
  // WiFi connection with better feedback
  Serial.println("Connecting to WiFi network: " + String(ssid));
  
  // Disconnect first (if previously connected)
  WiFi.disconnect();
  delay(1000);
  
  // Set WiFi mode and begin connection
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  // Connection timeout counter
  int timeout_counter = 0;
  while (WiFi.status() != WL_CONNECTED && timeout_counter < 30) {
    delay(1000);
    Serial.print(".");
    timeout_counter++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal strength: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("\nWiFi connection FAILED!");
    Serial.println("Check your WiFi credentials or network availability");
    Serial.println("Will continue without WiFi. You can still control via Serial monitor.");
  }
  
  // Start web server regardless of WiFi status
  server.begin();
  Serial.println("Web server started");
  
  lastTime = millis();
  
  // Initial takeoff boost if needed
  Serial.println("Applying initial takeoff power...");
  escPwm = TAKEOFF_THROTTLE;
  esc_signal.writeMicroseconds(escPwm);
  delay(1000); // Short burst of higher power to start lifting
  
  Serial.println("System initialized and ready!");
  Serial.println("Current target height: " + String(desiredHeight) + " cm");
  Serial.println("Type 'help' for available commands");
}

void loop() {
  // Calculate time difference for integral and derivative calculations
  unsigned long currentTime = millis();
  deltaTime = (currentTime - lastTime) / 1000.0; // Convert to seconds
  lastTime = currentTime;
  
  // Measure current height
  currentHeight = measureDistance();
  
  // Only apply PID if we got a valid reading
  if (currentHeight > 0) {
    applyPID();
    
    // Debug output every 500ms
    static unsigned long lastDisplayTime = 0;
    if (currentTime - lastDisplayTime > 500) {
      lastDisplayTime = currentTime;
      Serial.print("Height: ");
      Serial.print(currentHeight);
      Serial.print(" cm, Target: ");
      Serial.print(desiredHeight);
      Serial.print(" cm, PWM: ");
      Serial.println(escPwm);
    }
    
    // Periodically check WiFi status
    static unsigned long lastWifiCheck = 0;
    if (currentTime - lastWifiCheck > 10000) { // Every 10 seconds
      lastWifiCheck = currentTime;
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("WiFi connected, IP: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("WiFi disconnected, trying to reconnect...");
        WiFi.reconnect();
      }
    }
  } else {
    Serial.println("Invalid distance reading!");
  }
  
  // Handle web interface if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    handleWebInterface();
  }
  
  // Check for commands via Serial
  checkSerialCommands();
  
  // Small delay to stabilize readings
  delay(50);
}

float measureDistance() {
  // Clear the trigger pin
  pinMode(SIG_PIN, OUTPUT);
  digitalWrite(SIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Send 10μs pulse to trigger the sensor
  digitalWrite(SIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SIG_PIN, LOW);
  
  // Read the echo
  pinMode(SIG_PIN, INPUT);
  duration = pulseIn(SIG_PIN, HIGH, 30000); // 30ms timeout (max ~5m)
  
  // Calculate distance
  if (duration == 0) return -1; // Invalid reading
  
  // Convert to cm (speed of sound = 343 m/s = 0.0343 cm/μs)
  float distance = duration * 0.0343 / 2;
  
  // Filter out unreasonable values
  if (distance > 400 || distance < 2) {
    return -1; // Invalid range
  }
  
  return distance;
}

void applyPID() {
  // Calculate error
  float error = desiredHeight - currentHeight;
  
  // Calculate integral with anti-windup
  integral += error * deltaTime;
  integral = constrain(integral, -100, 100); // Prevent excessive integral buildup
  
  // Calculate derivative
  float derivative = 0;
  if (deltaTime > 0) {
    derivative = (error - previousError) / deltaTime;
  }
  previousError = error;
  
  // Calculate PID output
  float output = kp * error + ki * integral + kd * derivative;
  
  // Apply PID output to throttle, starting from hovering point
  escPwm = HOVER_THROTTLE + output;
  
  // Apply minimum power to ensure lift
  if (escPwm < 1600 && currentHeight < (desiredHeight - 10)) {
    escPwm = 1600; // Minimum power when significantly below target
  }
  
  // Constrain throttle to valid range
  escPwm = constrain(escPwm, MIN_THROTTLE, MAX_THROTTLE);
  
  // Send command to ESC
  esc_signal.writeMicroseconds(escPwm);
}

void handleWebInterface() {
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
    
    // Handle height setting request
    if (request.indexOf("GET /set?height=") >= 0) {
      int index = request.indexOf("height=") + 7;
      int endIndex = request.indexOf(" ", index);
      if (endIndex == -1) endIndex = request.length();
      String valueStr = request.substring(index, endIndex);
      float newHeight = valueStr.toFloat();
      
      // Validate input
      if (newHeight >= 10 && newHeight <= 300) {
        desiredHeight = newHeight;
        Serial.printf("Target height updated: %.1f cm\n", desiredHeight);
      }
    }
    
    // Handle manual throttle override
    if (request.indexOf("GET /manual?throttle=") >= 0) {
      int index = request.indexOf("throttle=") + 9;
      int endIndex = request.indexOf(" ", index);
      if (endIndex == -1) endIndex = request.length();
      String valueStr = request.substring(index, endIndex);
      int manualThrottle = valueStr.toInt();
      
      if (manualThrottle >= MIN_THROTTLE && manualThrottle <= MAX_THROTTLE) {
        escPwm = manualThrottle;
        esc_signal.writeMicroseconds(escPwm);
        Serial.printf("Manual throttle override: %d µs\n", escPwm);
      }
    }
    
    // Handle reset request
    if (request.indexOf("GET /reset") >= 0) {
      integral = 0;
      Serial.println("PID integral reset");
    }
    
    // Handle PID tuning parameters if needed
    if (request.indexOf("GET /tune?") >= 0) {
      // Parse kp
      if (request.indexOf("kp=") >= 0) {
        int index = request.indexOf("kp=") + 3;
        int endIndex = request.indexOf("&", index);
        if (endIndex == -1) endIndex = request.indexOf(" ", index);
        if (endIndex == -1) endIndex = request.length();
        String valueStr = request.substring(index, endIndex);
        kp = valueStr.toFloat();
      }
      
      // Parse ki
      if (request.indexOf("ki=") >= 0) {
        int index = request.indexOf("ki=") + 3;
        int endIndex = request.indexOf("&", index);
        if (endIndex == -1) endIndex = request.indexOf(" ", index);
        if (endIndex == -1) endIndex = request.length();
        String valueStr = request.substring(index, endIndex);
        ki = valueStr.toFloat();
      }
      
      // Parse kd
      if (request.indexOf("kd=") >= 0) {
        int index = request.indexOf("kd=") + 3;
        int endIndex = request.indexOf("&", index);
        if (endIndex == -1) endIndex = request.indexOf(" ", index);
        if (endIndex == -1) endIndex = request.length();
        String valueStr = request.substring(index, endIndex);
        kd = valueStr.toFloat();
      }
      
      Serial.printf("PID parameters updated: kp=%.2f, ki=%.2f, kd=%.2f\n", kp, ki, kd);
      integral = 0; // Reset integral when tuning changes
    }
    
    // Send enhanced webpage with more data and PID tuning
    client.println("HTTP/1.1 200 OK");
    client.println("Content-type:text/html");
    client.println("Connection: close");
    client.println();
    
    client.println(R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Height PID Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial; margin: 20px; }
    .container { max-width: 600px; margin: 0 auto; }
    .card { background: #f8f9fa; border-radius: 8px; padding: 15px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
    h2 { color: #0066cc; }
    input, button { padding: 8px; margin: 5px 0; }
    .value { font-weight: bold; font-size: 1.2em; }
    .status { display: flex; justify-content: space-between; }
  </style>
</head>
<body>
  <div class="container">
    <h2>Ultrasonic Height PID Control</h2>
    
    <div class="card">
      <h3>Height Control</h3>
      <form action="/set" method="get">
        <label>Desired Height (cm): 
          <input type="number" name="height" step="1" min="10" max="300" value=")rawliteral" + String(desiredHeight, 1) + R"rawliteral(">
        </label>
        <button type="submit">Set Height</button>
      </form>
      
      <div class="status">
        <p>Current Height: <span class="value">)rawliteral" + String(currentHeight, 1) + R"rawliteral( cm</span></p>
        <p>Motor Power: <span class="value">)rawliteral" + String(map(escPwm, MIN_THROTTLE, MAX_THROTTLE, 0, 100)) + R"rawliteral(%</span></p>
      </div>
    </div>
    
    <div class="card">
      <h3>PID Tuning</h3>
      <form action="/tune" method="get">
        <div>
          <label>Kp: <input type="number" name="kp" step="0.1" value=")rawliteral" + String(kp, 2) + R"rawliteral("></label>
        </div>
        <div>
          <label>Ki: <input type="number" name="ki" step="0.01" value=")rawliteral" + String(ki, 2) + R"rawliteral("></label>
        </div>
        <div>
          <label>Kd: <input type="number" name="kd" step="0.1" value=")rawliteral" + String(kd, 2) + R"rawliteral("></label>
        </div>
        <button type="submit">Update PID</button>
      </form>
      <form action="/reset" method="get" style="margin-top: 10px;">
        <button type="submit">Reset Integral</button>
      </form>
    </div>
    
    <div class="card">
      <h3>Manual Throttle Override</h3>
      <form action="/manual" method="get">
        <label>Manual Throttle (1000-2000): 
          <input type="number" name="throttle" step="10" min="1000" max="2000" value=")rawliteral" + String(escPwm) + R"rawliteral(">
        </label>
        <button type="submit">Apply</button>
      </form>
      <small>Use with caution! This will override PID control.</small>
    </div>
    
    <div class="card">
      <p>Error: <span class="value">)rawliteral" + String(desiredHeight - currentHeight, 1) + R"rawliteral( cm</span></p>
      <p>PWM Value: <span class="value">)rawliteral" + String(escPwm) + R"rawliteral( µs</span></p>
    </div>
    
    <p><small>IP: )rawliteral" + WiFi.localIP().toString() + R"rawliteral(</small></p>
  </div>
  <script>
    setTimeout(function(){ window.location.reload(); }, 2000);
  </script>
</body>
</html>
)rawliteral");
    
    client.stop();
    Serial.println("Client disconnected.");
  }
}

void checkSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    // Command to set height: "h:120"
    if (command.startsWith("h:")) {
      float newHeight = command.substring(2).toFloat();
      if (newHeight >= 10 && newHeight <= 300) {
        desiredHeight = newHeight;
        Serial.println("Target height set to: " + String(desiredHeight) + " cm");
      }
    }
    // Command to set throttle manually: "t:1800"
    else if (command.startsWith("t:")) {
      int newThrottle = command.substring(2).toInt();
      if (newThrottle >= MIN_THROTTLE && newThrottle <= MAX_THROTTLE) {
        escPwm = newThrottle;
        esc_signal.writeMicroseconds(escPwm);
        Serial.println("Manual throttle set to: " + String(escPwm));
      }
    }
    // Command to show IP address: "ip"
    else if (command == "ip") {
      if (WiFi.status() == WL_CONNECTED) {
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
      } else {
        Serial.println("WiFi not connected");
      }
    }
    // Command to reconnect WiFi: "wifi"
    else if (command == "wifi") {
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(ssid, password);
    }
    // Command to show status: "status"
    else if (command == "status") {
      Serial.println("=== System Status ===");
      Serial.println("Current height: " + String(currentHeight) + " cm");
      Serial.println("Target height: " + String(desiredHeight) + " cm");
      Serial.println("Current throttle: " + String(escPwm) + " µs");
      Serial.println("WiFi status: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected"));
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("IP: " + WiFi.localIP().toString());
      }
      Serial.println("PID values - Kp: " + String(kp) + ", Ki: " + String(ki) + ", Kd: " + String(kd));
    }
    // Command to show help: "help"
    else if (command == "help") {
      Serial.println("Available commands:");
      Serial.println("  h:X - Set target height to X cm (e.g. h:120)");
      Serial.println("  t:X - Set throttle to X µs (e.g. t:1800)");
      Serial.println("  ip - Show IP address");
      Serial.println("  wifi - Reconnect WiFi");
      Serial.println("  status - Show system status");
      Serial.println("  help - Show this help text");
    }
  }
}
