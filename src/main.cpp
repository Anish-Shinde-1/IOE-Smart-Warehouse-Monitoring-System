/**
 * IoT-Based Smart Warehouse Monitoring System
 * Platform: ESP32 (Wokwi Simulation)
 *
 * Core Objectives:
 * 1. Monitor ambient temperature/humidity, hazardous gas levels, motion intrusion, and stock levels.
 * 2. Trigger localized hardware indicators (Green/Red LEDs, Buzzer) when safety thresholds are breached.
 * 3. Serialize telemetry into lightweight JSON payload and publish over Wi-Fi via MQTT protocol.
 *
 * Pin Mapping:
 * - DHT22 Data:        GPIO 4
 * - MQ-2 Gas Analog:   GPIO 34
 * - PIR Motion Sensor: GPIO 27
 * - HC-SR04 Trig:      GPIO 5
 * - HC-SR04 Echo:      GPIO 18
 * - Green LED:         GPIO 2 (220 ohm resistor)
 * - Red LED:           GPIO 16 (220 ohm resistor)
 * - Buzzer:            GPIO 15
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// ================= Pin Definitions =================
#define DHT_PIN         4
#define GAS_PIN         34
#define PIR_PIN         27
#define TRIG_PIN        5
#define ECHO_PIN        18
#define GREEN_LED_PIN   2
#define RED_LED_PIN     16
#define BUZZER_PIN      15

// ================= Alert Thresholds =================
#define TEMP_THRESHOLD_C     30.0f   // Alert if > 30 deg C
#define GAS_THRESHOLD_RAW    300     // Alert if scaled gas_raw > 300
#define LOW_STOCK_THRESHOLD  20      // Alert if stock < 20%
#define DIST_MIN_STOCK_CM    5.0f    // 5 cm = 100% stock
#define DIST_MAX_STOCK_CM    35.0f   // 35 cm = 0% stock

// ================= Network Configuration =================
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// Public MQTT Broker
const char* MQTT_BROKER   = "broker.emqx.io";
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "warehouse/zone1/telemetry";

// Telemetry Polling Rate (2.0 seconds interval to match DHT22 0.5Hz sample rate)
const unsigned long POLL_INTERVAL_MS = 2000;

// ================= Global Instances =================
OneWire oneWire(DHT_PIN);
DallasTemperature tempSensor(&oneWire);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastPollTime = 0;
float lastValidDistance = 15.2f;
float lastValidTemp = 25.4f;


// ================= Function Declarations =================
void setupWiFi();
void reconnectMQTT();
float readUltrasonicDistance();
int calculateStockPercentage(float distanceCm);
int readCalibratedGasRaw();
void updateLocalAlarms(bool alert);
void publishTelemetry(float tempC, int gasRaw, int motion, float distanceCm, int stockPct, int alertStatus);

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n==================================================");
  Serial.println("  ESP32 SMART WAREHOUSE MONITORING NODE (IoE)    ");
  Serial.println("==================================================");

  // Use 10-bit ADC resolution (0 - 1023)
  analogReadResolution(10);

  // Initialize GPIO Pins
  pinMode(PIR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initial actuator states (Safe: Green LED ON, Red OFF, Buzzer OFF)
  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Initialize DS18B20 Sensor
  tempSensor.begin();
  Serial.println("[INIT] DS18B20 sensor initialized on GPIO 4");


  // Configure MQTT socket timeout to 1s to prevent watchdog stalls
  mqttClient.setSocketTimeout(1);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  // Connect to Wi-Fi non-blockingly
  setupWiFi();

  Serial.println("[INIT] System initialization complete. Starting monitor loop...\n");
}

// ================= Main Loop =================
void loop() {
  // Yield CPU time for ESP32 background tasks and Wi-Fi
  yield();

  // Maintain MQTT Connection non-blockingly
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
  }

  // Periodic sensor acquisition and telemetry evaluation
  unsigned long currentMillis = millis();
  if (currentMillis - lastPollTime >= POLL_INTERVAL_MS) {
    lastPollTime = currentMillis;

    // 1. Read DS18B20 Temperature
    tempSensor.requestTemperatures();
    float temperature = tempSensor.getTempCByIndex(0);
    if (temperature == DEVICE_DISCONNECTED_C || temperature < -50.0f) {
      temperature = lastValidTemp;
    } else {
      lastValidTemp = temperature;
    }

    // 2. Read MQ-2 Gas Sensor (calibrated to IoE protocol scale)
    int gasRaw = readCalibratedGasRaw();

    // 3. Read PIR Motion Sensor (GPIO 27)
    int motion = digitalRead(PIR_PIN);

    // 4. Read HC-SR04 Ultrasonic Distance (GPIO 5, 18)
    float distanceCm = readUltrasonicDistance();

    // 5. Calculate Stock Percentage (5cm=100%, 35cm=0%)
    int stockPct = calculateStockPercentage(distanceCm);

    // ==========================================
    // 6. Evaluate Alert Conditions & Grace Period
    // ==========================================
    bool highTempAlert   = (temperature > TEMP_THRESHOLD_C);
    bool gasLeakAlert    = (gasRaw > GAS_THRESHOLD_RAW);
    bool motionAlert     = (motion == HIGH);
    bool lowStockAlert   = (stockPct < LOW_STOCK_THRESHOLD);

    // 5-Second Startup Grace Period to prevent false alarms
    bool isWarmup = (millis() < 5000); 
    bool globalAlert = false;
    
    if (isWarmup) {
      // Suppress alerts while sensors stabilize
      globalAlert = false;
    } else {
      globalAlert = highTempAlert || gasLeakAlert || motionAlert || lowStockAlert;
    }
    
    int alertStatus = globalAlert ? 1 : 0;

    // 7. Update Hardware Actuators (LEDs & Buzzer)
    updateLocalAlarms(globalAlert);

    // 8. Publish Telemetry over MQTT
    publishTelemetry(temperature, gasRaw, motion, distanceCm, stockPct, alertStatus);

    // 9. Serial Diagnostics
    Serial.println("==================================================");
    Serial.printf("[1] DS18B20 Temp:  %.1f C   --> %s (Limit > %.0f C)\n",
                  temperature, highTempAlert ? "*** ALERT: HIGH TEMP ***" : "[OK]", TEMP_THRESHOLD_C);
    Serial.printf("[2] Gas (Slider):  %d        --> %s (Limit > %d)\n",
                  gasRaw, gasLeakAlert ? "*** ALERT: GAS LEAK ***" : "[OK]", GAS_THRESHOLD_RAW);
    Serial.printf("[3] PIR Motion:    %s     --> %s\n",
                  motion == HIGH ? "DETECTED" : "NONE", motionAlert ? "*** ALERT: INTRUSION ***" : "[OK]");
    Serial.printf("[4] HC-SR04 Dist:  %.1f cm   --> Stock: %d %% %s (Limit < %d %%)\n",
                  distanceCm, stockPct, lowStockAlert ? "*** ALERT: LOW STOCK ***" : "[OK]", LOW_STOCK_THRESHOLD);
    Serial.println("--------------------------------------------------");
    Serial.printf(">> SYSTEM STATUS:  %s\n", globalAlert ? ">>> ALERT ACTIVE <<<" : "NORMAL (ALL SAFE)");
    Serial.printf(">> ACTUATOR PINS:  GREEN LED: [%s] | RED LED: [%s] | BUZZER: [%s]\n",
                  globalAlert ? "OFF" : "ON",
                  globalAlert ? "ON" : "OFF",
                  globalAlert ? "ON" : "OFF");
    Serial.println("==================================================\n");
  }
}

// ================= Wi-Fi Setup =================
void setupWiFi() {
  Serial.print("[WiFi] Connecting to AP: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Quick non-blocking attempt (max 3 seconds)
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 6) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected successfully!");
    Serial.print("[WiFi] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] Connecting in background...");
  }
}

// ================= MQTT Reconnection =================
void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  static unsigned long lastReconnectAttempt = 0;
  unsigned long now = millis();

  // Retry connection every 5 seconds without blocking
  if (now - lastReconnectAttempt > 5000) {
    lastReconnectAttempt = now;
    Serial.print("[MQTT] Connecting to broker... ");

    String clientId = "ESP32-WarehouseNode-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed (rc=");
      Serial.print(mqttClient.state());
      Serial.println("), will retry...");
    }
  }
}

// ================= Gas Sensor Normalization =================
int readCalibratedGasRaw() {
  // Using Slide Potentiometer directly simulating 0-1023 analog gas level
  return analogRead(GAS_PIN);
}

// ================= Ultrasonic Measurement =================
float readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure pulse duration (timeout: 30ms -> ~5m max distance)
  long durationUs = pulseIn(ECHO_PIN, HIGH, 30000);
  if (durationUs <= 0) {
    // Retain last valid distance to avoid triggering false 0% stock alerts
    return lastValidDistance;
  }

  float distance = durationUs * 0.0343f / 2.0f;
  // Valid HC-SR04 physical range is 2cm - 400cm
  if (distance < 2.0f || distance > 400.0f) {
    return lastValidDistance;
  }

  lastValidDistance = distance;
  return distance;
}

// ================= Stock Calculation =================
int calculateStockPercentage(float distanceCm) {
  // 5 cm = 100% stock, 35 cm = 0% stock
  int stock = map((int)distanceCm, (int)DIST_MIN_STOCK_CM, (int)DIST_MAX_STOCK_CM, 100, 0);
  return constrain(stock, 0, 100);
}

// ================= Local Alarm Control =================
void updateLocalAlarms(bool alert) {
  if (alert) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }
}

// ================= Telemetry Publish =================
void publishTelemetry(float tempC, int gasRaw, int motion, float distanceCm, int stockPct, int alertStatus) {
  if (!mqttClient.connected()) {
    return;
  }

#if defined(ARDUINOJSON_VERSION_MAJOR) && (ARDUINOJSON_VERSION_MAJOR >= 7)
  JsonDocument doc;
#else
  StaticJsonDocument<256> doc;
#endif

  // Format floats to exactly 1 decimal place matching protocol requirements
  doc["temp_c"] = serialized(String(tempC, 1));
  doc["gas_raw"] = gasRaw;
  doc["motion"] = motion;
  doc["distance_cm"] = serialized(String(distanceCm, 1));
  doc["stock_pct"] = stockPct;
  doc["alert_status"] = alertStatus;

  char jsonBuffer[256];
  serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

  // Publish with 'retain=true' so the dashboard gets the latest data instantly on load
  bool published = mqttClient.publish(MQTT_TOPIC, jsonBuffer, true);
  if (published) {
    Serial.print("[MQTT PUB] Topic: ");
    Serial.println(MQTT_TOPIC);
    Serial.print("[MQTT PUB] Payload: ");
    Serial.println(jsonBuffer);
  } else {
    Serial.println("[MQTT PUB] Failed to publish message");
  }
}
