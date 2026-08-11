 🏠 Smart Home Automation Mini Project Using ESP32
 📌 Project Overview

The Smart Home Automation Mini Project is an ESP32-based IoT system designed to monitor and control different home automation functions through a single web interface.

The system integrates RFID authentication, relay-based appliance control, servo motor control, DHT11 temperature and humidity monitoring, and IR sensor detection.

The ESP32 connects to Wi-Fi and hosts a web-based dashboard where sensor information can be monitored and devices can be controlled remotely.

---

🎯 Aim

To develop a smart home automation system using ESP32 that combines multiple sensors and actuators into a single web-based control and monitoring interface.

---
🛠️ Components Required

- ESP32 Development Board
- Relay Module
- SG90 Servo Motor
- DHT11 Temperature & Humidity Sensor
- IR Sensor / IR Receiver
- RFID Reader Module
- RFID Tags/Cards
- Jumper Wires
- Breadboard
- Power Supply
- Wi-Fi Network

---

⚙️ Working Principle

1. The ESP32 connects to a Wi-Fi network.
2. The ESP32 hosts a web-based control dashboard.
3. DHT11 continuously measures temperature and humidity.
4. The measured environmental data is displayed on the web page.
5. The IR sensor detects the presence of an object or motion and reports its status.
6. The RFID reader authenticates the user using an RFID card/tag.
7. After authentication, the system provides access to the required control functions.
8. The relay module is used to switch home appliances such as a light or fan ON/OFF.
9. The servo motor is controlled from the web interface to simulate door or window control.
10. All sensors and actuators are monitored and controlled through a single web dashboard.

---

🌐 Web Interface

The ESP32 provides a web-based dashboard containing:

- System status
- RFID authentication status
- Temperature
- Humidity
- IR sensor status
- Relay/appliance control
- Servo motor control
- Device connection status

The user can access the dashboard using the IP address provided by the ESP32.

---

🔐 RFID Authentication

The RFID module is used to identify an authorized RFID card or tag.

The authentication process helps provide controlled access to the home automation system.

### Authentication Flow

```text
RFID Card/Tag
      ↓
RFID Reader
      ↓
ESP32
      ↓
UID Verification
      ↓
Access Granted / Denied
      ↓
Home Control Functions
