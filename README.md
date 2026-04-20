# MotionSensor

ASK 433/868 MHz wireless motion sensor prototype using the RadioHead RH_ASK library. Transmitter sends PIR trigger events, receiver prints them over Serial.

> **Status:** Prototype / development sketch. Not yet integrated with Home Assistant.

## System Overview

```
[PIR + ASK transmitter]  ---RF--->  [ASK receiver]  ---Serial--->  [PC / controller]
```

## Hardware

### Receiver — `Receiver_ASK/`

| Component | GPIO |
|-----------|------|
| ASK RX module | GPIO14 (D5 on ESP8266) |

### Transmitter — `Transmit_ASK/`

Early development firmware — also includes the servo clock calibration system and RTC menu as the clock and motion sensor were originally developed together.

## Libraries required

- `RadioHead` (RH_ASK driver)
- `SPI`
