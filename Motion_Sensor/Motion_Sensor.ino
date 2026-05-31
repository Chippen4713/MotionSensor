// Motion sensor (PIR) — ESP32-C3
// Pin D4

const int MOTION_PIN = 4;
int lastMotionState = -1;

void setup() {
  Serial.begin(115200);
  pinMode(MOTION_PIN, INPUT);
  Serial.println("Motion Sensor Started");
  delay(2000);
}

void loop() {
  int motion = digitalRead(MOTION_PIN);

  if (motion != lastMotionState) {
    Serial.println(motion ? "Motion: DETECTED" : "Motion: NONE");
    lastMotionState = motion;
  }

  delay(100);
}
