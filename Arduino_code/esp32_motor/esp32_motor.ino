#include <ESP32Servo.h>

Servo esc_signal;
int speedValue;

void setup()
{
  Serial.begin(115200);

  esc_signal.setPeriodHertz(50);  // 50Hz for ESC
  esc_signal.attach(5, 1000, 2000);  // GPIO13, min/max µs for ESC

  esc_signal.writeMicroseconds(1000);  // Arming signal
  delay(3000);

  Serial.println("Enter speed (0-200) or type 's' to stop the motor:");
}

void loop()
{
  if (Serial.available() > 0)
  {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("s"))
    {
      esc_signal.writeMicroseconds(1000);
      Serial.println("Motor stopped.");
    }
    else
    {
      speedValue = input.toInt();
      if (speedValue >= 0 && speedValue <= 200)
      {
        int pulse = map(speedValue, 0, 200, 1000, 1800);
        esc_signal.writeMicroseconds(pulse);
        Serial.print("Speed set to ");
        Serial.print(speedValue);
        Serial.print(" (PWM: ");
        Serial.print(pulse);
        Serial.println(" µs)");
      }
      else
      {
        Serial.println("Invalid input! Enter 0-200 or 's' to stop.");
      }
    }
  }
}
