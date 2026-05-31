#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================
// Wi-Fi
// =====================
const char* WIFI_SSID = "Occams Router 2.4GHz";
const char* WIFI_PASS = "Sofiljulemand1!";

// =====================
// MQTT / Home Assistant
// =====================
const char* MQTT_HOST = "192.168.1.58";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USER = "HA";
const char* MQTT_PASS = "Sofil123";

// =====================
// MQTT topics
// =====================
const char* TOPIC_STATUS      = "motion_sensor/status";
const char* TOPIC_MOTION      = "motion_sensor/state/motion";
const char* TOPIC_TEMP        = "motion_sensor/state/temperature";
const char* TOPIC_LIGHT_RAW   = "motion_sensor/state/light_raw";
const char* TOPIC_LIGHT_STATE = "motion_sensor/state/light_state";

// =====================
// Sensor pins
// =====================
const int MOTION_PIN = 4;   // D4 — PIR
const int TEMP_PIN   = 2;   // D2 — DS18B20
const int LDR_PIN    = 1;   // D1 — LDR (ADC)

// LDR thresholds — tune to your voltage divider
const int DARK_THRESHOLD   = 900;
const int BRIGHT_THRESHOLD = 1100;

// =====================
// Globals
// =====================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
OneWire oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

int lastMotion = -1;
unsigned long lastMqttReconnectAttempt = 0;

// =====================
// Utility
// =====================
void mqttPublishRetained(const char* topic, const String& payload) {
  bool ok = mqtt.publish(topic, payload.c_str(), true);

  Serial.print("[MQTT] ");
  Serial.print(ok ? "Published " : "FAILED ");
  Serial.print(topic);
  Serial.print(" = ");
  Serial.println(payload);
}

// =====================
// HA MQTT Discovery
// =====================
void publishDiscovery() {
  Serial.println("[HA] Publishing MQTT discovery...");

  mqttPublishRetained(
    "homeassistant/binary_sensor/motion_sensor_motion/config",
    "{"
    "\"name\":\"Motion\","
    "\"unique_id\":\"motion_sensor_motion\","
    "\"state_topic\":\"motion_sensor/state/motion\","
    "\"payload_on\":\"ON\","
    "\"payload_off\":\"OFF\","
    "\"device_class\":\"motion\","
    "\"availability_topic\":\"motion_sensor/status\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"device\":{"
      "\"identifiers\":[\"motion_sensor_node\"],"
      "\"name\":\"Motion Sensor Node\","
      "\"manufacturer\":\"Custom\","
      "\"model\":\"ESP32-C3 Sensor\""
    "}"
    "}"
  );

  mqttPublishRetained(
    "homeassistant/sensor/motion_sensor_temperature/config",
    "{"
    "\"name\":\"Temperature\","
    "\"unique_id\":\"motion_sensor_temperature\","
    "\"state_topic\":\"motion_sensor/state/temperature\","
    "\"unit_of_measurement\":\"\\u00b0C\","
    "\"device_class\":\"temperature\","
    "\"state_class\":\"measurement\","
    "\"availability_topic\":\"motion_sensor/status\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"device\":{"
      "\"identifiers\":[\"motion_sensor_node\"],"
      "\"name\":\"Motion Sensor Node\","
      "\"manufacturer\":\"Custom\","
      "\"model\":\"ESP32-C3 Sensor\""
    "}"
    "}"
  );

  mqttPublishRetained(
    "homeassistant/sensor/motion_sensor_light_raw/config",
    "{"
    "\"name\":\"Light Level (raw)\","
    "\"unique_id\":\"motion_sensor_light_raw\","
    "\"state_topic\":\"motion_sensor/state/light_raw\","
    "\"state_class\":\"measurement\","
    "\"icon\":\"mdi:brightness-6\","
    "\"availability_topic\":\"motion_sensor/status\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"device\":{"
      "\"identifiers\":[\"motion_sensor_node\"],"
      "\"name\":\"Motion Sensor Node\","
      "\"manufacturer\":\"Custom\","
      "\"model\":\"ESP32-C3 Sensor\""
    "}"
    "}"
  );

  mqttPublishRetained(
    "homeassistant/sensor/motion_sensor_light_state/config",
    "{"
    "\"name\":\"Light State\","
    "\"unique_id\":\"motion_sensor_light_state\","
    "\"state_topic\":\"motion_sensor/state/light_state\","
    "\"icon\":\"mdi:weather-sunny\","
    "\"availability_topic\":\"motion_sensor/status\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"device\":{"
      "\"identifiers\":[\"motion_sensor_node\"],"
      "\"name\":\"Motion Sensor Node\","
      "\"manufacturer\":\"Custom\","
      "\"model\":\"ESP32-C3 Sensor\""
    "}"
    "}"
  );

  Serial.println("[HA] Discovery published");
}

// =====================
// MQTT connect
// =====================
bool connectMqtt() {
  String clientId = "motion-sensor-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("[MQTT] Connecting to ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);

  // LWT: broker publishes "offline" if device drops unexpectedly
  bool ok = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASS, TOPIC_STATUS, 1, true, "offline");

  if (ok) {
    Serial.println("[MQTT] Connected");
    mqtt.publish(TOPIC_STATUS, "online", true);
    publishDiscovery();
    return true;
  }

  Serial.print("[MQTT] Connect failed, rc=");
  Serial.println(mqtt.state());
  return false;
}

// =====================
// WiFi connect
// =====================
void ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("[WIFI] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.print("[WIFI] Connected, IP=");
  Serial.println(WiFi.localIP());
}

// =====================
// Setup
// =====================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==================================");
  Serial.println("Motion Sensor Node Booting");
  Serial.println("==================================");

  pinMode(MOTION_PIN, INPUT);
  tempSensor.begin();

  ensureWifi();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setKeepAlive(60);
  mqtt.setSocketTimeout(10);
  mqtt.setBufferSize(1024);

  if (connectMqtt()) {
    Serial.println("[SYS] MQTT ready");
  } else {
    Serial.println("[SYS] MQTT initial connect failed");
  }

  Serial.println("[SYS] Sensor node fully ready");
}

// =====================
// Loop
// =====================
void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost connection, reconnecting...");
    ensureWifi();
  }

  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      if (connectMqtt()) {
        lastMqttReconnectAttempt = 0;
        Serial.println("[MQTT] Reconnected");
      }
    }
  } else {
    mqtt.loop();
  }

  // --- Motion (publish only on change) ---
  int motion = digitalRead(MOTION_PIN);
  if (motion != lastMotion) {
    mqttPublishRetained(TOPIC_MOTION, motion ? "ON" : "OFF");
    Serial.print("[SENSOR] Motion: ");
    Serial.println(motion ? "ON" : "OFF");
    lastMotion = motion;
  }

  // --- Temperature ---
  tempSensor.requestTemperatures();
  float tempC = tempSensor.getTempCByIndex(0);

  if (tempC != DEVICE_DISCONNECTED_C && tempC != -127.0) {
    char tempStr[8];
    dtostrf(tempC, 4, 1, tempStr);
    mqttPublishRetained(TOPIC_TEMP, String(tempStr));
    Serial.print("[SENSOR] Temp: ");
    Serial.print(tempStr);
    Serial.println(" C");
  } else {
    Serial.println("[SENSOR] Temp: sensor error");
  }

  // --- LDR ---
  int raw = analogRead(LDR_PIN);
  mqttPublishRetained(TOPIC_LIGHT_RAW, String(raw));

  const char* lightState;
  if (raw < DARK_THRESHOLD)        lightState = "DARK";
  else if (raw < BRIGHT_THRESHOLD) lightState = "MEDIUM";
  else                             lightState = "BRIGHT";
  mqttPublishRetained(TOPIC_LIGHT_STATE, String(lightState));

  Serial.print("[SENSOR] Light: ");
  Serial.print(raw);
  Serial.print(" -> ");
  Serial.println(lightState);

  Serial.println("---");
  delay(2000);
}
