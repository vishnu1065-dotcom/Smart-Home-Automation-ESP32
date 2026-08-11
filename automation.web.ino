#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <DHT.h>

// ==================== CONFIGURATION ====================
const char* ssid = "your ssid";
const char* password = "your password";


String authorizedUID = "51 AB BE 6E";

// ==================== PIN DEFINITIONS ====================
#define SS_PIN     5   // RC522 SDA
#define RST_PIN    22  // RC522 RST
#define SERVO_PIN  13  // SG90 Servo Signal
#define RELAY_PIN  26  // 2-Channel Relay IN1
#define DHT_PIN    4   // DHT11 Data Pin
#define IR_PIN     27  // IR Motion Sensor Output Pin

#define DHTTYPE    DHT11

// ==================== GLOBAL OBJECTS & VARS =============
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo doorServo;
WebServer server(80);
DHT dht(DHT_PIN, DHTTYPE);

// System State Variables
String rfidStatus = "Waiting for Card";
String doorStatus = "Locked";
int servoPos = 0;
String relayStatus = "OFF";
String lastUID = "No Card";
String wifiStatus = "Disconnected";

// Environmental & Motion State
float temperature = 0.0;
float humidity = 0.0;
String motionStatus = "No persons";

unsigned long unlockTimestamp = 0;
bool isUnlocked = false;
unsigned long lastDHTRead = 0;

// ==================== FUNCTION DECLARATIONS =============
void connectWiFi();
void setupRFID();
void setupServo();
void setupRelay();
void setupSensors();
void readRFID();
void readSensors();
void authorizeCard(String scannedUID);
void unlockDoor();
void lockDoor();
void updateRelay(bool turnOn);
void handleRoot();
void handleStatus();

// ==================== SETUP =============================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait for serial console
  
  Serial.println("\n--- Starting ESP32 Smart Home System ---");

  setupRelay();
  setupServo();
  setupRFID();
  setupSensors();
  connectWiFi();

  // Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();
  
  Serial.println("HTTP server started");
  Serial.print("Open Browser: http://");
  Serial.println(WiFi.localIP());
}

// ==================== MAIN LOOP =========================
void loop() {
  server.handleClient();
  
  readRFID();
  readSensors();

  // Auto-lock door after 50 seconds if left unlocked without a second tap
  if (isUnlocked && (millis() - unlockTimestamp >= 50000)) {
    lockDoor();
  }

  // Update WiFi status dynamically
  if (WiFi.status() == WL_CONNECTED) {
    wifiStatus = "Connected";
  } else {
    wifiStatus = "Disconnected";
  }
}

// ==================== HARDWARE & LOGIC FUNCTIONS ========
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  wifiStatus = "Connected";
  Serial.println("\nWiFi Connected successfully!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void setupRFID() {
  SPI.begin(18, 19, 23, 5); // SCK, MISO, MOSI, SS
  mfrc522.PCD_Init();
  Serial.println("RC522 RFID Reader Initialized.");
}

void setupServo() {
  ESP32PWM::allocateTimer(0);
  doorServo.setPeriodHertz(50); // Standard 50Hz servo
  doorServo.attach(SERVO_PIN, 500, 2400);
  doorServo.write(0);           // Initial position 0°
  servoPos = 0;
  Serial.println("Servo Motor Initialized at 0 degrees.");
}

void setupRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Active-LOW relay initial setup (HIGH = OFF state)
  relayStatus = "OFF";
  Serial.println("Relay Module Initialized.");
}

void setupSensors() {
  dht.begin();
  pinMode(IR_PIN, INPUT);
  Serial.println("DHT11 and IR Sensors Initialized.");
}

void readSensors() {
  // Read DHT11 non-blockingly every 2 seconds
  if (millis() - lastDHTRead >= 2000) {
    lastDHTRead = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;
    }
  }

  // Read IR Motion Sensor (LOW = Object/Motion Detected for typical IR obstacle/motion modules)
  int irVal = digitalRead(IR_PIN);
  if (irVal == LOW) {
    motionStatus = "person Detected!";
  } else {
    motionStatus = "Clear";
  }
}

void readRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println("\nCard Detected");

  String readUID = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      readUID += "0";
    }
    readUID += String(mfrc522.uid.uidByte[i], HEX);
    if (i < mfrc522.uid.size - 1) {
      readUID += " ";
    }
  }
  readUID.toUpperCase();
  
  lastUID = readUID;
  Serial.print("UID: ");
  Serial.println(lastUID);

  authorizeCard(lastUID);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void authorizeCard(String scannedUID) {
  if (scannedUID.equalsIgnoreCase(authorizedUID)) {
    if (isUnlocked) {
      Serial.println("Second Tap Detected: Turning System OFF");
      rfidStatus = "System Off";
      lockDoor();
    } else {
      Serial.println("First Tap Detected: Access Granted");
      rfidStatus = "Access Granted";
      unlockDoor();
    }
  } else {
    Serial.println("Access Denied");
    rfidStatus = "Access Denied";
    if (!isUnlocked) {
      doorStatus = "Locked";
      relayStatus = "OFF";
      servoPos = 0;
    }
  }
}

void unlockDoor() {
  Serial.println("Door Unlocked");
  doorStatus = "Unlocked";
  servoPos = 90;
  doorServo.write(servoPos);

  updateRelay(true);

  isUnlocked = true;
  unlockTimestamp = millis();
}

void lockDoor() {
  Serial.println("Door Locked");
  servoPos = 0;
  doorServo.write(servoPos);

  updateRelay(false);

  doorStatus = "Locked";
  isUnlocked = false;
  
  if (rfidStatus != "System Off") {
    rfidStatus = "Waiting for Card";
  }
}

void updateRelay(bool turnOn) {
  if (turnOn) {
    digitalWrite(RELAY_PIN, LOW); // Active-LOW relay turn ON
    relayStatus = "ON";
  } else {
    digitalWrite(RELAY_PIN, HIGH); // Active-LOW relay turn OFF
    relayStatus = "OFF";
  }
}

// ==================== WEB SERVER HANDLERS ================
void handleStatus() {
  String json = "{";
  json += "\"rfidStatus\":\"" + rfidStatus + "\",";
  json += "\"doorStatus\":\"" + doorStatus + "\",";
  json += "\"servoPos\":" + String(servoPos) + ",";
  json += "\"relayStatus\":\"" + relayStatus + "\",";
  json += "\"lastUID\":\"" + lastUID + "\",";
  json += "\"wifiStatus\":\"" + wifiStatus + "\",";
  json += "\"temperature\":\"" + String(temperature, 1) + " °C\",";
  json += "\"humidity\":\"" + String(humidity, 1) + " %\",";
  json += "\"motionStatus\":\"" + motionStatus + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Smart Home Dashboard</title>
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        body {
            background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            padding: 20px;
            color: #ffffff;
        }
        header {
            margin-bottom: 30px;
            text-align: center;
        }
        header h1 {
            font-size: 2.2rem;
            letter-spacing: 1px;
            margin-bottom: 8px;
            text-shadow: 0 2px 4px rgba(0,0,0,0.3);
        }
        header p {
            color: #b3cdd1;
            font-size: 1rem;
        }
        .grid-container {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
            gap: 20px;
            width: 100%;
            max-width: 1000px;
        }
        .card {
            background: rgba(255, 255, 255, 0.08);
            backdrop-filter: blur(10px);
            border-radius: 16px;
            padding: 24px;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(255, 255, 255, 0.1);
            transition: transform 0.3s ease, box-shadow 0.3s ease;
            position: relative;
            overflow: hidden;
        }
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 12px 40px 0 rgba(0, 0, 0, 0.4);
        }
        .card::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 4px;
        }
        .card-rfid::before { background: #00f2fe; }
        .card-door::before { background: #4facfe; }
        .card-servo::before { background: #ff0844; }
        .card-relay::before { background: #f6d365; }
        .card-uid::before { background: #b19ffb; }
        .card-temp::before { background: #ff9a9e; }
        .card-hum::before { background: #a1c4fd; }
        .card-motion::before { background: #ff0844; }
        .card-wifi::before { background: #00ff87; }

        .card-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            margin-bottom: 15px;
        }
        .card-title {
            font-size: 0.95rem;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: #d1d5db;
        }
        .card-value {
            font-size: 1.6rem;
            font-weight: 700;
            word-break: break-all;
        }
        
        .accent-rfid { color: #00f2fe; }
        .accent-door { color: #4facfe; }
        .accent-servo { color: #ff0844; }
        .accent-relay { color: #f6d365; }
        .accent-uid { color: #b19ffb; }
        .accent-temp { color: #ff9a9e; }
        .accent-hum { color: #a1c4fd; }
        .accent-motion { color: #ff758c; }
        .accent-wifi { color: #00ff87; }

        @media (max-width: 600px) {
            body { padding: 10px; }
            header h1 { font-size: 1.75rem; }
        }
    </style>
</head>
<body>

    <header>
        <h1>Smart Home Control</h1>
        <p>Real-Time ESP32 RFID & Sensor Dashboard</p>
    </header>

    <div class="grid-container">
        <!-- RFID Status -->
        <div class="card card-rfid">
            <div class="card-header">
                <span class="card-title">RFID Status</span>
            </div>
            <div class="card-value accent-rfid" id="rfidStatus">Waiting for Card</div>
        </div>

        <!-- Door Status -->
        <div class="card card-door">
            <div class="card-header">
                <span class="card-title">Door Status</span>
            </div>
            <div class="card-value accent-door" id="doorStatus">Locked</div>
        </div>

        <!-- Servo Position -->
        <div class="card card-servo">
            <div class="card-header">
                <span class="card-title">Servo Position</span>
            </div>
            <div class="card-value accent-servo" id="servoPos">0°</div>
        </div>

        <!-- Relay Status -->
        <div class="card card-relay">
            <div class="card-header">
                <span class="card-title">Relay Status</span>
            </div>
            <div class="card-value accent-relay" id="relayStatus">OFF</div>
        </div>

        <!-- Temperature -->
        <div class="card card-temp">
            <div class="card-header">
                <span class="card-title">Temperature</span>
            </div>
            <div class="card-value accent-temp" id="temperature">0.0 °C</div>
        </div>

        <!-- Humidity -->
        <div class="card card-hum">
            <div class="card-header">
                <span class="card-title">Humidity</span>
            </div>
            <div class="card-value accent-hum" id="humidity">0.0 %</div>
        </div>

        <!-- IR Motion Detection -->
        <div class="card card-motion">
            <div class="card-header">
                <span class="card-title">Motion Detection</span>
            </div>
            <div class="card-value accent-motion" id="motionStatus">Clear</div>
        </div>

        <!-- Last RFID UID -->
        <div class="card card-uid">
            <div class="card-header">
                <span class="card-title">Last RFID UID</span>
            </div>
            <div class="card-value accent-uid" id="lastUID">No Card</div>
        </div>

        <!-- WiFi Status -->
        <div class="card card-wifi">
            <div class="card-header">
                <span class="card-title">WiFi Status</span>
            </div>
            <div class="card-value accent-wifi" id="wifiStatus">Connected</div>
        </div>
    </div>

    <script>
        function updateDashboard() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('rfidStatus').innerText = data.rfidStatus;
                    document.getElementById('doorStatus').innerText = data.doorStatus;
                    document.getElementById('servoPos').innerText = data.servoPos + '°';
                    document.getElementById('relayStatus').innerText = data.relayStatus;
                    document.getElementById('temperature').innerText = data.temperature;
                    document.getElementById('humidity').innerText = data.humidity;
                    document.getElementById('motionStatus').innerText = data.motionStatus;
                    document.getElementById('lastUID').innerText = data.lastUID;
                    document.getElementById('wifiStatus').innerText = data.wifiStatus;
                })
                .catch(err => console.error('Dashboard Update Error:', err));
        }

        // Poll server for status every 1 second
        setInterval(updateDashboard, 1000);
    </script>
</body>
</html>
  )rawliteral";

  server.send(200, "text/html", html);
}
