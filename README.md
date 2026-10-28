# STM32 FreeRTOS Access Control Terminal

A door access terminal on an STM32F446RE (NUCLEO-F446RE). You present an
RFID card, type your PIN on a keypad, and a servo unlocks the door. Users
and an access log live in an I2C EEPROM, and an admin shell on the serial
port manages it all.

It runs on FreeRTOS. Every peripheral driver is written on the registers,
with no vendor HAL. The same firmware image runs on the real board and in
[EmbSIM](https://github.com/EmbLinkOs/EmbSIM), a circuit and firmware
simulator. There, scripted scenarios check what the terminal does (console,
OLED pixels, servo, buzzer, LEDs) on every change.

> **Status: in development.** The platform, drivers, display and shell are
> in; the access logic, its storage and the full scenario suite are being
> written. See [Progress](#progress).

## Features

- **Two-factor access:** MIFARE Classic card (MFRC522 on SPI) and a PIN on
  a 4x4 keypad. Lockout after repeated wrong PINs.
- **Users and access log** in a 24LC256 EEPROM (32 KB), with CRC-checked
  records that survive power loss.
- **Real-time clock:** DS3231 with a coin cell. Log entries are timestamped,
  and the terminal knows when its time can't be trusted.
- **128x64 OLED UI:** clock, PIN entry, granted, denied, lockout,
  door-open and alarm screens.
- **Door supervision:** servo lock, door contact and exit button.
  Door held open too long and door forced open raise alarms.
- **Admin shell** over the ST-LINK's virtual COM port: users, log, time,
  diagnostics (`top`, `i2c`, `faults`).
- **Robustness:** independent watchdog fed by a supervisor task. Fault
  records (hard fault with the stacked PC/LR and CFSR, assertion, stack
  overflow, out of memory) survive the reset and are reported at the next
  boot. I2C bus recovery when a slave holds SDA.

## Hardware

| Part | Interface | NUCLEO pins |
|------|-----------|-------------|
| MFRC522 RFID reader (RC522 module) | SPI1 | D3 SCK, D5 MISO, D4 MOSI, D10 CS, D7 RST |
| SSD1306 128x64 OLED | I2C1 (0x3C) | D15 SCL, D14 SDA |
| DS3231 RTC (ZS-042 module) | I2C1 (0x68) | D15 SCL, D14 SDA |
| 24LC256 EEPROM | I2C1 (0x50) | D15 SCL, D14 SDA |
| 4x4 membrane keypad | GPIO matrix | rows A0..A3, columns CN7 pins 1..4 |
| SG90 servo (the lock) | TIM2_CH3 PWM | D6 |
| Passive buzzer | TIM3_CH2 PWM | D9 |
| Green / red LEDs | GPIO | D11 / D12 |
| Exit button, door reed contact | GPIO, pull-up | A4 / A5 |
| Console and admin shell | USART2, 115200 8N1 | ST-LINK virtual COM port |

The full pin map is in [`firmware/platform/board.h`](firmware/platform/board.h).

## Building

The firmware needs the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-gcc`), CMake 3.20 or later and Ninja.

```sh
cmake --preset firmware
cmake --build --preset firmware        # build/firmware/access-terminal.elf and .bin
```

Unit tests of the portable code (formatting, shell, dates, graphics,
screens) run on the host with the address and undefined behaviour
sanitizers:

```sh
cmake --preset host
cmake --build --preset host
ctest --preset host
```

To flash a NUCLEO, copy `build/firmware/access-terminal.bin` to the board's
USB drive, or use STM32CubeProgrammer or `st-flash`.

## Simulation

[`sim/project.json`](sim/project.json) wires the whole terminal around the
NUCLEO-F446RE in EmbSIM. Each scenario in [`sim/scenarios`](sim/scenarios)
drives inputs (cards, keys, serial text) and checks the outputs:

```sh
embsim run sim/project.json --scenario sim/scenarios/boot.json
```

## Layout

```
firmware/
  platform/   startup, linker script, clocks, fault record, FreeRTOS config, pin map
  drivers/    GPIO, UART, I2C, SPI, timer PWM, watchdog (registers, no HAL)
  devices/    MFRC522, SSD1306, DS3231, 24LC256, keypad
  lib/        portable code, unit-tested on the host
  app/        tasks: supervisor, UI, admin shell, clock
sim/          EmbSIM circuit and scenarios
tests/        host unit tests
tools/        OLED screenshot renderer
third_party/  FreeRTOS kernel, CMSIS, ST's device header (with their licenses)
```

## Progress

- [x] Clock tree at 180 MHz, fault record, console, admin shell (passing in EmbSIM)
- [x] Screens and the portable library (host unit tests)
- [ ] I2C bus with the DS3231, 24LC256 and SSD1306: written, being verified in EmbSIM
- [ ] SPI with the MFRC522, keypad, PWM, watchdog: written, not yet verified
- [ ] Access logic, user database and access log in EEPROM
- [ ] Full EmbSIM scenario suite
- [ ] Wiring diagram, screenshots, parts list
- [ ] ESP32-C3 Wi-Fi gateway for the access log
