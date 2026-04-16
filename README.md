# ELE362 Dual-Nano Space Shooter

Open-source two-board game system built with two Arduino Nano (ATmega328P) boards:

- `ELE362_Gamepad_Slave`: reads MPU6050 + buttons, sends control packets over UART
- `ELE362_Console_Master`: runs game logic, drives OLED/servo/buzzer, sends haptic command back

This guide is meant to be enough for anyone to clone the repo, wire the hardware, upload both firmwares, and run the project on their own boards.

## Repository Layout

```text
.
|- ELE362_Console_Master/
|  |- src/main.cpp
|  |- platformio.ini
|
|- ELE362_Gamepad_Slave/
|  |- src/main.c
|  |- platformio.ini
|
|- ELE362.code-workspace
`- README.md
```

## 1) Hardware Bill Of Materials

Required parts:

| Qty | Part |
|---|---|
| 2 | Arduino Nano (ATmega328P, new bootloader) |
| 1 | SSD1306 OLED display (128x64, I2C, 0x3C typical) |
| 1 | MPU6050 module (GY-521 or similar) |
| 1 | SG90/MG90 micro servo (or equivalent 5V servo) |
| 1 | Passive buzzer or small speaker module |
| 1 | 10k potentiometer (for buzzer pitch control) |
| 2 | Momentary push buttons |
| 1 | Vibration motor (coin motor or ERM) |
| 1 | NPN transistor (2N2222/BC337) or logic-level N-MOSFET |
| 1 | Flyback diode (1N4148/1N5819/1N400x) |
| 1 | Electrolytic capacitor (around 470uF-1000uF, 1000uF preferred) |
| - | Breadboard, jumpers, USB cables |

Note: External pull-up resistors are not required for buttons in this project because AVR internal pull-ups are used.

## 2) Firmware Requirements

### Master (`ELE362_Console_Master`)

- PlatformIO environment: `nanoatmega328`
- Platform: `atmelavr`
- Board: `nanoatmega328new`
- Framework: `arduino` (needed for display stack)
- Monitor speed: `38400`
- Libraries:
	- Adafruit GFX
	- Adafruit SSD1306
	- Adafruit SH110X

### Slave (`ELE362_Gamepad_Slave`)

- PlatformIO environment: `nanoatmega328`
- Platform: `atmelavr`
- Board: `nanoatmega328new`
- Monitor speed: `38400`
- Bare-metal AVR code (no extra library dependency)

## 3) Wiring Guide

Important: both boards must share ground.

### 3.1 Board-to-board UART wiring

| Gamepad Slave | Console Master | Purpose |
|---|---|---|
| D1 (TX) | D0 (RX) | Slave packet stream to Master |
| D0 (RX) | D1 (TX) | Master feedback (`0xAA`) to Slave |
| GND | GND | Common reference |

### 3.2 Console Master wiring

| Master Pin | Connect To | Notes |
|---|---|---|
| A4 (SDA) | OLED SDA | I2C data |
| A5 (SCL) | OLED SCL | I2C clock |
| D9 | Servo signal | Timer1 PWM output |
| D11 | Buzzer signal | Timer2 tone output |
| A0 | Potentiometer wiper | Pot ends to 5V and GND |
| 5V/GND | OLED, pot, servo, buzzer GND | Ensure stable supply |

### 3.3 Gamepad Slave wiring

| Slave Pin | Connect To | Notes |
|---|---|---|
| A4 (SDA) | MPU6050 SDA | I2C data |
| A5 (SCL) | MPU6050 SCL | I2C clock |
| D2 | Fire button -> GND | Internal pull-up enabled, active LOW |
| D3 | Calibrate button -> GND | Internal pull-up enabled, active LOW |
| D4 | Vibration driver input | Set HIGH on hit feedback |
| 5V/GND | MPU6050 and button ground | Common ground required |

### 3.4 Recommended safe vibration motor driver

Do not power a motor directly from Nano GPIO for real-world use.

Recommended NPN wiring:

1. Slave D4 -> NPN base (direct drive in this project build).
2. NPN emitter -> GND.
3. NPN collector -> motor negative.
4. Motor positive -> +5V external (or regulated 5V rail).
5. Flyback diode across motor terminals (cathode to +5V, anode to collector/motor-).
6. Add 470uF-1000uF electrolytic capacitor between +5V and GND near the motor supply (1000uF was used in this project to reduce voltage dips during motor startup).
7. Tie external motor supply GND to Slave GND.

## 4) Software Setup

### 4.1 Preferred path: VS Code + PlatformIO extension

1. Install VS Code.
2. Install PlatformIO IDE extension.
3. Clone this repository.
4. Open `ELE362.code-workspace` in VS Code.
5. Connect only one Nano at a time while uploading.

### 4.2 Optional CLI path

If `pio` exists in PATH:

```bash
cd ELE362_Gamepad_Slave
pio run -e nanoatmega328
pio run -e nanoatmega328 -t upload --upload-port COMx

cd ../ELE362_Console_Master
pio run -e nanoatmega328
pio run -e nanoatmega328 -t upload --upload-port COMy
```

If `pio` is not available in PATH, use PlatformIO build/upload buttons from VS Code.

## 5) Upload Order (Recommended)

1. Disconnect UART wires between boards (D0/D1) during upload.
2. Upload `ELE362_Gamepad_Slave` firmware.
3. Upload `ELE362_Console_Master` firmware.
4. Power off both boards.
5. Reconnect UART cross-wiring and common GND.
6. Power both boards again.

Why disconnect D0/D1 during flashing: Nano bootloader uses UART pins and external devices may disturb upload sync.

## 6) First Power-On Checklist

1. OLED shows the game screen.
2. Tilt gamepad board and verify ship movement.
3. Fire button shoots bullets.
4. Calibrate button recenters neutral tilt.
5. On life loss, Master sends `0xAA` and Slave vibration motor triggers.
6. Potentiometer changes buzzer pitch.

## 7) Communication Protocol

### 7.1 Slave -> Master frame (UART 38400)

5-byte fixed frame:

| Byte | Value | Meaning |
|---|---|---|
| 0 | `0xFF` | Start marker |
| 1 | `X` | Tilt X mapped to 0..253 |
| 2 | `Y` | Tilt Y mapped to 0..253 |
| 3 | `BTN` | Bit0 = fire, Bit1 = calibrate/power |
| 4 | `0xFE` | End marker |

Slave sends frames at about 60 Hz using Timer1 CTC (`OCR1A=1041`, prescaler 256 at 16 MHz).

### 7.2 Master -> Slave feedback

| Value | Meaning |
|---|---|
| `0xAA` | Life lost event, triggers vibration pulse on Slave |

## 8) Runtime Notes

- Master game loop is frame-gated to about 16 ms per update.
- Master uses Timer0 CTC 1 ms tick for internal millisecond counter.
- OLED init probes both `0x3C` and `0x3D`.
- Servo lives indicator is driven from D9 (Timer1).
- Buzzer pitch is modulated by ADC read from A0.

## 9) Troubleshooting

| Problem | Common Cause | Fix |
|---|---|---|
| Upload fails (`getsync`) | D0/D1 busy or wrong COM port | Disconnect UART wires, select correct COM port, retry |
| OLED blank | Wrong I2C wiring/address | Check A4/A5, verify 0x3C/0x3D module |
| No movement | Slave not sending frames | Confirm Slave powered and UART TX->RX cross connected |
| Buttons not working | Wrong wiring with pull-up logic | Ensure button connects pin to GND when pressed |
| No vibration | Motor driver wiring issue | Check transistor orientation, diode polarity, common GND |
| Random resets | Servo/motor current spikes | Use stronger 5V rail and common ground topology |

## 10) Open-Source Contribution

Contributions are welcome.

1. Open an issue with clear reproduction steps or feature proposal.
2. Fork and create a branch for your change.
3. Keep changes focused (firmware, docs, or hardware notes).
4. Validate build for the modified project(s).
5. Submit a pull request with what changed and how you tested it.

## 11) License

This project is licensed under the MIT License.

See `LICENSE` for full text.
