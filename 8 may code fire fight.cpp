#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "Trisha Fire Service";
const char* password = "123456789";

// Pin definitions
// Flame Sensors (2 sensors only)
#define FLAME_LEFT_PIN  34   // Left flame sensor
#define FLAME_RIGHT_PIN 35   // Right flame sensor
#define RELAY_PIN       4    // Controls pump/fan for extinguisher

// Ultrasonic Sensor
#define TRIG_PIN 21
#define ECHO_PIN 19

// L298N Motor Driver
#define IN1 26   // Motor 1 Direction
#define IN2 27   // Motor 1 Direction
#define ENA 25   // Enable A (Motor 1 & 2) - PWM
#define IN3 14   // Motor 3 Direction
#define IN4 12   // Motor 3 Direction
#define ENB 33   // Enable B (Motor 3 & 4) - PWM (CHANGED FROM GPIO 5 to GPIO 33)

// DHT11 Sensor
#define DHT_PIN 13     // DHT11 data pin
#define DHT_TYPE DHT11

// Servo Motor
#define SERVO_PIN 2    // Servo PWM pin

// PWM channels
#define PWM_CHANNEL_A 0  // For ENA (Motor 1 & 2)
#define PWM_CHANNEL_B 1  // For ENB (Motor 3 & 4)
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// === Flame Detection Parameters ===
int flameThreshold = 500;

// === Flame Proximity Parameters ===
int closeFlameThreshold = 300;
int farFlameMin = 300;
int farFlameMax = 400;

// Global variables
WebServer server(80);
DHT dht(DHT_PIN, DHT_TYPE);
Servo flameServo;

bool autonomousMode = false;
bool manualMode = true;
int currentSpeed = 200;
int manualSpeed = 200;
unsigned long lastUltrasonicRead = 0;
unsigned long lastFlameRead = 0;
unsigned long lastFireAlert = 0;
unsigned long lastTemperatureRead = 0;
const int ultrasonicInterval = 50;
const int flameInterval = 100;
const int fireAlertInterval = 1000;
const int temperatureInterval = 2000;

float distance = 0;
float temperature = 0;
int servoPosition = 90;

// Flame detection variables
bool fireDetected = false;
String fireLocation = "None";
bool pumpState = false;
int flameProximity = 0;

// Flame sensor readings
int flameLeftValue = 1023;
int flameRightValue = 1023;

bool apMode = false;

// === Flame Direction Enum ===
enum FlameDirection {
  NO_FLAME,
  FLAME_LEFT_SIDE,
  FLAME_RIGHT_SIDE,
  FLAME_BOTH_SIDES
};

// HTML for web interface
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Fire Fighting Robot</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            max-width: 600px;
            width: 100%;
        }
        h1 {
            text-align: center;
            color: #333;
            margin-bottom: 10px;
            font-size: 2.2em;
        }
        .subtitle {
            text-align: center;
            color: #666;
            margin-bottom: 20px;
            font-size: 0.9em;
        }
        .fire-alert {
            background: #ff0000;
            color: white;
            padding: 15px;
            border-radius: 10px;
            margin: 10px 0;
            text-align: center;
            font-weight: bold;
            font-size: 18px;
            animation: blink 1s infinite;
            display: none;
            box-shadow: 0 4px 15px rgba(255, 0, 0, 0.3);
        }
        @keyframes blink {
            0% { opacity: 1; }
            50% { opacity: 0.6; }
            100% { opacity: 1; }
        }
        .fire-alert.show {
            display: block;
        }
        .flame-sensors {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin: 15px 0;
        }
        .flame-sensor {
            background: #f5f5f5;
            padding: 15px;
            border-radius: 10px;
            text-align: center;
            font-size: 16px;
            font-weight: bold;
        }
        .flame-sensor.fire {
            background: #ffebee;
            color: #ff0000;
            animation: blink 0.5s infinite;
        }
        .ip-display {
            background: #e3f2fd;
            padding: 10px;
            border-radius: 10px;
            text-align: center;
            margin-bottom: 20px;
            font-size: 14px;
            color: #1976d2;
            word-break: break-all;
        }
        .sensor-panel {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-bottom: 20px;
        }
        .sensor-card {
            background: #f5f5f5;
            border-radius: 10px;
            padding: 15px;
            text-align: center;
        }
        .sensor-card h3 {
            font-size: 14px;
            color: #666;
            margin-bottom: 8px;
        }
        .sensor-card .value {
            font-size: 24px;
            font-weight: bold;
            color: #333;
        }
        .status-panel {
            background: #f5f5f5;
            border-radius: 10px;
            padding: 15px;
            margin-bottom: 20px;
            text-align: center;
        }
        .status-item {
            display: inline-block;
            margin: 5px 10px;
            padding: 5px 15px;
            background: white;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            font-size: 14px;
        }
        .mode-selector {
            display: flex;
            justify-content: center;
            gap: 10px;
            margin-bottom: 30px;
        }
        .mode-btn {
            padding: 12px 20px;
            border: none;
            border-radius: 25px;
            background: #ddd;
            color: #333;
            font-size: 16px;
            cursor: pointer;
            transition: all 0.3s;
            flex: 1;
            font-weight: bold;
        }
        .mode-btn:hover {
            background: #bbb;
            transform: scale(1.05);
        }
        .mode-btn.active {
            background: #667eea;
            color: white;
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4);
        }
        .control-pad {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin-bottom: 30px;
        }
        .control-btn {
            padding: 20px;
            border: none;
            border-radius: 15px;
            background: #f0f0f0;
            font-size: 24px;
            cursor: pointer;
            transition: all 0.3s;
            color: #333;
            user-select: none;
            -webkit-user-select: none;
            touch-action: manipulation;
            min-height: 60px;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .control-btn:hover {
            background: #667eea;
            color: white;
            transform: scale(1.05);
            box-shadow: 0 4px 15px rgba(102, 126, 234, 0.3);
        }
        .control-btn:active {
            background: #764ba2;
            transform: scale(0.95);
        }
        .control-btn:disabled {
            opacity: 0.4;
            cursor: not-allowed;
            transform: none;
            box-shadow: none;
        }
        .speed-control {
            margin-top: 20px;
            text-align: center;
        }
        .speed-slider {
            width: 100%;
            margin: 10px 0;
            -webkit-appearance: none;
            height: 8px;
            background: #ddd;
            border-radius: 5px;
            outline: none;
        }
        .speed-slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 25px;
            height: 25px;
            background: #667eea;
            border-radius: 50%;
            cursor: pointer;
        }
        .speed-slider::-moz-range-thumb {
            width: 25px;
            height: 25px;
            background: #667eea;
            border-radius: 50%;
            cursor: pointer;
        }
        .speed-label {
            color: #666;
            font-size: 14px;
            font-weight: bold;
        }
        .sensor-reading {
            color: #28a745;
            font-weight: bold;
        }
        .mode-indicator {
            text-align: center;
            margin: 10px 0;
            padding: 8px;
            background: #e3f2fd;
            border-radius: 10px;
            font-weight: bold;
            font-size: 16px;
        }
        .pump-status {
            text-align: center;
            margin: 10px 0;
            padding: 8px;
            border-radius: 10px;
            font-weight: bold;
            font-size: 16px;
        }
        .pump-on {
            background: #ffebee;
            color: #ff0000;
        }
        .pump-off {
            background: #e8f5e9;
            color: #2e7d32;
        }
        .servo-indicator {
            text-align: center;
            margin: 10px 0;
            padding: 8px;
            border-radius: 10px;
            background: #f3e5f5;
            color: #7b1fa2;
            font-weight: bold;
            font-size: 14px;
        }
        @media (max-width: 400px) {
            .container {
                padding: 20px;
            }
            .control-btn {
                padding: 15px;
                font-size: 20px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🔥 Fire Fighting Robot</h1>
        <div class="subtitle">ESP32 Autonomous Fire Detection & Extinguishing</div>
        
        <div class="ip-display" id="ipDisplay">
            Connecting to ESP32...
        </div>
        
        <div class="fire-alert" id="fireAlert">
            🔥 FIRE DETECTED! Location: <span id="fireLocationDisplay">None</span>
        </div>
        
        <div class="sensor-panel">
            <div class="sensor-card">
                <h3>🌡️ Temperature</h3>
                <div class="value" id="temperature">--</div>
                <div style="font-size: 12px; color: #666;">°C</div>
            </div>
            <div class="sensor-card">
                <h3>📏 Distance</h3>
                <div class="value" id="distance">--</div>
                <div style="font-size: 12px; color: #666;">cm</div>
            </div>
        </div>
        
        <div class="flame-sensors">
            <div class="flame-sensor" id="flameLeft">👈 Left Sensor<br><span id="flameLeftValue">1023</span></div>
            <div class="flame-sensor" id="flameRight">👉 Right Sensor<br><span id="flameRightValue">1023</span></div>
        </div>
        
        <div class="mode-indicator" id="modeIndicator">
            Mode: Manual
        </div>
        
        <div class="pump-status pump-off" id="pumpStatus">
            💧 Extinguisher: OFF
        </div>
        
        <div class="servo-indicator" id="servoStatus">
            🎯 Servo Direction: Center
        </div>
        
        <div class="status-panel">
            <div class="status-item">
                🔥 Fire Intensity: <span id="fireIntensity" class="sensor-reading">0%</span>
            </div>
            <div class="status-item">
                🎯 Flame Direction: <span id="flameDirection">None</span>
            </div>
        </div>
        
        <div class="mode-selector">
            <button class="mode-btn active" onclick="setMode('manual')">🎮 Manual</button>
            <button class="mode-btn" onclick="setMode('autonomous')">🤖 Auto</button>
            <button class="mode-btn" onclick="setMode('stop')">⏹️ Stop</button>
        </div>
        
        <div class="control-pad" id="controlPad">
            <div></div>
            <button class="control-btn" onmousedown="sendCommand('forward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('forward')" ontouchend="sendCommand('stop')">⬆️</button>
            <div></div>
            <button class="control-btn" onmousedown="sendCommand('left')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('left')" ontouchend="sendCommand('stop')">⬅️</button>
            <button class="control-btn" onclick="sendCommand('stop')" style="background: #ffebee; color: #f44336;">⏹️</button>
            <button class="control-btn" onmousedown="sendCommand('right')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('right')" ontouchend="sendCommand('stop')">➡️</button>
            <div></div>
            <button class="control-btn" onmousedown="sendCommand('backward')" onmouseup="sendCommand('stop')" ontouchstart="sendCommand('backward')" ontouchend="sendCommand('stop')">⬇️</button>
            <div></div>
        </div>
        
        <div class="speed-control">
            <label class="speed-label">⚡ Speed: <span id="speedValue">200</span></label>
            <input type="range" class="speed-slider" min="100" max="255" value="200" onchange="updateSpeed(this.value)" id="speedSlider">
        </div>
    </div>
    
    <script>
        let currentSpeed = 200;
        let currentMode = 'manual';
        
        document.getElementById('ipDisplay').textContent = '🌐 ESP32 IP: ' + window.location.hostname;
        
        function sendCommand(command) {
            let url = `/control?command=${command}&speed=${currentSpeed}`;
            fetch(url)
                .then(response => response.text())
                .catch(error => console.error('Error:', error));
        }
        
        function setMode(mode) {
            currentMode = mode;
            fetch(`/mode?mode=${mode}`)
                .then(response => response.text())
                .then(data => {
                    document.querySelectorAll('.mode-btn').forEach(btn => {
                        btn.classList.remove('active');
                        if (btn.textContent.toLowerCase().includes(mode) || 
                            (mode === 'stop' && btn.textContent.includes('Stop'))) {
                            btn.classList.add('active');
                        }
                    });
                    
                    const modeNames = {
                        'manual': '🎮 Manual Control',
                        'autonomous': '🤖 Autonomous Mode',
                        'stop': '⏹️ Stopped'
                    };
                    document.getElementById('modeIndicator').textContent = 
                        `Mode: ${modeNames[mode] || mode}`;
                    
                    const indicator = document.getElementById('modeIndicator');
                    if (mode === 'autonomous') {
                        indicator.style.background = '#fff3e0';
                        indicator.style.color = '#e65100';
                    } else if (mode === 'stop') {
                        indicator.style.background = '#ffebee';
                        indicator.style.color = '#c62828';
                    } else {
                        indicator.style.background = '#e3f2fd';
                        indicator.style.color = '#1976d2';
                    }
                    
                    const buttons = document.querySelectorAll('#controlPad button');
                    buttons.forEach(btn => {
                        btn.disabled = (mode === 'autonomous' || mode === 'stop');
                    });
                })
                .catch(error => console.error('Error:', error));
        }
        
        function updateSpeed(value) {
            currentSpeed = parseInt(value);
            document.getElementById('speedValue').textContent = value;
            fetch(`/speed?value=${value}`)
                .catch(error => console.error('Error:', error));
        }
        
        function updateSensors() {
            fetch('/sensors')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('distance').textContent = data.distance.toFixed(1);
                    document.getElementById('temperature').textContent = data.temperature.toFixed(1);
                    
                    document.getElementById('flameLeftValue').textContent = data.flameLeft;
                    document.getElementById('flameRightValue').textContent = data.flameRight;
                    
                    const flameLeft = document.getElementById('flameLeft');
                    const flameRight = document.getElementById('flameRight');
                    
                    flameLeft.className = data.flameLeft < 500 ? 'flame-sensor fire' : 'flame-sensor';
                    flameRight.className = data.flameRight < 500 ? 'flame-sensor fire' : 'flame-sensor';
                    
                    const fireAlert = document.getElementById('fireAlert');
                    const fireLocationDisplay = document.getElementById('fireLocationDisplay');
                    
                    if (data.fireDetected) {
                        fireAlert.classList.add('show');
                        fireLocationDisplay.textContent = data.fireLocation;
                    } else {
                        fireAlert.classList.remove('show');
                    }
                    
                    document.getElementById('fireIntensity').textContent = data.fireIntensity + '%';
                    document.getElementById('flameDirection').textContent = data.flameDirection;
                    
                    const pumpStatus = document.getElementById('pumpStatus');
                    if (data.pumpOn) {
                        pumpStatus.textContent = '💧 Extinguisher: ON - SPRAYING!';
                        pumpStatus.className = 'pump-status pump-on';
                    } else {
                        pumpStatus.textContent = '💧 Extinguisher: OFF';
                        pumpStatus.className = 'pump-status pump-off';
                    }
                    
                    const servoStatus = document.getElementById('servoStatus');
                    if (data.servoAngle <= 80) {
                        servoStatus.innerHTML = '🎯 Servo Direction: Pointing LEFT';
                    } else if (data.servoAngle >= 100) {
                        servoStatus.innerHTML = '🎯 Servo Direction: Pointing RIGHT';
                    } else {
                        servoStatus.innerHTML = '🎯 Servo Direction: Center';
                    }
                    
                    const dist = document.getElementById('distance');
                    dist.style.color = data.distance < 20 ? '#dc3545' : '#28a745';
                    
                    const temp = document.getElementById('temperature');
                    temp.style.color = data.temperature > 40 ? '#dc3545' : '#28a745';
                })
                .catch(error => console.error('Error:', error));
        }
        
        document.addEventListener('keydown', function(event) {
            if (currentMode !== 'manual') return;
            
            switch(event.key) {
                case 'ArrowUp':
                case 'w':
                case 'W':
                    event.preventDefault();
                    sendCommand('forward');
                    break;
                case 'ArrowDown':
                case 's':
                case 'S':
                    event.preventDefault();
                    sendCommand('backward');
                    break;
                case 'ArrowLeft':
                case 'a':
                case 'A':
                    event.preventDefault();
                    sendCommand('left');
                    break;
                case 'ArrowRight':
                case 'd':
                case 'D':
                    event.preventDefault();
                    sendCommand('right');
                    break;
                case ' ':
                    event.preventDefault();
                    sendCommand('stop');
                    break;
            }
        });
        
        document.addEventListener('keyup', function(event) {
            if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'w', 'W', 'a', 'A', 's', 'S', 'd', 'D'].includes(event.key)) {
                sendCommand('stop');
            }
        });
        
        setInterval(updateSensors, 200);
        updateSensors();
    </script>
</body>
</html>
)rawliteral";

// Function to set PWM for Motor A (ENA - GPIO 25)
void setMotorASpeed(int speed) {
    #ifdef ESP32
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcWrite(ENA, speed);
        #else
            ledcWrite(PWM_CHANNEL_A, speed);
        #endif
    #endif
}

// Function to set PWM for Motor B (ENB - GPIO 33)
void setMotorBSpeed(int speed) {
    #ifdef ESP32
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcWrite(ENB, speed);
        #else
            ledcWrite(PWM_CHANNEL_B, speed);
        #endif
    #endif
}

// Function to set both motors to same speed
void setBothMotorsSpeed(int speed) {
    setMotorASpeed(speed);
    setMotorBSpeed(speed);
    Serial.print("PWM -> ENA(GPIO25): ");
    Serial.print(speed);
    Serial.print(" | ENB(GPIO33): ");
    Serial.println(speed);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=================================");
    Serial.println("   ESP32 Fire Fighting Robot");
    Serial.println("   GPIO 33 for ENB (Motor B)");
    Serial.println("=================================");
    
    // Initialize flame sensor pins
    pinMode(FLAME_LEFT_PIN, INPUT);
    pinMode(FLAME_RIGHT_PIN, INPUT);
    
    // Initialize relay pin
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
    pumpState = false;
    
    // Initialize ultrasonic sensor pins
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    
    // Motor pins
    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(ENA, OUTPUT);
    pinMode(IN3, OUTPUT);
    pinMode(IN4, OUTPUT);
    pinMode(ENB, OUTPUT);
    
    // Stop motors initially
    stopMotors();
    
    // Setup PWM for ESP32
    Serial.println("\n🔧 Setting up PWM channels...");
    #ifdef ESP32
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            // New ESP32 Arduino core (3.0.0+)
            ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
            ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);
            Serial.println("✅ Using new LEDC API");
        #else
            // Old ESP32 Arduino core (2.x.x)
            ledcSetup(PWM_CHANNEL_A, PWM_FREQ, PWM_RESOLUTION);
            ledcSetup(PWM_CHANNEL_B, PWM_FREQ, PWM_RESOLUTION);
            ledcAttachPin(ENA, PWM_CHANNEL_A);
            ledcAttachPin(ENB, PWM_CHANNEL_B);
            Serial.println("✅ Using legacy LEDC API");
        #endif
    #endif
    
    Serial.print("   ENA -> GPIO 25 (Channel ");
    Serial.print(PWM_CHANNEL_A);
    Serial.println(")");
    Serial.print("   ENB -> GPIO 33 (Channel ");
    Serial.print(PWM_CHANNEL_B);
    Serial.println(")");
    
    // Test both motors individually
    Serial.println("\n🔧 TESTING MOTORS INDIVIDUALLY:");
    
    Serial.println("\n--- Testing Motor A (Left Side) ---");
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    for (int speed = 100; speed <= 255; speed += 50) {
        Serial.print("  Motor A speed: ");
        Serial.println(speed);
        setMotorASpeed(speed);
        delay(500);
    }
    setMotorASpeed(0);
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    Serial.println("✅ Motor A test complete");
    delay(1000);
    
    Serial.println("\n--- Testing Motor B (Right Side) ---");
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    for (int speed = 100; speed <= 255; speed += 50) {
        Serial.print("  Motor B speed: ");
        Serial.println(speed);
        setMotorBSpeed(speed);
        delay(500);
    }
    setMotorBSpeed(0);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    Serial.println("✅ Motor B test complete");
    delay(1000);
    
    // Test both motors together
    Serial.println("\n--- Testing Both Motors Together (Forward) ---");
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    for (int speed = 150; speed <= 255; speed += 50) {
        Serial.print("  Both motors speed: ");
        Serial.println(speed);
        setBothMotorsSpeed(speed);
        delay(500);
    }
    stopMotors();
    Serial.println("✅ Both motors test complete");
    
    // Initialize DHT11 sensor
    dht.begin();
    Serial.println("\n✅ DHT11 sensor initialized");
    
    // Initialize Servo
    flameServo.attach(SERVO_PIN);
    flameServo.write(90);
    Serial.println("✅ Servo initialized - Center position (90°)");
    
    // Test relay
    Serial.println("\n🔧 Testing relay...");
    digitalWrite(RELAY_PIN, HIGH);
    delay(1000);
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("✅ Relay test complete");
    
    // Connect to WiFi
    Serial.println("\n---------------------------------");
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi Connected!");
        Serial.print("📡 IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("\n👉 Open: http://");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n❌ WiFi connection failed!");
        Serial.println("🔄 Creating Access Point...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("FireFighter_Robot", "12345678");
        Serial.print("📡 AP IP address: ");
        Serial.println(WiFi.softAPIP());
        Serial.print("👉 Open: http://");
        Serial.println(WiFi.softAPIP());
        apMode = true;
    }
    
    // Setup web server routes
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", index_html);
    });
    
    server.on("/control", HTTP_GET, []() {
        if (server.hasArg("command")) {
            String command = server.arg("command");
            
            if (server.hasArg("speed")) {
                manualSpeed = server.arg("speed").toInt();
                if (!autonomousMode) {
                    currentSpeed = manualSpeed;
                }
            }
            
            Serial.print("🎮 Command: ");
            Serial.print(command);
            Serial.print(" | Speed: ");
            Serial.println(currentSpeed);
            
            if (!autonomousMode || command == "stop") {
                if (command == "forward") moveForward();
                else if (command == "backward") moveBackward();
                else if (command == "left") turnLeft();
                else if (command == "right") turnRight();
                else if (command == "stop") stopMotors();
            } else {
                Serial.println("   ⚠️ Ignored - Auto mode active");
            }
        }
        server.send(200, "text/plain", "OK");
    });
    
    server.on("/mode", HTTP_GET, []() {
        if (server.hasArg("mode")) {
            String mode = server.arg("mode");
            
            if (mode == "manual") {
                autonomousMode = false;
                manualMode = true;
                stopMotors();
                currentSpeed = manualSpeed;
                digitalWrite(RELAY_PIN, LOW);
                pumpState = false;
                Serial.println("🎮 Mode: Manual");
            } else if (mode == "autonomous") {
                autonomousMode = true;
                manualMode = false;
                Serial.println("🤖 Mode: Autonomous");
            } else if (mode == "stop") {
                autonomousMode = false;
                manualMode = false;
                stopMotors();
                digitalWrite(RELAY_PIN, LOW);
                pumpState = false;
                Serial.println("⏹️ Mode: Stop");
            }
        }
        server.send(200, "text/plain", "OK");
    });
    
    server.on("/speed", HTTP_GET, []() {
        if (server.hasArg("value")) {
            manualSpeed = server.arg("value").toInt();
            if (!autonomousMode) {
                currentSpeed = manualSpeed;
            }
            Serial.print("⚡ Speed: ");
            Serial.println(manualSpeed);
        }
        server.send(200, "text/plain", "OK");
    });
    
    server.on("/sensors", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["distance"] = distance;
        doc["temperature"] = temperature;
        doc["flameLeft"] = flameLeftValue;
        doc["flameRight"] = flameRightValue;
        doc["fireDetected"] = fireDetected;
        doc["fireLocation"] = fireLocation;
        doc["fireIntensity"] = getMaxFlameIntensity();
        doc["pumpOn"] = pumpState;
        doc["flameProximity"] = flameProximity;
        doc["servoAngle"] = servoPosition;
        
        String directionStr = "None";
        switch (getFlameDirection()) {
            case FLAME_LEFT_SIDE: directionStr = "Left"; break;
            case FLAME_RIGHT_SIDE: directionStr = "Right"; break;
            case FLAME_BOTH_SIDES: directionStr = "Both"; break;
            default: directionStr = "None";
        }
        doc["flameDirection"] = directionStr;
        
        String mode = "stop";
        if (autonomousMode) mode = "autonomous";
        else if (manualMode) mode = "manual";
        doc["mode"] = mode;
        doc["speed"] = currentSpeed;
        
        String jsonString;
        serializeJson(doc, jsonString);
        server.send(200, "application/json", jsonString);
    });
    
    server.begin();
    Serial.println("\n✅ HTTP server started");
    Serial.println("=================================");
    Serial.println("\n📋 NEW WIRING:");
    Serial.println("  ENA (Motor A) -> GPIO 25 ✅");
    Serial.println("  ENB (Motor B) -> GPIO 33 ✅ (CHANGED FROM GPIO 5)");
    Serial.println("\n⚠️  IMPORTANT: Reconnect ENB wire from GPIO 5 to GPIO 33!");
    Serial.println("=================================\n");
}

// === FLAME DETECTION FUNCTIONS ===

void readFlameSensors() {
    flameLeftValue = analogRead(FLAME_LEFT_PIN);
    flameRightValue = analogRead(FLAME_RIGHT_PIN);
}

FlameDirection getFlameDirection() {
    bool leftFire = (flameLeftValue < flameThreshold);
    bool rightFire = (flameRightValue < flameThreshold);
    
    if (!leftFire && !rightFire) return NO_FLAME;
    if (leftFire && rightFire) return FLAME_BOTH_SIDES;
    if (leftFire) return FLAME_LEFT_SIDE;
    if (rightFire) return FLAME_RIGHT_SIDE;
    return NO_FLAME;
}

void controlServoByFlame() {
    FlameDirection direction = getFlameDirection();
    
    switch (direction) {
        case FLAME_LEFT_SIDE:
            servoPosition = 45;
            flameServo.write(servoPosition);
            break;
        case FLAME_RIGHT_SIDE:
            servoPosition = 135;
            flameServo.write(servoPosition);
            break;
        case FLAME_BOTH_SIDES:
        case NO_FLAME:
            servoPosition = 90;
            flameServo.write(servoPosition);
            break;
    }
}

int getFlameProximity() {
    int minValue = min(flameLeftValue, flameRightValue);
    if (minValue < closeFlameThreshold) return 1;
    else if (minValue >= farFlameMin && minValue < farFlameMax) return 2;
    else if (minValue < flameThreshold) return 3;
    else return 0;
}

int getMaxFlameIntensity() {
    int minValue = min(flameLeftValue, flameRightValue);
    if (minValue >= flameThreshold) return 0;
    int intensity = map(minValue, 0, flameThreshold, 100, 0);
    return constrain(intensity, 0, 100);
}

void readTemperature() {
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
        temperature = temp;
    }
}

void controlExtinguisher() {
    bool flameDetected = (flameLeftValue < flameThreshold || flameRightValue < flameThreshold);
    
    if (flameDetected && !pumpState) {
        digitalWrite(RELAY_PIN, HIGH);
        pumpState = true;
        Serial.println("🔥 FIRE! Pump ON!");
    } else if (!flameDetected && pumpState) {
        digitalWrite(RELAY_PIN, LOW);
        pumpState = false;
        Serial.println("✅ Pump OFF");
    }
}

void updateFireDetection() {
    FlameDirection direction = getFlameDirection();
    flameProximity = getFlameProximity();
    controlServoByFlame();
    fireDetected = (direction != NO_FLAME);
    
    switch (direction) {
        case NO_FLAME: fireLocation = "None"; break;
        case FLAME_LEFT_SIDE: fireLocation = "Left Side"; break;
        case FLAME_RIGHT_SIDE: fireLocation = "Right Side"; break;
        case FLAME_BOTH_SIDES: fireLocation = "Both Sides"; break;
    }
}

// === NAVIGATION FUNCTIONS ===

void navigateToFire() {
    FlameDirection direction = getFlameDirection();
    
    if (!fireDetected) {
        if (distance < 20) {
            turnRight();
            delay(500);
        } else {
            moveForward();
        }
        return;
    }
    
    switch (direction) {
        case FLAME_LEFT_SIDE:
            currentSpeed = 255;
            turnLeft();
            delay(flameProximity == 1 ? 300 : 150);
            break;
        case FLAME_RIGHT_SIDE:
            currentSpeed = 255;
            turnRight();
            delay(flameProximity == 1 ? 300 : 150);
            break;
        case FLAME_BOTH_SIDES:
            currentSpeed = (flameProximity == 1) ? 150 : 255;
            moveForward();
            delay(flameProximity == 1 ? 200 : 100);
            break;
        default:
            currentSpeed = 255;
            moveForward();
            break;
    }
    
    if (flameProximity == 1) {
        stopMotors();
    }
}

void loop() {
    server.handleClient();
    
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastUltrasonicRead >= ultrasonicInterval) {
        distance = readUltrasonicDistance();
        lastUltrasonicRead = currentMillis;
    }
    
    if (currentMillis - lastFlameRead >= flameInterval) {
        readFlameSensors();
        updateFireDetection();
        controlExtinguisher();
        lastFlameRead = currentMillis;
    }
    
    if (currentMillis - lastTemperatureRead >= temperatureInterval) {
        readTemperature();
        lastTemperatureRead = currentMillis;
    }
    
    if (autonomousMode) {
        navigateToFire();
    }
}

float readUltrasonicDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) return 999;
    return duration * 0.034 / 2;
}

// === MOTOR CONTROL FUNCTIONS ===

void moveForward() {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    setBothMotorsSpeed(currentSpeed);
    Serial.println("⬆️ Forward");
}

void moveBackward() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    setBothMotorsSpeed(currentSpeed);
    Serial.println("⬇️ Backward");
}

void turnLeft() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    setBothMotorsSpeed(currentSpeed);
    Serial.println("⬅️ Left");
}

void turnRight() {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    setBothMotorsSpeed(currentSpeed);
    Serial.println("➡️ Right");
}

void stopMotors() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    setBothMotorsSpeed(0);
    Serial.println("⏹️ Stop");
}
