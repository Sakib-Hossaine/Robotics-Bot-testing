#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Trisha Fire Service";
const char* password = "123456789";

// Pin definitions
// IR Sensors
#define IR_SENSOR_LEFT 34   // Left IR sensor
#define IR_SENSOR_RIGHT 35  // Right IR sensor

// Ultrasonic Sensor
#define TRIG_PIN 21
#define ECHO_PIN 19

// L298N Motor Driver
#define IN1 26
#define IN2 27
#define ENA 25  // Enable A (Left motors) - PWM
#define IN3 14
#define IN4 12
#define ENB 5   // Enable B (Right motors) - PWM

// PWM channels
#define PWM_CHANNEL_LEFT 0
#define PWM_CHANNEL_RIGHT 1
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8

// Global variables
WebServer server(80);

bool autonomousMode = false;
bool manualMode = true;  // Start in manual mode
int currentSpeed = 200;  // Default speed (0-255)
unsigned long lastUltrasonicRead = 0;
unsigned long lastIRRead = 0;
const int ultrasonicInterval = 50;  // Read ultrasonic every 50ms
const int irInterval = 10;          // Read IR sensors every 10ms

float distance = 0;
bool leftIRSensor = false;
bool rightIRSensor = false;
bool apMode = false;

// Fire detection variables
bool fireDetected = false;
String fireLocation = "None";
unsigned long lastFireAlert = 0;
const int fireAlertInterval = 1000; // Alert every 1 second

// HTML for web interface
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Robot Control</title>
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
            max-width: 500px;
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
        .test-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
            margin-bottom: 20px;
        }
        .test-btn {
            padding: 15px;
            border: none;
            border-radius: 10px;
            background: #ff9800;
            color: white;
            font-size: 14px;
            cursor: pointer;
            transition: all 0.3s;
            font-weight: bold;
        }
        .test-btn:hover {
            background: #f57c00;
            transform: scale(1.05);
        }
        .test-btn:active {
            background: #e65100;
            transform: scale(0.95);
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
        <h1>🤖 Robot Control</h1>
        <div class="subtitle">ESP32 WiFi Robot with Fire Detection</div>
        
        <div class="ip-display" id="ipDisplay">
            Connecting to ESP32...
        </div>
        
        <div class="fire-alert" id="fireAlert">
            🔥 FIRE DETECTED! Location: <span id="fireLocationDisplay">None</span>
        </div>
        
        <div class="mode-indicator" id="modeIndicator">
            Mode: Manual
        </div>
        
        <div class="test-buttons">
            <button class="test-btn" onclick="testMotor('left')">🧪 Test Left Motor</button>
            <button class="test-btn" onclick="testMotor('right')">🧪 Test Right Motor</button>
            <button class="test-btn" onclick="testMotor('both')">🧪 Test Both Motors</button>
            <button class="test-btn" onclick="testMotor('stop')" style="background: #f44336;">⏹️ Stop Test</button>
        </div>
        
        <div class="status-panel">
            <div class="status-item">
                📏 Distance: <span id="distance" class="sensor-reading">0</span> cm
            </div>
            <div class="status-item">
                👁️ IR Left: <span id="irLeft" class="sensor-reading">Clear</span>
            </div>
            <div class="status-item">
                👁️ IR Right: <span id="irRight" class="sensor-reading">Clear</span>
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
        
        function testMotor(motor) {
            fetch(`/test?motor=${motor}&speed=${currentSpeed}`)
                .then(response => response.text())
                .then(data => console.log('Test:', data))
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
                    
                    // Update fire alert
                    const fireAlert = document.getElementById('fireAlert');
                    const fireLocationDisplay = document.getElementById('fireLocationDisplay');
                    
                    if (data.fireDetected) {
                        fireAlert.classList.add('show');
                        fireLocationDisplay.textContent = data.fireLocation;
                        
                        // Update IR sensors for fire
                        document.getElementById('irLeft').textContent = '🔥 FIRE!';
                        document.getElementById('irRight').textContent = '🔥 FIRE!';
                        document.getElementById('irLeft').style.color = '#ff0000';
                        document.getElementById('irRight').style.color = '#ff0000';
                    } else {
                        fireAlert.classList.remove('show');
                        
                        // Normal IR display
                        document.getElementById('irLeft').textContent = data.irLeft ? '🔴 Object' : '🟢 Clear';
                        document.getElementById('irRight').textContent = data.irRight ? '🔴 Object' : '🟢 Clear';
                        document.getElementById('irLeft').style.color = data.irLeft ? '#dc3545' : '#28a745';
                        document.getElementById('irRight').style.color = data.irRight ? '#dc3545' : '#28a745';
                    }
                    
                    const dist = document.getElementById('distance');
                    dist.style.color = data.distance < 20 ? '#dc3545' : '#28a745';
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
        
        document.addEventListener('keypress', function(event) {
            switch(event.key.toLowerCase()) {
                case 'm':
                    setMode('manual');
                    break;
                case 'a':
                    if (!event.ctrlKey) {
                        setMode('autonomous');
                    }
                    break;
                case 'x':
                    setMode('stop');
                    break;
            }
        });
        
        setInterval(updateSensors, 200);
        updateSensors();
    </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=================================");
    Serial.println("   ESP32 Robot Controller");
    Serial.println("   with Fire Detection");
    Serial.println("=================================");
    
    // Initialize pins
    pinMode(IR_SENSOR_LEFT, INPUT);
    pinMode(IR_SENSOR_RIGHT, INPUT);
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
    
    // Setup PWM for ESP32 - FIXED for newer ESP32 board packages
    #ifdef ESP32
        // For ESP32 Arduino core 3.0.0+
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);  // New API
            ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);  // New API
        #else
            // For ESP32 Arduino core 2.x.x
            ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQ, PWM_RESOLUTION);
            ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQ, PWM_RESOLUTION);
            ledcAttachPin(ENA, PWM_CHANNEL_LEFT);
            ledcAttachPin(ENB, PWM_CHANNEL_RIGHT);
        #endif
    #endif
    
    Serial.println("\nMotor pins initialized");
    Serial.println("PWM channels configured");
    Serial.println("Fire detection enabled in Manual Mode");
    
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
        Serial.println("\n👉 Open this URL in your browser:");
        Serial.print("   http://");
        Serial.println(WiFi.localIP());
    } else {
        // If WiFi connection fails, create Access Point
        Serial.println("\n❌ WiFi connection failed!");
        Serial.println("🔄 Creating Access Point...");
        
        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESP32_Robot", "12345678");
        
        Serial.println("\n✅ Access Point Created!");
        Serial.print("📡 AP IP address: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("\n👉 Connect to WiFi network: ESP32_Robot");
        Serial.println("   Password: 12345678");
        Serial.print("   Then open: http://");
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
            
            if (!autonomousMode || command == "stop") {
                if (command == "forward") {
                    moveForward();
                } else if (command == "backward") {
                    moveBackward();
                } else if (command == "left") {
                    turnLeft();
                } else if (command == "right") {
                    turnRight();
                } else if (command == "stop") {
                    stopMotors();
                }
            }
            
            if (server.hasArg("speed")) {
                currentSpeed = server.arg("speed").toInt();
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
                Serial.println("🎮 Mode: Manual - Fire Detection Active");
            } else if (mode == "autonomous") {
                autonomousMode = true;
                manualMode = false;
                Serial.println("🤖 Mode: Autonomous");
            } else if (mode == "stop") {
                autonomousMode = false;
                manualMode = false;
                stopMotors();
                Serial.println("⏹️ Mode: Stop");
            }
        }
        server.send(200, "text/plain", "OK");
    });
    
    server.on("/speed", HTTP_GET, []() {
        if (server.hasArg("value")) {
            currentSpeed = server.arg("value").toInt();
            Serial.print("⚡ Speed: ");
            Serial.println(currentSpeed);
        }
        server.send(200, "text/plain", "OK");
    });
    
    server.on("/test", HTTP_GET, []() {
        if (server.hasArg("motor")) {
            String motor = server.arg("motor");
            int testSpeed = 200;
            if (server.hasArg("speed")) {
                testSpeed = server.arg("speed").toInt();
            }
            
            Serial.println("\n=== MOTOR TEST ===");
            
            if (motor == "left") {
                Serial.println("Testing LEFT motor only");
                // Left motor forward
                digitalWrite(IN1, HIGH);
                digitalWrite(IN2, LOW);
                setMotorSpeed(ENA, testSpeed);
                // Stop right motor
                digitalWrite(IN3, LOW);
                digitalWrite(IN4, LOW);
                setMotorSpeed(ENB, 0);
            } else if (motor == "right") {
                Serial.println("Testing RIGHT motor only");
                // Stop left motor
                digitalWrite(IN1, LOW);
                digitalWrite(IN2, LOW);
                setMotorSpeed(ENA, 0);
                // Right motor forward
                digitalWrite(IN3, HIGH);
                digitalWrite(IN4, LOW);
                setMotorSpeed(ENB, testSpeed);
            } else if (motor == "both") {
                Serial.println("Testing BOTH motors");
                moveForward();
            } else if (motor == "stop") {
                Serial.println("Stopping test");
                stopMotors();
            }
            
            Serial.printf("Speed: %d\n", testSpeed);
            Serial.println("==================");
        }
        server.send(200, "text/plain", "OK");
    });
    
    server.on("/sensors", HTTP_GET, []() {
        StaticJsonDocument<256> doc;
        doc["distance"] = distance;
        doc["irLeft"] = leftIRSensor;
        doc["irRight"] = rightIRSensor;
        doc["fireDetected"] = fireDetected;
        doc["fireLocation"] = fireLocation;
        
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
    Serial.println("\n📋 WIRING CHECKLIST:");
    Serial.println("---------------------------------");
    Serial.println("L298N -> ESP32:");
    Serial.println("  ENA  -> GPIO 25 (Remove jumper!)");
    Serial.println("  ENB  -> GPIO 5  (Remove jumper!)");
    Serial.println("  IN1  -> GPIO 26");
    Serial.println("  IN2  -> GPIO 27");
    Serial.println("  IN3  -> GPIO 14");
    Serial.println("  IN4  -> GPIO 12");
    Serial.println("  GND  -> ESP32 GND");
    Serial.println("  12V  -> External Battery (7-12V)");
    Serial.println("  5V   -> ESP32 VIN (optional)");
    Serial.println("\nSensors -> ESP32:");
    Serial.println("  IR Left  OUT -> GPIO 34");
    Serial.println("  IR Right OUT -> GPIO 35");
    Serial.println("  Ultrasonic TRIG -> GPIO 21");
    Serial.println("  Ultrasonic ECHO -> GPIO 19");
    Serial.println("\n🔥 Fire Detection: Active in Manual Mode");
    Serial.println("=================================\n");
    
    // Run initial motor test
    Serial.println("🔧 Running 2-second motor test...");
    setMotorSpeed(ENA, 200);
    setMotorSpeed(ENB, 200);
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    delay(2000);
    stopMotors();
    Serial.println("✅ Motor test complete. Check if motors moved.\n");
}

// Helper function to set motor speed - works with both old and new API
void setMotorSpeed(int pin, int speed) {
    #ifdef ESP32
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcWrite(pin, speed);  // New API uses pin directly
        #else
            // Old API uses channel
            if (pin == ENA) {
                ledcWrite(PWM_CHANNEL_LEFT, speed);
            } else if (pin == ENB) {
                ledcWrite(PWM_CHANNEL_RIGHT, speed);
            }
        #endif
    #endif
}

// Fire detection function
void checkFireDetection() {
    // Read IR sensors (active LOW - adjust if your sensors are different)
    bool leftFire = !digitalRead(IR_SENSOR_LEFT);
    bool rightFire = !digitalRead(IR_SENSOR_RIGHT);
    
    if (leftFire && rightFire) {
        fireDetected = true;
        fireLocation = "Both Sides";
    } else if (leftFire) {
        fireDetected = true;
        fireLocation = "Left Side";
    } else if (rightFire) {
        fireDetected = true;
        fireLocation = "Right Side";
    } else {
        fireDetected = false;
        fireLocation = "None";
    }
    
    // Print fire alert to Serial Monitor
    if (fireDetected && (millis() - lastFireAlert > fireAlertInterval)) {
        Serial.println("🔥 FIRE DETECTED! Location: " + fireLocation);
        lastFireAlert = millis();
    }
}

void loop() {
    server.handleClient();
    
    unsigned long currentMillis = millis();
    
    // Read sensors at intervals
    if (currentMillis - lastUltrasonicRead >= ultrasonicInterval) {
        distance = readUltrasonicDistance();
        lastUltrasonicRead = currentMillis;
    }
    
    if (currentMillis - lastIRRead >= irInterval) {
        leftIRSensor = !digitalRead(IR_SENSOR_LEFT);
        rightIRSensor = !digitalRead(IR_SENSOR_RIGHT);
        lastIRRead = currentMillis;
    }
    
    // Check for fire in manual mode
    if (manualMode) {
        checkFireDetection();
    }
    
    // Handle autonomous mode
    if (autonomousMode) {
        autonomousControl();
    }
}

void autonomousControl() {
    // Simple obstacle avoidance algorithm
    if (distance < 20) {
        // Obstacle detected in front
        stopMotors();
        delay(100);
        
        // Check which way to turn
        if (!rightIRSensor) {
            Serial.println("↗️ Avoiding right");
            turnRight();
            delay(300);
        } else if (!leftIRSensor) {
            Serial.println("↖️ Avoiding left");
            turnLeft();
            delay(300);
        } else {
            // Both sides blocked, move backward
            Serial.println("⬇️ Moving back");
            moveBackward();
            delay(500);
            turnRight();
            delay(500);
        }
    } else if (leftIRSensor) {
        // Obstacle on left, turn right
        Serial.println("↗️ Right turn (IR left)");
        turnRight();
        delay(200);
    } else if (rightIRSensor) {
        // Obstacle on right, turn left
        Serial.println("↖️ Left turn (IR right)");
        turnLeft();
        delay(200);
    } else {
        // Path clear, move forward
        moveForward();
    }
}

float readUltrasonicDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duration == 0) {
        return 999; // Return large value if no echo received
    }
    float distance = duration * 0.034 / 2;
    return distance;
}

void moveForward() {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    setMotorSpeed(ENA, currentSpeed);
    
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    setMotorSpeed(ENB, currentSpeed);
    
    Serial.println("⬆️ Forward");
}

void moveBackward() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    setMotorSpeed(ENA, currentSpeed);
    
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    setMotorSpeed(ENB, currentSpeed);
    
    Serial.println("⬇️ Backward");
}

void turnLeft() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    setMotorSpeed(ENA, currentSpeed);
    
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    setMotorSpeed(ENB, currentSpeed);
    
    Serial.println("⬅️ Left");
}

void turnRight() {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    setMotorSpeed(ENA, currentSpeed);
    
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    setMotorSpeed(ENB, currentSpeed);
    
    Serial.println("➡️ Right");
}

void stopMotors() {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    setMotorSpeed(ENA, 0);
    
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, LOW);
    setMotorSpeed(ENB, 0);
    
    Serial.println("⏹️ Stop");
}