// LDR light sensor — ESP32-C3
// Pin D1 (ADC)
// Thresholds are for a 10k pull-down voltage divider; tune to your hardware.

const int LDR_PIN = 1;

// ADC range on ESP32-C3 is 0–4095
const int DARK_THRESHOLD   = 900;
const int BRIGHT_THRESHOLD = 1100;

void setup() {
  Serial.begin(115200);
  Serial.println("LDR Sensor Started");
  delay(2000);
}

void loop() {
  int raw = analogRead(LDR_PIN);

  const char* state;
  if (raw < DARK_THRESHOLD)        state = "DARK";
  else if (raw < BRIGHT_THRESHOLD) state = "MEDIUM";
  else                             state = "BRIGHT";

  Serial.print("Light raw: ");
  Serial.print(raw);
  Serial.print("  ->  ");
  Serial.println(state);

  delay(1000);
}
