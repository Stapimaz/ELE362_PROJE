# ELE362 Console Master

Console-side firmware for the dual-Nano game system.

It receives control packets from the Gamepad Slave over UART, runs game logic, renders to SSD1306 OLED, drives a servo life indicator, and controls a buzzer.

## Quick Requirements

- Arduino Nano (ATmega328P, new bootloader)
- SSD1306 OLED 128x64 (I2C)
- Servo on D9
- Buzzer on D11
- Potentiometer on A0
- UART link to Slave on D0/D1

## Pin Summary

| Pin | Role |
|---|---|
| D0 (RX) | UART from Slave D1 |
| D1 (TX) | UART to Slave D0 |
| D9 | Servo PWM |
| D11 | Buzzer output |
| A0 | Potentiometer input |
| A4/A5 | OLED SDA/SCL |

## Build and Upload

### PlatformIO CLI

```bash
pio run -e nanoatmega328
pio run -e nanoatmega328 -t upload --upload-port COMx
```

### VS Code PlatformIO Extension

Use Build and Upload actions in PlatformIO sidebar.

## Notes

- Upload with UART cross-wires disconnected (D0/D1) to avoid bootloader conflicts.
- Monitor speed is 38400 baud.

## Full System Guide

See the root guide: [../README.md](../README.md)
