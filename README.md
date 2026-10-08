# IoT-Based Smart Warehouse Monitoring System (ESP32)

An Internet of Everything (IoE) Edge Node project powered by an ESP32 and simulated in Wokwi. This project actively monitors environmental conditions (Temperature, Gas/Smoke), motion intrusions, and physical inventory levels. It publishes real-time telemetry over Wi-Fi via MQTT and features a stunning live-updating frontend dashboard.

---

## 1. Features
* **Real-time Telemetry:** Hardware readings published every 2 seconds via MQTT over WebSockets.
* **Retained Messages:** Instantly synchronizes the web dashboard state on load.
* **Live Dashboard:** A beautiful, responsive frontend UI with glassmorphism, animated progress bars, and a pulsing global alert status banner.
* **Resilient Architecture:** Includes a 5-second startup grace period and non-blocking reconnect logic for Wi-Fi and MQTT.

## 2. Hardware Architecture & Sensors

| Component | Function | ESP32 Pin | Interface |
| :--- | :--- | :--- | :--- |
| **DS18B20** | Temperature Sensor | `GPIO 4` | Digital 1-Wire |
| **Slide Potentiometer** | Gas / Smoke Simulator | `GPIO 34` | Analog (0-1023) |
| **PIR** | Motion / Intrusion Detection | `GPIO 27` | Digital Input |
| **HC-SR04** | Ultrasonic (Stock Level) | Trig: `GPIO 5`, Echo: `GPIO 18` | Digital I/O |
| **Green LED** | Normal Status Indicator | `GPIO 2` | Digital Output |
| **Red LED** | Alert Status Indicator | `GPIO 16` | Digital Output |
| **Piezo Buzzer** | Audible Alarm | `GPIO 15` | Digital Output |

---

## 3. Alert Logic & Thresholds

The ESP32 evaluates 4 independent safety conditions. If any threshold is breached, a **Global Alert** triggers the Red LED and Buzzer, and updates the web dashboard.

1. **High Temperature**: Ambient temperature > `30.0 °C`
2. **Gas Leak Detected**: Potentiometer ADC reading > `300`
3. **Unauthorized Intrusion**: PIR sensor state == `HIGH`
4. **Low Stock Level**: Stock percentage < `20%`
   * *(Note: Ultrasonic distance is mapped mathematically from 5cm = 100% to 35cm = 0%)*

---

## 4. How to Run the Project

### Step 1: Run the ESP32 Firmware
1. Open the project in **VS Code** with the **PlatformIO** extension installed.
2. Build the firmware by running `pio run` in the terminal.
3. Install the **Wokwi for VS Code** extension.
4. Press `F1` (or `Ctrl+Shift+P`) and select **`Wokwi: Start Simulator`**.
5. The ESP32 will boot up, connect to the virtual Wi-Fi, and begin publishing MQTT data!

### Step 2: Open the Live Dashboard
1. Open your File Explorer and navigate to the `dashboard/` folder inside this project.
2. Double-click the **`index.html`** file to open it in any modern web browser.
3. The dashboard connects wirelessly to the public EMQX MQTT broker (`broker.emqx.io`) using WebSockets and instantly displays live data.

### Step 3: Interact
Interact with the components inside the Wokwi simulation (slide the temperature, trigger motion, adjust the gas potentiometer) and watch the hardware LEDs and your web dashboard react instantaneously!
