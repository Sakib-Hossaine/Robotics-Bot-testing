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

  // Initialize relay pin - START OFF
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // LOW = OFF for active HIGH relay
  // Relay OFF (Active LOW)

  // Initialize servo
  myServo.attach(SERVO_PIN);
  myServo.write(SERVO_UP_ANGLE); // Servo in UP position

  stopMotors();

  Serial.println("=== SIMPLE LINE FOLLOWER WITH SOIL MOISTURE ===");
  Serial.println("Testing sensors first...");
  Serial.println("Soil moisture thresholds: DRY >= 3000, WET <= 2600");
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
    else if (servoDown && checkingMoisture)
    {
      if (currentTime - moistureCheckStartTime >= 500)
      { // Wait for servo to settle
        int soilMoisture = readSoilMoisture();
        Serial.print("Soil Moisture Value: ");
        Serial.println(soilMoisture);

        // FIXED LOGIC: Dry if >= 3000, Wet if <= 2600
        if (soilMoisture >= DRY_THRESHOLD)
        {
          Serial.println("Soil is DRY (>=3000) - Activating pump for 2 seconds");
          activatePump();
          delay(2000); // Pump runs for 2 seconds
          deactivatePump();
          Serial.println("Pump deactivated");
        }
        else if (soilMoisture <= WET_THRESHOLD)
        {
          Serial.println("Soil is WET - No watering needed");
        }
        else
        {
          Serial.println("Soil moisture is MODERATE - No watering needed");
        }

        // Raise servo back up
        Serial.println("Raising servo...");
        myServo.write(SERVO_UP_ANGLE);
        delay(500); // Wait for servo to move up

        checkingMoisture = false; // Moisture check complete
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
  Serial.println("PUMP ON");
}

void deactivatePump()
{
  digitalWrite(RELAY_PIN, LOW); // Relay OFF
  Serial.println("PUMP OFF");
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