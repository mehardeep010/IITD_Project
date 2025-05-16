#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {} // Wait for serial to be ready
  
  // Use the SAME pin configuration as your main sketch
  Wire.begin(4,5);  // SDA = D2 (GPIO4), SCL = D1 (GPIO5)
  
  Serial.println("\nI2C Scanner");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  Serial.println("Scanning...");
  
  for(address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println();
      nDevices++;
    } else if (error == 4) {
      Serial.print("Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println();
    }
  }
  
  if (nDevices == 0) {
    Serial.println("No I2C devices found");
  } else {
    Serial.print("Found ");
    Serial.print(nDevices);
    Serial.println(" device(s)");
  }

  // Also output the expected I2C address for VL53L1X
  Serial.println("VL53L1X default address is 0x29");
  
  delay(5000); // Wait 5 seconds before scanning again
}
