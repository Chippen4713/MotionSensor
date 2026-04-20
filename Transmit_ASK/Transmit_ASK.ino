///////////////////
// Module Import //
///////////////////
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <virtuabotixRTC.h>

//////////////////////////
// Initialize Variables //
//////////////////////////
Adafruit_PWMServoDriver board1(0x40);
Adafruit_PWMServoDriver board2(0x41);

// DS1302 style RTC: CLK, DAT, RST
virtuabotixRTC rtc(D5, D6, D7);

struct ServoCal {
  uint16_t open;
  uint16_t closed;
};

ServoCal cal[28];
uint16_t currentPos[28];

int selectedServo = 0;
const char* CAL_FILE = "/calibration.json";

//////////////////////
// Pin Definitions  //
//////////////////////
const int PIR_PIN = D8;   // Warning: boot strap pin on ESP8266

//////////////////////
// WiFi placeholders//
//////////////////////
String wifiSSID = "";
String wifiPASS = "";

//////////////////////
// App Mode System  //
//////////////////////
enum AppMode {
  MODE_MAIN,
  MODE_SERVO_CAL,
  MODE_RTC,
  MODE_MOTION,
  MODE_WIFI,
  MODE_RUN
};

AppMode currentMode = MODE_MAIN;

//////////////////////
// Helper functions //
//////////////////////

void setClockServoRaw(int servoNum, uint16_t pulse) {
  pulse = constrain(pulse, 220, 420);

  if (servoNum >= 0 && servoNum <= 13) {
    board1.setPWM(servoNum, 0, pulse);
  } else if (servoNum >= 14 && servoNum <= 27) {
    board2.setPWM(servoNum - 14, 0, pulse);
  }
}

void moveSelected(int delta) {
  currentPos[selectedServo] = constrain((int)currentPos[selectedServo] + delta, 220, 420);
  setClockServoRaw(selectedServo, currentPos[selectedServo]);
  Serial.printf("Servo %d current = %u\n", selectedServo, currentPos[selectedServo]);
}

void printServo(int s) {
  Serial.printf("Servo %d -> current=%u open=%u closed=%u\n",
                s, currentPos[s], cal[s].open, cal[s].closed);
}

void printAll() {
  for (int i = 0; i < 28; i++) {
    Serial.printf("%2d: open=%u closed=%u current=%u\n",
                  i, cal[i].open, cal[i].closed, currentPos[i]);
  }
}

bool saveCalibration() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < 28; i++) {
    JsonObject obj = arr.createNestedObject();
    obj["open"] = cal[i].open;
    obj["closed"] = cal[i].closed;
  }

  File f = LittleFS.open(CAL_FILE, "w");
  if (!f) {
    Serial.println("Failed to open calibration file for writing");
    return false;
  }

  size_t written = serializeJsonPretty(doc, f);
  f.close();

  if (written == 0) {
    Serial.println("Failed to write calibration file");
    return false;
  }

  Serial.println("Calibration saved");
  return true;
}

bool loadCalibration() {
  if (!LittleFS.exists(CAL_FILE)) {
    Serial.println("Calibration file not found, using defaults");
    return false;
  }

  File f = LittleFS.open(CAL_FILE, "r");
  if (!f) {
    Serial.println("Failed to open calibration file");
    return false;
  }

  DynamicJsonDocument doc(4096);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.printf("JSON read failed: %s\n", err.c_str());
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() != 28) {
    Serial.println("Calibration file has wrong servo count");
    return false;
  }

  for (int i = 0; i < 28; i++) {
    cal[i].open = arr[i]["open"] | 340;
    cal[i].closed = arr[i]["closed"] | 300;
  }

  Serial.println("Calibration loaded");
  return true;
}

void setDefaults() {
  for (int i = 0; i < 28; i++) {
    cal[i].open = 340;
    cal[i].closed = 300;
    currentPos[i] = cal[i].closed;
  }
}

void moveToStored(int servoNum, char state) {
  if (state == 'o') {
    currentPos[servoNum] = cal[servoNum].open;
  } else if (state == 'c') {
    currentPos[servoNum] = cal[servoNum].closed;
  } else {
    return;
  }

  setClockServoRaw(servoNum, currentPos[servoNum]);
  printServo(servoNum);
}

////////////////////
// RTC functions  //
////////////////////

void printRTCNow() {
  rtc.updateTime();
  Serial.printf("RTC: %04d-%02d-%02d  %02d:%02d:%02d\n",
                rtc.year, rtc.month, rtc.dayofmonth,
                rtc.hours, rtc.minutes, rtc.seconds);
}

void setRTCFromCompileTime() {
  // Manual one-time set fallback example
  // seconds, minutes, hours, dayofweek, dayofmonth, month, year
  rtc.setDS1302Time(0, 0, 12, 1, 6, 4, 2026);
  Serial.println("RTC set to fixed compile-time example value");
  printRTCNow();
}

/////////////////////
// WiFi functions  //
/////////////////////

void printWiFiStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("RSSI: ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("WiFi not connected");
  }
}

void connectWiFi() {
  if (wifiSSID.length() == 0) {
    Serial.println("WiFi SSID not set. Use:");
    Serial.println("ssid YourNetworkName");
    Serial.println("pass YourPassword");
    return;
  }

  Serial.printf("Connecting to %s\n", wifiSSID.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  printWiFiStatus();
}

/////////////////////////
// Menu Print Functions//
/////////////////////////

void printMainMenu() {
  Serial.println();
  Serial.println("=========== MAIN MENU ===========");
  Serial.println("1  -> Servo calibration");
  Serial.println("2  -> RTC menu");
  Serial.println("3  -> Motion sensor menu");
  Serial.println("4  -> WiFi / Internet menu");
  Serial.println("5  -> Run clock mode");
  Serial.println("6  -> Show calibration");
  Serial.println("m  -> Show main menu");
  Serial.println("=================================");
}

void printServoMenu() {
  Serial.println();
  Serial.println("------ SERVO CALIBRATION ------");
  Serial.println("s <n>      select servo 0..27");
  Serial.println("+          move selected +5");
  Serial.println("-          move selected -5");
  Serial.println("++         move selected +1");
  Serial.println("--         move selected -1");
  Serial.println("o          save current as OPEN");
  Serial.println("c          save current as CLOSED");
  Serial.println("to         move selected to OPEN");
  Serial.println("tc         move selected to CLOSED");
  Serial.println("p          print selected");
  Serial.println("pa         print all");
  Serial.println("w          write calibration file");
  Serial.println("l          load calibration file");
  Serial.println("m          back to main menu");
  Serial.println("-------------------------------");
}

void printRTCMenu() {
  Serial.println();
  Serial.println("------ RTC MENU ------");
  Serial.println("status           show RTC time");
  Serial.println("setfixed         set RTC to fixed example time");
  Serial.println("watch            print RTC time 10 times");
  Serial.println("m                back to main menu");
  Serial.println("----------------------");
}

void printMotionMenu() {
  Serial.println();
  Serial.println("------ MOTION MENU ------");
  Serial.println("read       read PIR pin");
  Serial.println("watch      live PIR monitor for 10 sec");
  Serial.println("m          back to main menu");
  Serial.println("-------------------------");
}

void printWiFiMenu() {
  Serial.println();
  Serial.println("------ WIFI MENU ------");
  Serial.println("status                 show WiFi status");
  Serial.println("scan                   scan WiFi");
  Serial.println("ssid <name>            set WiFi SSID");
  Serial.println("pass <password>        set WiFi password");
  Serial.println("connect                connect WiFi");
  Serial.println("disconnect             disconnect WiFi");
  Serial.println("m                      back to main menu");
  Serial.println("-----------------------");
}

////////////////////////
// Mode Change Helper //
////////////////////////

void setMode(AppMode newMode) {
  currentMode = newMode;

  switch (currentMode) {
    case MODE_MAIN:
      printMainMenu();
      break;
    case MODE_SERVO_CAL:
      printServoMenu();
      break;
    case MODE_RTC:
      printRTCMenu();
      break;
    case MODE_MOTION:
      printMotionMenu();
      break;
    case MODE_WIFI:
      printWiFiMenu();
      break;
    case MODE_RUN:
      Serial.println("Clock run mode started");
      Serial.println("Type m to return to main menu");
      break;
  }
}

///////////////////////
// Command Handlers  //
///////////////////////

void handleServoCommand(String cmd) {
  cmd.trim();

  if (cmd == "m") {
    setMode(MODE_MAIN);
  } else if (cmd.startsWith("s ")) {
    int n = cmd.substring(2).toInt();
    if (n >= 0 && n < 28) {
      selectedServo = n;
      Serial.printf("Selected servo %d\n", selectedServo);
      printServo(selectedServo);
    } else {
      Serial.println("Invalid servo number");
    }
  } else if (cmd == "+") {
    moveSelected(5);
  } else if (cmd == "-") {
    moveSelected(-5);
  } else if (cmd == "++") {
    moveSelected(1);
  } else if (cmd == "--") {
    moveSelected(-1);
  } else if (cmd == "o") {
    cal[selectedServo].open = currentPos[selectedServo];
    Serial.printf("Servo %d OPEN saved = %u\n", selectedServo, cal[selectedServo].open);
  } else if (cmd == "c") {
    cal[selectedServo].closed = currentPos[selectedServo];
    Serial.printf("Servo %d CLOSED saved = %u\n", selectedServo, cal[selectedServo].closed);
  } else if (cmd == "to") {
    moveToStored(selectedServo, 'o');
  } else if (cmd == "tc") {
    moveToStored(selectedServo, 'c');
  } else if (cmd == "p") {
    printServo(selectedServo);
  } else if (cmd == "pa") {
    printAll();
  } else if (cmd == "w") {
    saveCalibration();
  } else if (cmd == "l") {
    loadCalibration();
  } else {
    Serial.println("Unknown servo command");
  }
}

void handleRTCCommand(String cmd) {
  cmd.trim();

  if (cmd == "m") {
    setMode(MODE_MAIN);
  } else if (cmd == "status") {
    printRTCNow();
  } else if (cmd == "setfixed") {
    setRTCFromCompileTime();
  } else if (cmd == "watch") {
    for (int i = 0; i < 10; i++) {
      printRTCNow();
      delay(1000);
    }
  } else {
    Serial.println("Unknown RTC command");
  }
}

void handleMotionCommand(String cmd) {
  cmd.trim();

  if (cmd == "m") {
    setMode(MODE_MAIN);
  } else if (cmd == "read") {
    Serial.printf("PIR state = %d\n", digitalRead(PIR_PIN));
  } else if (cmd == "watch") {
    Serial.println("Watching PIR for 10 seconds...");
    unsigned long start = millis();
    while (millis() - start < 10000) {
      Serial.printf("PIR = %d\n", digitalRead(PIR_PIN));
      delay(500);
    }
    Serial.println("Done");
  } else {
    Serial.println("Unknown motion command");
  }
}

void handleWiFiCommand(String cmd) {
  cmd.trim();

  if (cmd == "m") {
    setMode(MODE_MAIN);
  } else if (cmd == "status") {
    printWiFiStatus();
  } else if (cmd == "scan") {
    Serial.println("Scanning WiFi...");
    int n = WiFi.scanNetworks();
    if (n <= 0) {
      Serial.println("No networks found");
    } else {
      for (int i = 0; i < n; i++) {
        Serial.printf("%d: %s (%d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
      }
    }
  } else if (cmd.startsWith("ssid ")) {
    wifiSSID = cmd.substring(5);
    Serial.print("SSID set to: ");
    Serial.println(wifiSSID);
  } else if (cmd.startsWith("pass ")) {
    wifiPASS = cmd.substring(5);
    Serial.println("Password updated");
  } else if (cmd == "connect") {
    connectWiFi();
  } else if (cmd == "disconnect") {
    WiFi.disconnect();
    Serial.println("WiFi disconnected");
  } else {
    Serial.println("Unknown WiFi command");
  }
}

void handleMainCommand(String cmd) {
  cmd.trim();

  if (cmd == "1") {
    setMode(MODE_SERVO_CAL);
  } else if (cmd == "2") {
    setMode(MODE_RTC);
  } else if (cmd == "3") {
    setMode(MODE_MOTION);
  } else if (cmd == "4") {
    setMode(MODE_WIFI);
  } else if (cmd == "5") {
    setMode(MODE_RUN);
  } else if (cmd == "6") {
    printAll();
  } else if (cmd == "m") {
    printMainMenu();
  } else {
    Serial.println("Unknown main menu command");
  }
}

void handleRunCommand(String cmd) {
  cmd.trim();

  if (cmd == "m") {
    setMode(MODE_MAIN);
  } else if (cmd == "time") {
    printRTCNow();
  } else {
    Serial.println("Run mode placeholder. Type m for main menu.");
  }
}

void routeCommand(String cmd) {
  switch (currentMode) {
    case MODE_MAIN:
      handleMainCommand(cmd);
      break;
    case MODE_SERVO_CAL:
      handleServoCommand(cmd);
      break;
    case MODE_RTC:
      handleRTCCommand(cmd);
      break;
    case MODE_MOTION:
      handleMotionCommand(cmd);
      break;
    case MODE_WIFI:
      handleWiFiCommand(cmd);
      break;
    case MODE_RUN:
      handleRunCommand(cmd);
      break;
  }
}

///////////
// Setup //
///////////

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("====================================");
  Serial.println("Servo Clock Program (06/04/2026 kem)");
  Serial.println("====================================");

  pinMode(PIR_PIN, INPUT);

  Wire.begin(D2, D1);

  board1.begin();
  board2.begin();
  board1.setPWMFreq(50);
  board2.setPWMFreq(50);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
  }

  setDefaults();
  loadCalibration();

  // Safe startup: load positions into memory only
  for (int i = 0; i < 28; i++) {
    currentPos[i] = cal[i].closed;
  }

  setMode(MODE_MAIN);
  printServo(selectedServo);
}

//////////
// Loop //
//////////

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    routeCommand(cmd);
  }
}