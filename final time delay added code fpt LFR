// IR Sensors - 3 Sensor Array
#define IR_LEFT 34
#define IR_CENTER 32
#define IR_RIGHT 18

// Motor Driver L298N
#define IN1 25
#define IN2 27
#define IN3 14
#define IN4 19
#define ENA 23
#define ENB 22

// Motor Speed
#define DRIVE_SPEED 60

// Servo
#include <ESP32Servo.h>
#define SERVO_PIN 13

// Soil Moisture Sensor
#define SOIL_MOISTURE_PIN 36

// Relay Module (Active LOW)
#define RELAY_PIN 33

// Soil Moisture Thresholds - FIXED for your values
#define DRY_THRESHOLD 3000 // Dry if reading >= 3000
#define WET_THRESHOLD 2600 // Wet if reading <= 2600
#define NUM_READINGS 10

// Pump timing configuration
#define PUMP_DURATION 5000  // Pump runs for 5 seconds
#define PUMP_DELAY_BEFORE_ON 2000  // Wait 2 seconds before turning pump ON

// Servo Angles
#define SERVO_UP_ANGLE 90
#define SERVO_DOWN_ANGLE 0

Servo myServo;

// Sensor values - ADJUST THESE BASED ON YOUR TEST
bool IR_ON_BLACK = LOW;  // Change if needed
bool IR_ON_WHITE = HIGH; // Change if needed

int irLeft, irCenter, irRight;
bool isPaused = false;
unsigned long pauseStartTime = 0;

// Soil moisture variables
bool checkingMoisture = false;
bool servoDown = false;
unsigned long moistureCheckStartTime = 0;
bool pumpActivated = false;
unsigned long pumpStartTime = 0;

void setup()
{
  Serial.begin(115200);

  pinMode(IR_LEFT, INPUT);
  pinMode(IR_CENTER, INPUT);
  pinMode(IR_RIGHT, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Initialize soil moisture sensor
  pinMode(SOIL_MOISTURE_PIN, INPUT);
  analogReadResolution(12);

  // Initialize relay
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay OFF (Active LOW)

  // Initialize servo
  myServo.attach(SERVO_PIN);
  myServo.write(SERVO_UP_ANGLE); // Servo in UP position

  stopMotors();

  Serial.println("=== SIMPLE LINE FOLLOWER WITH SOIL MOISTURE ===");
  Serial.println("Testing sensors first...");
  Serial.print("Soil moisture thresholds: DRY >= 3000, WET <= 2600");
  Serial.print(" | Pump will start after: ");
  Serial.print(PUMP_DELAY_BEFORE_ON / 1000);
  Serial.print(" seconds | Pump runs for: ");
  Serial.print(PUMP_DURATION / 1000);
  Serial.println(" seconds");
}

void loop()
{
  // Read sensors
  irLeft = digitalRead(IR_LEFT);
  irCenter = digitalRead(IR_CENTER);
  irRight = digitalRead(IR_RIGHT);

  // Print values every 500ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 500)
  {
    Serial.print("L:");
    Serial.print(irLeft);
    Serial.print(" C:");
    Serial.print(irCenter);
    Serial.print(" R:");
    Serial.print(irRight);

    // SIMPLIFIED LOGIC:
    // Move if center sensor is DIFFERENT from outer sensors
    if (irCenter != irLeft && irCenter != irRight)
    {
      Serial.println(" -> MOVING (Center different from sides)");
    }
    else if (irLeft == irCenter && irCenter == irRight)
    {
      Serial.println(" -> ALL SAME (All sensors see same surface)");
    }
    else
    {
      Serial.println(" -> STOPPED");
    }

    lastPrint = millis();
  }

  // Check if paused
  if (isPaused)
  {
    // Handle the 3-second pause with moisture checking
    handlePauseWithMoistureCheck();
    return;
  }

  // SIMPLIFIED MOVEMENT LOGIC:
  // Move when center sensor is different from both outer sensors
  if (irCenter != irLeft && irCenter != irRight)
  {
    // Center is different - likely on the line
    driveForward();

    // NEW: Keep servo at 0 degrees when moving
    if (!servoDown)
    {
      myServo.write(SERVO_DOWN_ANGLE);
      servoDown = true;
    }
  }
  // All three same (all black or all white) - pause
  else if (irLeft == irCenter && irCenter == irRight)
  {
    if (!isPaused)
    {
      Serial.println("ALL SAME - PAUSING 3 SECONDS");
      stopMotors();
      isPaused = true;
      pauseStartTime = millis();

      // Start moisture checking sequence
      checkingMoisture = true;
      servoDown = false;
      pumpActivated = false;
      moistureCheckStartTime = millis();
    }
  }
  else
  {
    stopMotors();
  }

  delay(50);
}

void handlePauseWithMoistureCheck()
{
  unsigned long currentTime = millis();

  // Check if 3 seconds have passed
  if (currentTime - pauseStartTime >= 3000)
  {
    // Time's up - make sure servo is up and resume
    if (servoDown)
    {
      myServo.write(SERVO_UP_ANGLE);
      delay(500); // Wait for servo to move up
      servoDown = false;
    }
    
    // Make sure pump is off before resuming
    if (pumpActivated)
    {
      deactivatePump();
      pumpActivated = false;
    }
    
    Serial.println("RESUMING - 3 seconds complete");
    isPaused = false;
    checkingMoisture = false;
    return;
  }

  // If we're checking moisture during the pause
  if (checkingMoisture)
  {
    // Step 1: Lower servo (at start of pause)
    if (!servoDown)
    {
      if (currentTime - moistureCheckStartTime >= 200)
      { // Small delay before lowering
        Serial.println("Lowering servo to check soil moisture...");
        myServo.write(SERVO_DOWN_ANGLE);
        servoDown = true;
        moistureCheckStartTime = currentTime; // Reset timer for next step
      }
    }
    // Step 2: Read soil moisture (after servo is down)
    else if (servoDown && !pumpActivated)
    {
      if (currentTime - moistureCheckStartTime >= 500)
      { // Wait for servo to settle
        int soilMoisture = readSoilMoisture();
        Serial.print("Soil Moisture Value: ");
        Serial.println(soilMoisture);

        // FIXED LOGIC: Dry if >= 3000, Wet if <= 2600
        if (soilMoisture >= DRY_THRESHOLD)
        {
          Serial.print("Soil is DRY (>=3000) - Will wait ");
          Serial.print(PUMP_DELAY_BEFORE_ON / 1000);
          Serial.println(" seconds before starting pump");
          
          // Start the delay timer for pump
          pumpStartTime = currentTime;
          pumpActivated = true;
        }
        else if (soilMoisture <= WET_THRESHOLD)
        {
          Serial.println("Soil is WET (<=2600) - No watering needed");
          // Raise servo back up
          Serial.println("Raising servo...");
          myServo.write(SERVO_UP_ANGLE);
          delay(500);
          checkingMoisture = false;
        }
        else
        {
          Serial.println("Soil moisture is MODERATE (2601-2999) - No watering needed");
          // Raise servo back up
          Serial.println("Raising servo...");
          myServo.write(SERVO_UP_ANGLE);
          delay(500);
          checkingMoisture = false;
        }
      }
    }
    // Step 3: Handle pump delay and activation
    else if (pumpActivated)
    {
      unsigned long timeSinceDryDetected = currentTime - pumpStartTime;
      
      if (timeSinceDryDetected < PUMP_DELAY_BEFORE_ON)
      {
        // Still waiting before turning pump on
        Serial.print("Waiting ");
        Serial.print((PUMP_DELAY_BEFORE_ON - timeSinceDryDetected) / 1000);
        Serial.println(" seconds before starting pump...");
        delay(100); // Small delay to avoid spamming serial
      }
      else if (timeSinceDryDetected >= PUMP_DELAY_BEFORE_ON && 
               timeSinceDryDetected < (PUMP_DELAY_BEFORE_ON + PUMP_DURATION))
      {
        // Pump should be ON
        if (digitalRead(RELAY_PIN) == LOW) // If pump is not already ON
        {
          Serial.println("DELAY COMPLETE - Starting pump NOW!");
          activatePump();
        }
        
        unsigned long pumpRunningTime = timeSinceDryDetected - PUMP_DELAY_BEFORE_ON;
        Serial.print("Pump running for: ");
        Serial.print(pumpRunningTime / 1000);
        Serial.print(" / ");
        Serial.print(PUMP_DURATION / 1000);
        Serial.println(" seconds");
        delay(100);
      }
      else if (timeSinceDryDetected >= (PUMP_DELAY_BEFORE_ON + PUMP_DURATION))
      {
        // Pump duration complete - turn off and cleanup
        Serial.println("Pump duration complete");
        deactivatePump();
        
        // Raise servo back up
        Serial.println("Raising servo...");
        myServo.write(SERVO_UP_ANGLE);
        delay(500);
        
        pumpActivated = false;
        checkingMoisture = false;
        Serial.println("Moisture check complete");
      }
    }
  }
}

// Function to read soil moisture with averaging
int readSoilMoisture()
{
  long total = 0;
  for (int i = 0; i < NUM_READINGS; i++)
  {
    total += analogRead(SOIL_MOISTURE_PIN);
    delay(10);
  }
  return total / NUM_READINGS;
}

void activatePump()
{
  digitalWrite(RELAY_PIN, HIGH); // Relay ON (Active LOW)
  Serial.println(">>> PUMP IS NOW ON <<<");
}

void deactivatePump()
{
  digitalWrite(RELAY_PIN, LOW); // Relay OFF
  Serial.println(">>> PUMP IS NOW OFF <<<");
}

void driveForward()
{
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, DRIVE_SPEED);
  analogWrite(ENB, DRIVE_SPEED);
}

void stopMotors()
{
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}
