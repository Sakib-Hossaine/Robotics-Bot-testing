#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ================= WIFI =================
const char* ssid = "Trisha Fire Service";
const char* password = "123456789";

// ================= FLAME SENSORS =================
#define FLAME_LEFT_PIN   34
#define FLAME_RIGHT_PIN  35
#define RELAY_PIN        4

// ================= ULTRASONIC =================
#define TRIG_PIN 21
#define ECHO_PIN 19

// ================= MOTOR DRIVER =================
#define IN1 26
#define IN2 27
#define ENA 32

#define IN3 14
#define IN4 12
#define ENB 33

// ================= DHT11 =================
#define DHT_PIN 13
#define DHT_TYPE DHT11

// ================= SERVO =================
#define SERVO_PIN 18

// ================= PWM =================
#define PWM_CHANNEL_A 0
#define PWM_CHANNEL_B 1
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// ================= FIRE SETTINGS =================
int flameThreshold = 500;

// ================= GLOBAL =================
WebServer server(80);
DHT dht(DHT_PIN, DHT_TYPE);
Servo flameServo;

int currentSpeed = 250;

unsigned long lastUltrasonicRead = 0;
unsigned long lastFlameRead = 0;
unsigned long lastTemperatureRead = 0;

const int ultrasonicInterval = 50;
const int flameInterval = 100;
const int temperatureInterval = 2000;

float distance = 0;
float temperature = 0;

int servoPosition = 90;

bool fireDetected = false;
String fireLocation = "NONE";

bool pumpState = false;

int flameLeftValue = 1023;
int flameRightValue = 1023;

// ================= FLAME DIRECTION =================
enum FlameDirection {
  NO_FLAME,
  FLAME_LEFT_SIDE,
  FLAME_RIGHT_SIDE,
  FLAME_BOTH_SIDES
};

// ================= WEB PAGE =================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>ESP32 Fire Robot</title>

<style>
body{
    font-family:Arial;
    background:#111;
    color:white;
    text-align:center;
    padding:20px;
}

.card{
    background:#222;
    padding:20px;
    border-radius:15px;
    margin:15px auto;
    max-width:400px;
}

.value{
    font-size:30px;
    font-weight:bold;
}

.fire{
    color:red;
    animation:blink 0.5s infinite;
}

@keyframes blink{
    50%{opacity:0.5;}
}
</style>
</head>

<body>

<h1>🔥 ESP32 Fire Fighting Robot</h1>

<div class="card">
<h2>Temperature</h2>
<div class="value" id="temp">0</div>
</div>

<div class="card">
<h2>Distance</h2>
<div class="value" id="dist">0</div>
</div>

<div class="card">
<h2>Fire Status</h2>
<div class="value fire" id="fire">NO FIRE</div>
</div>

<div class="card">
<h2>Flame Direction</h2>
<div class="value" id="direction">NONE</div>
</div>

<div class="card">
<h2>Pump</h2>
<div class="value" id="pump">OFF</div>
</div>

<script>

function updateSensors(){

fetch('/sensors')
.then(response => response.json())
.then(data => {

document.getElementById("temp").innerHTML =
data.temperature.toFixed(1) + " °C";

document.getElementById("dist").innerHTML =
data.distance.toFixed(1) + " cm";

document.getElementById("fire").innerHTML =
data.fireDetected ? "🔥 FIRE DETECTED" : "✅ SAFE";

document.getElementById("direction").innerHTML =
data.fireLocation;

document.getElementById("pump").innerHTML =
data.pumpOn ? "ON" : "OFF";

});
}

setInterval(updateSensors,200);

</script>

</body>
</html>
)rawliteral";

// ================= PWM FUNCTIONS =================

void setMotorASpeed(int speed) {

  #ifdef ESP32
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(ENA, speed);
    #else
      ledcWrite(PWM_CHANNEL_A, speed);
    #endif
  #endif
}

void setMotorBSpeed(int speed) {

  #ifdef ESP32
    #if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(ENB, speed);
    #else
      ledcWrite(PWM_CHANNEL_B, speed);
    #endif
  #endif
}

void setBothMotorsSpeed(int speed) {

  setMotorASpeed(speed);
  setMotorBSpeed(speed);
}

// ================= SETUP =================

void setup() {

  Serial.begin(115200);

  pinMode(FLAME_LEFT_PIN, INPUT);
  pinMode(FLAME_RIGHT_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  stopMotors();

  // ================= PWM =================
  #ifdef ESP32

    #if ESP_ARDUINO_VERSION_MAJOR >= 3

      ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
      ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);

    #else

      ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
      ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);

      ledcAttachPin(ENA, PWM_CHANNEL_A);
      ledcAttachPin(ENB, PWM_CHANNEL_B);

    #endif

  #endif

  // ================= DHT =================
  dht.begin();

  // ================= SERVO FIX =================
  flameServo.setPeriodHertz(50);

  flameServo.attach(SERVO_PIN, 500, 2400);

  flameServo.write(90);

  delay(1000);

  Serial.println("Testing Servo...");

  flameServo.write(45);
  delay(1000);

  flameServo.write(135);
  delay(1000);

  flameServo.write(90);
  delay(1000);

  Serial.println("Servo Ready");

  // ================= WIFI =================
  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  int attempts = 0;

  while (WiFi.status() != WL_CONNECTED && attempts < 20) {

    delay(500);

    Serial.print(".");

    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("\nWiFi Connected");

    Serial.println(WiFi.localIP());

  } else {

    WiFi.mode(WIFI_AP);

    WiFi.softAP("FireFighter_Robot", "12345678");

    Serial.println("\nAP Started");

    Serial.println(WiFi.softAPIP());
  }

  // ================= WEB SERVER =================

  server.on("/", HTTP_GET, []() {

    server.send_P(200, "text/html", index_html);
  });

  server.on("/sensors", HTTP_GET, []() {

    StaticJsonDocument<512> doc;

    doc["distance"] = distance;
    doc["temperature"] = temperature;

    doc["flameLeft"] = flameLeftValue;
    doc["flameRight"] = flameRightValue;

    doc["fireDetected"] = fireDetected;

    doc["fireLocation"] = fireLocation;

    doc["pumpOn"] = pumpState;

    doc["servoAngle"] = servoPosition;

    String jsonString;

    serializeJson(doc, jsonString);

    server.send(200, "application/json", jsonString);
  });

  server.begin();

  Serial.println("HTTP Server Started");
}

// ================= FLAME =================

void readFlameSensors() {

  flameLeftValue = analogRead(FLAME_LEFT_PIN);

  flameRightValue = analogRead(FLAME_RIGHT_PIN);
}

FlameDirection getFlameDirection() {

  bool leftFire = flameLeftValue < flameThreshold;

  bool rightFire = flameRightValue < flameThreshold;

  if (!leftFire && !rightFire)
    return NO_FLAME;

  if (leftFire && rightFire)
    return FLAME_BOTH_SIDES;

  if (leftFire)
    return FLAME_LEFT_SIDE;

  if (rightFire)
    return FLAME_RIGHT_SIDE;

  return NO_FLAME;
}

void controlServoByFlame() {

  FlameDirection direction = getFlameDirection();

  switch(direction){

    case FLAME_LEFT_SIDE:

      servoPosition = 45;

      break;

    case FLAME_RIGHT_SIDE:

      servoPosition = 135;

      break;

    default:

      servoPosition = 90;

      break;
  }

  flameServo.write(servoPosition);
}

void updateFireDetection() {

  FlameDirection direction = getFlameDirection();

  fireDetected = direction != NO_FLAME;

  switch(direction){

    case FLAME_LEFT_SIDE:

      fireLocation = "LEFT";

      break;

    case FLAME_RIGHT_SIDE:

      fireLocation = "RIGHT";

      break;

    case FLAME_BOTH_SIDES:

      fireLocation = "CENTER";

      break;

    default:

      fireLocation = "NONE";

      break;
  }
}

void controlExtinguisher() {

  bool flameDetected =
    (flameLeftValue < flameThreshold ||
     flameRightValue < flameThreshold);

  if (flameDetected) {

    digitalWrite(RELAY_PIN, HIGH);

    pumpState = true;

  } else {

    digitalWrite(RELAY_PIN, LOW);

    pumpState = false;
  }
}

// ================= DHT =================

void readTemperature() {

  float temp = dht.readTemperature();

  if (!isnan(temp)) {

    temperature = temp;
  }
}

// ================= ULTRASONIC =================

float readUltrasonicDistance() {

  digitalWrite(TRIG_PIN, LOW);

  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);

  delayMicroseconds(10);

  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if(duration == 0)
    return 999;

  return duration * 0.034 / 2;
}

// ================= NAVIGATION =================

void navigateToFire() {

  FlameDirection direction = getFlameDirection();

  if (!fireDetected) {

    if (distance < 20) {

      turnRight();

      delay(400);

    } else {

      moveForward();
    }

    return;
  }

  switch(direction){

    case FLAME_LEFT_SIDE:

      turnLeft();

      delay(200);

      break;

    case FLAME_RIGHT_SIDE:

      turnRight();

      delay(200);

      break;

    case FLAME_BOTH_SIDES:

      moveForward();

      delay(150);

      break;

    default:

      stopMotors();

      break;
  }

  if(distance < 10){

    stopMotors();
  }
}

// ================= LOOP =================

void loop() {

  server.handleClient();

  unsigned long currentMillis = millis();

  if(currentMillis - lastUltrasonicRead >= ultrasonicInterval){

    distance = readUltrasonicDistance();

    lastUltrasonicRead = currentMillis;
  }

  if(currentMillis - lastFlameRead >= flameInterval){

    readFlameSensors();

    updateFireDetection();

    controlServoByFlame();

    controlExtinguisher();

    lastFlameRead = currentMillis;
  }

  if(currentMillis - lastTemperatureRead >= temperatureInterval){

    readTemperature();

    lastTemperatureRead = currentMillis;
  }

  navigateToFire();
}

// ================= MOTOR FUNCTIONS =================

void moveForward() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  setBothMotorsSpeed(currentSpeed);
}

void moveBackward() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  setBothMotorsSpeed(currentSpeed);
}

void turnLeft() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  setBothMotorsSpeed(currentSpeed);
}

void turnRight() {

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  setBothMotorsSpeed(currentSpeed);
}

void stopMotors() {

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  setBothMotorsSpeed(0);
}
