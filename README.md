# Transformer_Winding

An ESP32-based wire spooler using a stepper motor and web dashboard. Configure layers and turns via a browser, and control the motor with a button or remotely.

## Features
- Stepper motor control (DIR, STEP, EN)
- Start/Pause/Resume with button or web interface
- Real-time layer/turn tracking
- Acceleration/deceleration logic
- Wi-Fi dashboard to set layers/turns

## Hardware
- ESP32 Dev Module  
- Stepper motor & driver (e.g. A4988)  
- Push button  
- External power supply

## Pinout
| Function | ESP32 Pin |
|----------|-----------|
| DIR      | 25        |
| STEP     | 26        |
| EN       | 27        |
| Button   | 32        |

## Wi-Fi Setup
Update in code:
```cpp
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
