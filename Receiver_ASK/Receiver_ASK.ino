#include <RH_ASK.h>
#include <SPI.h>

RH_ASK driver(1000, 14, -1, -1, false);  // RX on GPIO14 = D5

void setup() {
  Serial.begin(115200);
  delay(200);

  if (!driver.init()) {
    Serial.println("RX init failed");
    while (1) delay(1000);
  }

  Serial.println("RX ready");
}

void loop() {
  uint8_t buf[RH_ASK_MAX_MESSAGE_LEN];
  uint8_t buflen = sizeof(buf);

  if (driver.recv(buf, &buflen)) {
    Serial.print("Received: ");
    for (uint8_t i = 0; i < buflen; i++) {
      Serial.write(buf[i]);
    }
    Serial.println("");
  }
}