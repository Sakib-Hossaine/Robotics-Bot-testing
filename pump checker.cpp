// Pump Relay Control using ESP32
// Relay connected to pin D23

const int PUMP_RELAY_PIN = 23;  // D23 on ESP32

void setup() {
  Serial.begin(115200);
  
  // Initialize relay pin as output
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  
  // Ensure pump starts OFF
  digitalWrite(PUMP_RELAY_PIN, LOW);
  
  Serial.println("Pump Control System Ready");
  Serial.println("Commands: 'ON' to start pump, 'OFF' to stop pump");
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    
    if (command == "ON") {
      pumpOn();
    } else if (command == "OFF") {
      pumpOff();
    } else {
      Serial.println("Invalid command! Use 'ON' or 'OFF'");
    }
  }
}

void pumpOn() {
  digitalWrite(PUMP_RELAY_PIN, HIGH);
  Serial.println("Pump is ON");
}

void pumpOff() {
  digitalWrite(PUMP_RELAY_PIN, LOW);
  Serial.println("Pump is OFF");
}
