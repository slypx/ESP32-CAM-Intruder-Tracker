# Wiring and configuration

## Sensor-controller pin map

The supplied sketch uses the following logical mapping:

| Device or signal | Firmware pin |
| --- | ---: |
| Ultrasonic trigger | `D9` |
| Ultrasonic echo | `D10` |
| PIR digital output | `D7` |
| Gas/smoke analog output | `A0` |
| Normal buzzer | `D8` |
| Loud buzzer | `D6` |
| Status LED | `D13` |
| LiDAR/ToF data | Board-specific I²C SDA/SCL |
| LiDAR/ToF I²C address | `0x10` |

## ESP32-CAM configuration

The camera sketch uses the `esp32cam::pins::AiThinker` mapping supplied by the camera library. Select the matching AI Thinker ESP32-CAM board profile in the Arduino IDE.

Wi-Fi values belong only in a local `secrets.h` file:

```cpp
#pragma once
constexpr char WIFI_SSID[] = "YOUR_WIFI_SSID";
constexpr char WIFI_PASS[] = "YOUR_WIFI_PASSWORD";
```

## Electrical notes

- Join grounds across each subsystem before connecting signal wires.
- Verify every sensor's voltage requirements. Some ultrasonic echo outputs require level shifting before connecting to a 3.3 V MCU.
- Use a stable external supply for the ESP32-CAM; camera and Wi-Fi current spikes can cause brownouts on weak USB-serial adapters.
- Do not drive a high-current buzzer directly from a GPIO. Use an appropriate transistor/MOSFET and flyback protection when required by the load.
- MQ-style gas sensors need warm-up time and can draw substantial heater current. Power them according to their datasheet.
- Keep the ESP32-CAM flash/programming wiring separate from normal run mode, and remove the boot strap connection after uploading.

## Thresholds to calibrate

| Setting | Default | Purpose |
| --- | ---: | --- |
| `GAS_THRESHOLD` | `130` | Repeating gas alert |
| `GAS_CRITICAL_THRESHOLD` | `800` | Latched critical alarm |
| Minimum intrusion distance | `30 cm` | Near edge of trigger zone |
| Maximum intrusion distance | `80 cm` | Far edge of trigger zone |
| Sonar maximum | `200 cm` | NewPing range limit |

Do not treat the defaults as universal. Record a stable clean-air baseline, test the actual installation, and set margins appropriate to the sensor and environment.
