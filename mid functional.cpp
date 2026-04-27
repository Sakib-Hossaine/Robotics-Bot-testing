#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Trisha Fire Service";
const char* password = "123456789";

// Pin definitions
// Flame Sensors (2 sensors only)
#define FLAME_LEFT_PIN  34   // Left flame sensor
#define FLAME_RIGHT_PIN 35   // Right flame sensor
#define RELAY_PIN       4   // Controls pump/fan for extinguisher

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

// === Flame Detection Parameters ===
int flameThreshold = 500;           // Values below this = fire detected (adjust based on your sensors)
int requiredFireConfirmations = 3;   // Consecutive readings to confirm fire
int requiredNoFireConfirmations = 3; // Consecutive readings to extinguish

// === Flame Proximity Parameters ===
int closeFlameThreshold = 300;    // Flame is very close (<300)
int farFlameMin = 300;            // Flame just visible (300-400)
int farFlameMax = 400;            // Flame a bit far

// Global variables
WebServer server(80);

bool autonomousMode = false;
bool manualMode = true;  // Start in manual mode
int currentSpeed = 200;  // Default speed (0-255)
unsigned long lastUltrasonicRead = 0;
unsigned long lastFlameRead = 0;
unsigned long lastFireAlert = 0;
const int ultrasonicInterval = 50;  // Read ultrasonic every 50ms
const int flameInterval = 100;      // Read flame sensors every 100ms
const int fireAlertInterval = 1000; // Alert every 1 second

float distance = 0;

// Flame detection variables
bool fireDetected = false;
String fireLocation = "None";
int fireCounter = 0;      // Counts consecutive fire detections
int noFireCounter = 0;    // Counts consecutive no-fire detections
bool pumpState = false;   // Current pump state
int flameProximity = 0;   // How close is the flame (0=no flame, 1=close, 2=medium, 3=far)

// Flame sensor readings (only 2 sensors)
int flameLeftValue = 1023;
int flameRightValue = 1023;

bool apMode = false;

// === Flame Direction Enum (for 2 sensors) ===
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
        
        <div class="status-panel">
            <div class="status-item">
                📏 Distance: <span id="distance" class="sensor-reading">0</span> cm
            </div>
            <div class="status-item">
                🔥 Fire Intensity: <span id="fireIntensity" class="sensor-reading">0%</span>
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
                    
                    // Update flame sensor values
                    document.getElementById('flameLeftValue').textContent = data.flameLeft;
                    document.getElementById('flameRightValue').textContent = data.flameRight;
                    
                    // Update flame sensor colors
                    const flameLeft = document.getElementById('flameLeft');
                    const flameRight = document.getElementById('flameRight');
                    
                    flameLeft.className = data.flameLeft < 500 ? 'flame-sensor fire' : 'flame-sensor';
                    flameRight.className = data.flameRight < 500 ? 'flame-sensor fire' : 'flame-sensor';
                    
                    // Update fire alert
                    const fireAlert = document.getElementById('fireAlert');
                    const fireLocationDisplay = document.getElementById('fireLocationDisplay');
                    
                    if (data.fireDetected) {
                        fireAlert.classList.add('show');
                        fireLocationDisplay.textContent = data.fireLocation;
                    } else {
                        fireAlert.classList.remove('show');
                    }
                    
                    // Update fire intensity
                    document.getElementById('fireIntensity').textContent = data.fireIntensity + '%';
                    
                    // Update pump status
                    const pumpStatus = document.getElementById('pumpStatus');
                    if (data.pumpOn) {
                        pumpStatus.textContent = '💧 Extinguisher: ON - SPRAYING!';
                        pumpStatus.className = 'pump-status pump-on';
                    } else {
                        pumpStatus.textContent = '💧 Extinguisher: OFF';
                        pumpStatus.className = 'pump-status pump-off';
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
    Serial.println("   ESP32 Fire Fighting Robot");
    Serial.println("   with 2-Sensor Flame Detection");
    Serial.println("=================================");
    
    // Initialize flame sensor pins (2 sensors only)
    pinMode(FLAME_LEFT_PIN, INPUT);
    pinMode(FLAME_RIGHT_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);  // Start with pump OFF (HIGH for active LOW relay)
    
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
    #ifdef ESP32
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
            ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);
        #else
            ledcSetup(PWM_CHANNEL_LEFT, PWM_FREQ, PWM_RESOLUTION);
            ledcSetup(PWM_CHANNEL_RIGHT, PWM_FREQ, PWM_RESOLUTION);
            ledcAttachPin(ENA, PWM_CHANNEL_LEFT);
            ledcAttachPin(ENB, PWM_CHANNEL_RIGHT);
        #endif
    #endif
    
    Serial.println("\nFlame Detection System Initialized (2 Sensors)");
    Serial.println("Motor pins initialized");
    Serial.println("PWM channels configured");
    
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
        Serial.println("\n❌ WiFi connection failed!");
        Serial.println("🔄 Creating Access Point...");
        
        WiFi.mode(WIFI_AP);
        WiFi.softAP("FireFighter_Robot", "12345678");
        
        Serial.println("\n✅ Access Point Created!");
        Serial.print("📡 AP IP address: ");
        Serial.println(WiFi.softAPIP());
        Serial.println("\n👉 Connect to WiFi network: FireFighter_Robot");
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
                digitalWrite(RELAY_PIN, HIGH);  // Turn off pump
                pumpState = false;
                Serial.println("🎮 Mode: Manual - Fire Detection Active");
            } else if (mode == "autonomous") {
                autonomousMode = true;
                manualMode = false;
                Serial.println("🤖 Mode: Autonomous - Fire Detection & Fighting Active");
            } else if (mode == "stop") {
                autonomousMode = false;
                manualMode = false;
                stopMotors();
                digitalWrite(RELAY_PIN, HIGH);  // Turn off pump
                pumpState = false;
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
    
    server.on("/sensors", HTTP_GET, []() {
        StaticJsonDocument<512> doc;
        doc["distance"] = distance;
        doc["flameLeft"] = flameLeftValue;
        doc["flameRight"] = flameRightValue;
        doc["fireDetected"] = fireDetected;
        doc["fireLocation"] = fireLocation;
        doc["fireIntensity"] = getMaxFlameIntensity();
        doc["pumpOn"] = pumpState;
        doc["flameProximity"] = flameProximity;
        
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
    Serial.println("Flame Sensors -> ESP32:");
    Serial.println("  Left Sensor  -> GPIO 34");
    Serial.println("  Right Sensor -> GPIO 35");
    Serial.println("  Relay/Pump   -> GPIO 32");
    Serial.println("\nL298N -> ESP32:");
    Serial.println("  ENA  -> GPIO 25");
    Serial.println("  ENB  -> GPIO 5");
    Serial.println("  IN1  -> GPIO 26");
    Serial.println("  IN2  -> GPIO 27");
    Serial.println("  IN3  -> GPIO 14");
    Serial.println("  IN4  -> GPIO 12");
    Serial.println("\nUltrasonic -> ESP32:");
    Serial.println("  TRIG -> GPIO 21");
    Serial.println("  ECHO -> GPIO 19");
    Serial.println("\n🔥 Fire Detection: Active in Both Manual and Autonomous Mode");
    Serial.println("=================================\n");
}

// Helper function to set motor speed
void setMotorSpeed(int pin, int speed) {
    #ifdef ESP32
        #if ESP_ARDUINO_VERSION_MAJOR >= 3
            ledcWrite(pin, speed);
        #else
            if (pin == ENA) {
                ledcWrite(PWM_CHANNEL_LEFT, speed);
            } else if (pin == ENB) {
                ledcWrite(PWM_CHANNEL_RIGHT, speed);
            }
        #endif
    #endif
}

// === FLAME DETECTION FUNCTIONS (2 Sensors) ===

void readFlameSensors() {
    flameLeftValue = analogRead(FLAME_LEFT_PIN);
    flameRightValue = analogRead(FLAME_RIGHT_PIN);
    
    Serial.print("Flame Sensors - Left: ");
    Serial.print(flameLeftValue);
    Serial.print(" | Right: ");
    Serial.println(flameRightValue);
}

FlameDirection getFlameDirection() {
    bool leftFire = (flameLeftValue < flameThreshold);
    bool rightFire = (flameRightValue < flameThreshold);
    
    if (!leftFire && !rightFire) {
        return NO_FLAME;
    }
    
    if (leftFire && rightFire) {
        return FLAME_BOTH_SIDES;
    }
    
    if (leftFire) {
        return FLAME_LEFT_SIDE;
    }
    
    if (rightFire) {
        return FLAME_RIGHT_SIDE;
    }
    
    return NO_FLAME;
}

int getFlameProximity() {
    int minValue = min(flameLeftValue, flameRightValue);
    
    if (minValue < closeFlameThreshold) {
        return 1;  // VERY CLOSE
    } else if (minValue >= farFlameMin && minValue < farFlameMax) {
        return 2;  // MEDIUM DISTANCE
    } else if (minValue < flameThreshold) {
        return 3;  // FAR but visible
    } else {
        return 0;  // NO FLAME
    }
}

int getMaxFlameIntensity() {
    int minValue = min(flameLeftValue, flameRightValue);
    
    if (minValue >= flameThreshold) {
        return 0;  // No flame
    }
    
    // Convert to percentage (lower value = higher intensity)
    int intensity = map(minValue, 0, flameThreshold, 100, 0);
    return constrain(intensity, 0, 100);
}

void controlExtinguisher() {
    bool flameDetected = (flameLeftValue < flameThreshold || 
                         flameRightValue < flameThreshold);
    
    if (flameDetected) {
        fireCounter++;
        noFireCounter = 0;
        
        if (!pumpState && fireCounter >= requiredFireConfirmations) {
            digitalWrite(RELAY_PIN, LOW);  // LOW = Pump ON (active LOW relay)
            pumpState = true;
            Serial.println("🔥🔥🔥 ACTION: Pump ACTIVATED! Extinguishing fire... 🔥🔥🔥");
        } else if (!pumpState) {
            Serial.println("⚠️ Flame detected, confirming fire... (" + 
                          String(fireCounter) + "/" + String(requiredFireConfirmations) + ")");
        }
    } else {
        fireCounter = 0;
        noFireCounter++;
        
        if (pumpState && noFireCounter >= requiredNoFireConfirmations) {
            digitalWrite(RELAY_PIN, HIGH);  // HIGH = Pump OFF
            pumpState = false;
            Serial.println("✅ Fire extinguished! Pump DEACTIVATED ✅");
        } else if (pumpState) {
            Serial.println("🧯 Confirming fire is out... (" + 
                          String(noFireCounter) + "/" + String(requiredNoFireConfirmations) + ")");
        }
    }
}

void updateFireDetection() {
    FlameDirection direction = getFlameDirection();
    flameProximity = getFlameProximity();
    
    // Update fire detection status
    fireDetected = (direction != NO_FLAME);
    
    // Update fire location
    switch (direction) {
        case NO_FLAME:
            fireLocation = "None";
            break;
        case FLAME_LEFT_SIDE:
            fireLocation = "Left Side";
            break;
        case FLAME_RIGHT_SIDE:
            fireLocation = "Right Side";
            break;
        case FLAME_BOTH_SIDES:
            fireLocation = "Both Sides";
            break;
    }
    
    // Print fire alert
    if (fireDetected && (millis() - lastFireAlert > fireAlertInterval)) {
        Serial.print("🔥 FIRE DETECTED! Location: ");
        Serial.print(fireLocation);
        Serial.print(" | Proximity: ");
        switch (flameProximity) {
            case 1: Serial.println(" VERY CLOSE!"); break;
            case 2: Serial.println(" Medium distance"); break;
            case 3: Serial.println(" Far but visible"); break;
        }
        lastFireAlert = millis();
    }
}

// === NAVIGATION FUNCTIONS ===

void navigateToFire() {
    FlameDirection direction = getFlameDirection();
    
    Serial.print("🎯 NAVIGATION: ");
    
    if (!fireDetected) {
        Serial.println("No flame detected - patrolling");
        // Simple patrol behavior - move forward
        if (distance < 20) {
            // Obstacle ahead, turn
            turnRight();
            delay(500);
        } else {
            moveForward();
        }
        return;
    }
    
    // Navigate based on flame direction and proximity
    switch (direction) {
        case FLAME_LEFT_SIDE:
            Serial.print("Flame on LEFT - ");
            if (flameProximity == 1) {
                Serial.println("TURN LEFT sharply!");
                turnLeft();
                delay(300);
            } else {
                Serial.println("Turn LEFT gradually");
                turnLeft();
                delay(150);
            }
            break;
            
        case FLAME_RIGHT_SIDE:
            Serial.print("Flame on RIGHT - ");
            if (flameProximity == 1) {
                Serial.println("TURN RIGHT sharply!");
                turnRight();
                delay(300);
            } else {
                Serial.println("Turn RIGHT gradually");
                turnRight();
                delay(150);
            }
            break;
            
        case FLAME_BOTH_SIDES:
            Serial.print("Flame on BOTH sides - ");
            if (flameProximity == 1) {
                Serial.println("FIRE SURROUNDED! Moving forward slowly");
                int originalSpeed = currentSpeed;
                currentSpeed = 150;  // Slow approach
                moveForward();
                delay(200);
                currentSpeed = originalSpeed;
            } else {
                Serial.println("Moving forward to center");
                moveForward();
                delay(100);
            }
            break;
            
        default:
            Serial.println("No clear direction - moving forward");
            moveForward();
            break;
    }
    
    // Emergency stop if flame is too close
    if (flameProximity == 1) {
        Serial.println("⚠️⚠️⚠️ FLAME IS VERY CLOSE! Activating extinguisher! ⚠️⚠️⚠️");
        stopMotors();
        // Ensure pump is on
        if (!pumpState && fireCounter >= requiredFireConfirmations) {
            digitalWrite(RELAY_PIN, LOW);
            pumpState = true;
        }
    }
}

void loop() {
    server.handleClient();
    
    unsigned long currentMillis = millis();
    
    // Read ultrasonic sensor
    if (currentMillis - lastUltrasonicRead >= ultrasonicInterval) {
        distance = readUltrasonicDistance();
        lastUltrasonicRead = currentMillis;
    }
    
    // Read flame sensors
    if (currentMillis - lastFlameRead >= flameInterval) {
        readFlameSensors();
        updateFireDetection();
        
        // Control extinguisher in both modes
        controlExtinguisher();
        
        lastFlameRead = currentMillis;
    }
    
    // Handle autonomous mode
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
    if (duration == 0) {
        return 999;
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