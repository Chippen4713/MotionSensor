#include <OneWire.h>
#include <DallasTemperature.h>

// -------- Pins --------
const int motionPin = 4;   // D4
const int tempPin = 2;     // D2 (DS18B20)
const int ldrPin = 1;      // D1 (ADC)

// -------- Motion --------
int lastMotionState = -1;

// -------- Temperature --------
OneWire oneWire(tempPin);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);

  pinMode(motionPin, INPUT);

  sensors.begin();

  Serial.println("ESP32-C3 Full Sensor Node Started");
  delay(2000);
}

void loop() {

  // =====================
  // MOTION SENSOR
  // =====================
  int motion = digitalRead(motionPin);

  if (motion != lastMotionState) {
    Serial.println(motion ? "Motion: DETECTED" : "Motion: NONE");
    lastMotionState = motion;
  }

  // =====================
  // TEMPERATURE (DS18B20)
  // =====================
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C || tempC == -127.0) {
    Serial.println("Temp sensor error");
  } else {
    Serial.print("Temp: ");
    Serial.print(tempC);
    Serial.println(" °C");
  }

  // =====================
  // LIGHT SENSOR (LDR)
  // =====================
  int lightValue = analogRead(ldrPin);

  String lightState;

  if (lightValue < 900) {
    lightState = "DARK";
  } else if (lightValue < 1100) {
    lightState = "MEDIUM";
  } else {
    lightState = "BRIGHT";
  }

  Serial.print("Light: ");
  Serial.print(lightValue);
  Serial.print(" -> ");
  Serial.println(lightState);

  Serial.println("----------------------");

  delay(2000);
}