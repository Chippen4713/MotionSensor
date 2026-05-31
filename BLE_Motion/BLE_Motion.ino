// ESP32-C3 PIR motion sensor → Home Assistant via BTHome BLE v2
// OTA flashing over WiFi — no USB cable needed after first flash.
// HA discovers automatically: Settings → Devices & Services → Bluetooth

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>

const char* WIFI_SSID = "Occams Router 2.4GHz";
const char* WIFI_PASS = "Sofiljulemand1!";

const char* OTA_HOSTNAME = "ble-motion";
const char* OTA_PASSWORD = "Bl3-M0ti0n#OTA";

#define MOTION_PIN  2   // update once GPIO scan confirms correct pin

#define BTHOME_UUID_LO  0xD2
#define BTHOME_UUID_HI  0xFC
#define BTHOME_V2       0x40   // v2, no encryption

#define OBJ_PACKET_ID   0x00
#define OBJ_MOTION      0x21

#define HEARTBEAT_MS    30000
#define STATUS_MS        5000
#define WIFI_TIMEOUT_MS 10000

BLEAdvertising* pAdvertising;
uint8_t         packetId   = 0;
int             lastMotion = -1;
unsigned long   lastHeartbeat = 0;
unsigned long   lastStatus    = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("[WIFI] Connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500); Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" OK  IP="); Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FAILED (OTA unavailable, BLE continues)");
  }
}

void setupOTA() {
  if (WiFi.status() != WL_CONNECTED) return;
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]()   { pAdvertising->stop(); Serial.println("[OTA] Starting..."); });
  ArduinoOTA.onEnd([]()     { Serial.println("[OTA] Done — rebooting"); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    Serial.printf("[OTA] %u%%\n", done * 100 / total);
  });
  ArduinoOTA.onError([](ota_error_t err) { Serial.printf("[OTA] Error %u\n", err); });
  ArduinoOTA.begin();
  Serial.printf("[OTA] Ready — hostname: %s\n", OTA_HOSTNAME);
}

void advertise(bool motion) {
  uint8_t payload[] = {
    BTHOME_V2,
    OBJ_PACKET_ID, packetId++,
    OBJ_MOTION,    (uint8_t)(motion ? 1 : 0)
  };

  std::string raw;
  raw += (char)0x02; raw += (char)0x01; raw += (char)0x06;  // Flags

  uint8_t svcLen = 1 + 2 + sizeof(payload);
  raw += (char)svcLen;
  raw += (char)0x16;
  raw += (char)BTHOME_UUID_LO;
  raw += (char)BTHOME_UUID_HI;
  for (size_t i = 0; i < sizeof(payload); i++) raw += (char)payload[i];

  BLEAdvertisementData advData;
  advData.addData((char*)raw.c_str(), raw.length());

  BLEAdvertisementData scanData;
  scanData.setName("BLE Motion");

  pAdvertising->stop();
  pAdvertising->setAdvertisementData(advData);
  pAdvertising->setScanResponseData(scanData);
  pAdvertising->start();

  Serial.printf("[BLE] pkt=%d  motion=%s\n", packetId - 1, motion ? "DETECTED" : "CLEAR");
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);
  delay(100);

  Serial.println("\n==================================");
  Serial.println("BLE Motion Sensor — BTHome v2");
  Serial.println("==================================");

  pinMode(MOTION_PIN, INPUT_PULLDOWN);

  connectWiFi();
  setupOTA();

  BLEDevice::init("BLE Motion");
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setMinInterval(160);
  pAdvertising->setMaxInterval(160);

  lastMotion = digitalRead(MOTION_PIN);
  advertise(lastMotion);
  lastHeartbeat = millis();

  Serial.println("[SYS] Ready");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();

  unsigned long now = millis();
  int motion = digitalRead(MOTION_PIN);

  if (motion != lastMotion) {
    lastMotion    = motion;
    lastHeartbeat = now;
    Serial.printf("[MOTION] %s\n", motion ? "DETECTED" : "CLEAR");
    advertise(lastMotion);
    if (motion) {
      delay(200); advertise(lastMotion);
      delay(200); advertise(lastMotion);
    }
  }

  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = now;
    advertise(lastMotion);
  }

  if (now - lastStatus >= STATUS_MS) {
    lastStatus = now;
    Serial.printf("[ALIVE] motion=%s  pkt=%d\n", lastMotion ? "DETECTED" : "CLEAR", packetId);
    int pins[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    Serial.print("[SCAN]");
    for (int i = 0; i < 11; i++) {
      pinMode(pins[i], INPUT_PULLDOWN);
      Serial.printf(" G%d=%d", pins[i], digitalRead(pins[i]));
    }
    Serial.println();
  }

  delay(50);
}
