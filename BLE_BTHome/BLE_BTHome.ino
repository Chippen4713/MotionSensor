// ESP32-C3 sensor node → Home Assistant via BTHome BLE
// OTA flashing over WiFi — no USB cable needed after first flash.
// HA discovers this automatically: Settings → Devices & Services → Bluetooth
//
// Required libraries (Arduino Library Manager):
//   - OneWire           (Paul Stoffregen)
//   - DallasTemperature (Miles Burton)
//   Built-in: ESP32 BLE + WiFi + ArduinoOTA (come with the esp32 Arduino core)

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// =====================
// WiFi (needed for OTA only)
// =====================
const char* WIFI_SSID = "Occams Router 2.4GHz";
const char* WIFI_PASS = "Sofiljulemand1!";

// =====================
// OTA
// =====================
const char* OTA_HOSTNAME = "motion-sensor";
const char* OTA_PASSWORD = "ota1234";          // change to something strong

// =====================
// Sensor pins
// =====================
#define MOTION_PIN  4   // D4 — PIR
#define TEMP_PIN    2   // D2 — DS18B20
#define LDR_PIN     1   // D1 — LDR (ADC)
#define REED_PIN    3   // D3 — reed switch (other leg to GND, INPUT_PULLUP)
                        //       LOW  = magnet present = window CLOSED
                        //       HIGH = magnet absent  = window OPEN

// LDR calibration — measure your actual ADC values in full dark / full bright
// and adjust these. Run with Serial monitor to find your range.
#define LIGHT_RAW_DARK    0     // ADC at full dark   → 0%
#define LIGHT_RAW_BRIGHT  4095  // ADC at full bright → 100%

// =====================
// BTHome v2 constants
// =====================
#define BTHOME_UUID_LO     0xD2    // Service UUID 0xFCD2, little-endian
#define BTHOME_UUID_HI     0xFC
#define BTHOME_DEVICE_INFO 0x40    // v2, no encryption

// Object IDs — MUST appear in payload sorted low → high
#define OBJ_PACKET_ID    0x00      // uint8,   no factor
#define OBJ_TEMPERATURE  0x02      // sint16,  factor 0.01 °C
#define OBJ_ILLUMINANCE  0x05      // uint24,  factor 0.01 — used for 0-100% light
#define OBJ_MOTION       0x21      // uint8,   binary (0/1)
#define OBJ_WINDOW       0x2D      // uint8,   binary (0=closed, 1=open)

// =====================
// Timing
// =====================
#define SLOW_INTERVAL_MS   30000   // temp + LDR update interval
#define WIFI_TIMEOUT_MS    10000   // max wait for WiFi on boot

// =====================
// Globals
// =====================
BLEAdvertising*  pAdvertising;
uint8_t          packetId = 0;

OneWire           oneWire(TEMP_PIN);
DallasTemperature tempSensor(&oneWire);

int           lastMotion   = -1;
int           lastReed     = -1;
float         lastTempC    = 0.0f;
int           lastLightPct = 0;
unsigned long lastSlowUpdate = 0;

// =====================
// LDR normalisation
// =====================
int ldrToPct(int raw) {
  return (int)constrain(map(raw, LIGHT_RAW_DARK, LIGHT_RAW_BRIGHT, 0, 100), 0, 100);
}

// =====================
// WiFi
// =====================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WIFI] Connecting");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" OK  IP=");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED (OTA unavailable, BLE continues)");
  }
}

// =====================
// OTA
// =====================
void setupOTA() {
  if (WiFi.status() != WL_CONNECTED) return;

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    pAdvertising->stop();   // pause BLE during flash
    Serial.println("[OTA] Starting flash...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] Done — rebooting");
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    Serial.printf("[OTA] %u%%\n", done * 100 / total);
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[OTA] Error %u\n", err);
  });

  ArduinoOTA.begin();
  Serial.printf("[OTA] Ready — hostname: %s\n", OTA_HOSTNAME);
}

// =====================
// Build & push BLE advertisement
// =====================
void advertise(bool motion, bool windowOpen, float tempC, int lightPct) {
  int16_t  tempVal  = (int16_t)(tempC * 100.0f);
  uint32_t lightEnc = (uint32_t)(lightPct * 100);   // factor 0.01 → ×100

  // BTHome service data payload (objects sorted by Object ID)
  uint8_t btData[] = {
    BTHOME_DEVICE_INFO,
    OBJ_PACKET_ID,    packetId++,
    OBJ_TEMPERATURE,  (uint8_t)(tempVal & 0xFF),    (uint8_t)((tempVal >> 8) & 0xFF),
    OBJ_ILLUMINANCE,  (uint8_t)(lightEnc & 0xFF),   (uint8_t)((lightEnc >> 8) & 0xFF), (uint8_t)((lightEnc >> 16) & 0xFF),
    OBJ_MOTION,       (uint8_t)(motion ? 1 : 0),
    OBJ_WINDOW,       (uint8_t)(windowOpen ? 1 : 0)
  };

  // Full AD payload: Flags + Service Data
  std::string raw;

  // Flags (LE General Discoverable, BR/EDR not supported)
  raw += (char)0x02;
  raw += (char)0x01;
  raw += (char)0x06;

  // Service Data — 16-bit UUID (type 0x16)
  uint8_t svcLen = 1 + 2 + sizeof(btData);   // type + UUID + btData
  raw += (char)svcLen;
  raw += (char)0x16;
  raw += (char)BTHOME_UUID_LO;
  raw += (char)BTHOME_UUID_HI;
  for (size_t i = 0; i < sizeof(btData); i++) raw += (char)btData[i];

  BLEAdvertisementData advData;
  advData.addData((char*)raw.c_str(), raw.length());

  BLEAdvertisementData scanData;
  scanData.setName("MotionSensor");

  pAdvertising->stop();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(scanData);
  pAdvertising->start();

  Serial.print("[BLE] pkt=");    Serial.print(packetId - 1);
  Serial.print(" motion=");      Serial.print(motion ? "ON" : "OFF");
  Serial.print(" window=");      Serial.print(windowOpen ? "OPEN" : "CLOSED");
  Serial.print(" temp=");        Serial.print(tempC, 1);  Serial.print("C");
  Serial.print(" light=");       Serial.print(lightPct);  Serial.println("%");
}

// =====================
// Setup
// =====================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("==================================");
  Serial.println("Motion Sensor Node — BTHome BLE");
  Serial.println("==================================");

  pinMode(MOTION_PIN, INPUT);
  pinMode(REED_PIN, INPUT_PULLUP);
  tempSensor.begin();

  connectWiFi();
  setupOTA();

  BLEDevice::init("MotionSensor");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setMinInterval(160);   // 100 ms  (unit = 0.625 ms)
  pAdvertising->setMaxInterval(160);

  // Initial sensor read so first advertisement has real values
  tempSensor.requestTemperatures();
  lastTempC    = tempSensor.getTempCByIndex(0);
  if (lastTempC == DEVICE_DISCONNECTED_C || lastTempC == -127.0f) lastTempC = 0.0f;
  lastLightPct = ldrToPct(analogRead(LDR_PIN));
  lastReed     = digitalRead(REED_PIN);
  lastSlowUpdate = millis();

  Serial.println("[SYS] Ready — motion/window immediate, temp/light every 30s");
}

// =====================
// Loop
// =====================
void loop() {
  // OTA takes priority — must be called frequently
  if (WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();

  unsigned long now = millis();

  // --- Motion: immediate on change ---
  int motion = digitalRead(MOTION_PIN);
  if (motion != lastMotion) {
    lastMotion = motion;
    Serial.print("[MOTION] ");
    Serial.println(motion ? "DETECTED" : "CLEAR");
    advertise(lastMotion, lastReed == HIGH, lastTempC, lastLightPct);
  }

  // --- Reed switch: immediate on change ---
  int reed = digitalRead(REED_PIN);
  if (reed != lastReed) {
    lastReed = reed;
    bool windowOpen = (reed == HIGH);
    Serial.print("[WINDOW] ");
    Serial.println(windowOpen ? "OPEN" : "CLOSED");
    advertise(lastMotion, windowOpen, lastTempC, lastLightPct);
  }

  // --- Slow update: temp + LDR every 30s ---
  if (now - lastSlowUpdate >= SLOW_INTERVAL_MS) {
    lastSlowUpdate = now;

    tempSensor.requestTemperatures();
    float tempC = tempSensor.getTempCByIndex(0);
    if (tempC == DEVICE_DISCONNECTED_C || tempC == -127.0f) {
      Serial.println("[SENSOR] Temp: error");
    } else {
      lastTempC = tempC;
    }
    lastLightPct = ldrToPct(analogRead(LDR_PIN));

    Serial.print("[SENSOR] Temp:");  Serial.print(lastTempC, 1);  Serial.print("C");
    Serial.print("  Light:");        Serial.print(lastLightPct);  Serial.println("%");

    advertise(lastMotion, lastReed == HIGH, lastTempC, lastLightPct);
  }

  delay(50);   // poll motion at ~20 Hz
}
