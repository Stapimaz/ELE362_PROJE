# ELE362 Gamepad Slave

Gamepad-side firmware for the dual-Nano game system.

It reads MPU6050 tilt values and button states, then transmits fixed UART frames to the Console Master. It also listens for hit feedback (`0xAA`) and triggers vibration output.

## Quick Requirements

- Arduino Nano (ATmega328P, new bootloader)
- MPU6050 on I2C (A4/A5)
- Fire button on D2 (active LOW with internal pull-up)
- Calibrate button on D3 (active LOW with internal pull-up)
- Vibration driver control on D4
- UART link to Master on D0/D1

## Pin Summary

| Pin | Role |
|---|---|
| D0 (RX) | UART from Master D1 |
| D1 (TX) | UART to Master D0 |
| D2 | Fire button input |
| D3 | Calibrate button input |
| D4 | Vibration driver control |
| A4/A5 | MPU6050 SDA/SCL |

## Protocol

Slave sends 5-byte frames at about 60 Hz:

`0xFF, X, Y, BTN, 0xFE`

Feedback command from Master:

`0xAA` -> vibration pulse.

## Build and Upload

### PlatformIO CLI

```bash
pio run -e nanoatmega328
pio run -e nanoatmega328 -t upload --upload-port COMx
```

### VS Code PlatformIO Extension

Use Build and Upload actions in PlatformIO sidebar.

## Safety Note

Use a transistor/MOSFET + flyback diode for the vibration motor driver.

## Full System Guide

See the root guide: [../README.md](../README.md)
