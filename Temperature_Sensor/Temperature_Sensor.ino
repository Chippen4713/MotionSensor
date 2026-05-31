// DS18B20 temperature sensor — ESP32-C3
// Requires: OneWire, DallasTemperature libraries
// Pin D2

#include <OneWire.h>
#include <DallasTemperature.h>

const int TEMP_PIN = 2;

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin();
  Serial.println("Temperature Sensor Started");
  delay(2000);
}

void loop() {
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  if (tempC == DEVICE_DISCONNECTED_C || tempC == -127.0) {
    Serial.println("Temp sensor error or disconnected");
  } else {
    Serial.print("Temp: ");
    Serial.print(tempC, 1);
    Serial.println(" C");
  }

  delay(2000);
}
