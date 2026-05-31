# Motion Sensor

ESP32-C3 Mini sensor node that broadcasts motion, temperature, light, and window state to Home Assistant via the BTHome BLE v2 protocol. No WiFi pairing required — HA discovers it automatically via Bluetooth.

## Variants

| Sketch | Sensors | Notes |
|---|---|---|
| `BLE_BTHome` | PIR motion, DS18B20 temp, LDR light, reed switch window | Full sensor suite |
| `BLE_Motion` | PIR motion only | Minimal build |
| `Motion_Sensor` | PIR motion | Earlier WiFi/MQTT version |
| `WiFi_MQTT_HA` | PIR motion | WiFi + MQTT direct to HA |
| `Temperature_Sensor` | DS18B20 temp | Standalone temp node |
| `LDR_Sensor` | LDR light | Standalone light node |

The `BLE_BTHome` sketch is the current recommended version.

## Hardware (BLE_BTHome)

| Component | Pin |
|---|---|
| ESP32-C3 Mini | — |
| PIR sensor | GPIO4 |
| DS18B20 temperature | GPIO2 |
| LDR (light) | GPIO1 (ADC) |
| Reed switch (window) | GPIO3 (INPUT_PULLUP) |

## How It Works

The sensor advertises a BTHome v2 BLE service data payload every time a sensor value changes. Motion and window state trigger an immediate update; temperature and light update every 30 seconds.

Home Assistant's BTHome integration discovers the device automatically — go to **Settings → Devices & Services → Bluetooth** after powering on.

## BTHome Payload

Objects are sorted by Object ID as required by the spec:

| Object | ID | Type |
|---|---|---|
| Packet counter | 0x00 | uint8 |
| Temperature | 0x02 | sint16 (factor 0.01 °C) |
| Illuminance | 0x05 | uint24 (factor 0.01, used as 0–100%) |
| Motion | 0x21 | binary uint8 |
| Window | 0x2D | binary uint8 |

## OTA Updates

WiFi is used only for OTA flashing. The sensor connects on boot, sets up ArduinoOTA, then continues operating over BLE even if WiFi is unavailable.

```
Hostname: motion-sensor
OTA password: (set in sketch)
```

## Libraries

- ESP32 Arduino core (BLE, WiFi, ArduinoOTA built-in)
- OneWire (Paul Stoffregen)
- DallasTemperature (Miles Burton)
