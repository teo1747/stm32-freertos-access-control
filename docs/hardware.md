# Hardware

Everything is a common breakout module on the NUCLEO-F446RE's Arduino
headers, with jumper wires and a breadboard. No soldering beyond header
pins where a module ships without them.

![Wiring](images/wiring.svg)

## Parts

| Qty | Part | Notes |
|----:|------|-------|
| 1 | NUCLEO-F446RE | The ST-LINK on board flashes it and carries the console over USB |
| 1 | RC522 RFID module (MFRC522) | 13.56 MHz; usually sold with a MIFARE Classic 1K card and key fob |
| 1 | SSD1306 0.96" OLED, 128x64, **I2C** (4 pins) | Not the 7-pin SPI version |
| 1 | DS3231 RTC module (ZS-042 or "mini") | With a CR2032 cell. On the ZS-042 module, remove the charging diode or its resistor if you fit a non-rechargeable CR2032 |
| 1 | 24LC256 EEPROM, DIP-8 (or an AT24C256 module) | A0, A1, A2 and WP to GND: address 0x50. The ZS-042 RTC module carries an AT24C32 at 0x57, which the firmware ignores |
| 1 | 4x4 membrane keypad | 8-pin ribbon |
| 1 | SG90 micro servo | Moves the lock's bolt; a real door would use a lock driven by a relay or a MOSFET |
| 1 | Passive buzzer module (or a bare passive buzzer and a 100 Ohm resistor) | **Passive**: the firmware plays tones. An active one only beeps |
| 1 | Green LED, 1 red LED, 2 x 330 Ohm resistors | |
| 1 | Push button | The exit button (inside the door) |
| 1 | Reed switch with a magnet (door contact), or a second push button for the bench | Closed when the door is closed |
| 2 | 4.7 kOhm resistors | I2C pull-ups on SCL and SDA. Skip them if a module already has its own, as most do |
| 1 | Breadboard (830 points) and male-female and male-male jumper wires | |
| 1 | USB Mini-B cable | For the NUCLEO-F446RE (MB1136) |

## Wiring

| Module pin | NUCLEO pin | STM32 pin | Function |
|------------|------------|-----------|----------|
| RC522 SDA | D10 | PB6 | SPI chip select |
| RC522 SCK | D3 | PB3 | SPI1_SCK (AF5) |
| RC522 MOSI | D4 | PB5 | SPI1_MOSI (AF5) |
| RC522 MISO | D5 | PB4 | SPI1_MISO (AF5) |
| RC522 RST | D7 | PA8 | Reset (NRSTPD) |
| RC522 IRQ | not connected | | The firmware polls the reader |
| RC522 3.3V, GND | 3V3, GND | | **3.3 V only** |
| OLED, DS3231, 24LC256 SCL | D15 | PB8 | I2C1_SCL (AF4), 400 kHz |
| OLED, DS3231, 24LC256 SDA | D14 | PB9 | I2C1_SDA (AF4) |
| OLED, DS3231, 24LC256 VCC, GND | 3V3, GND | | |
| Servo signal (orange) | D6 | PB10 | TIM2_CH3 (AF1), 50 Hz |
| Servo + (red), - (brown) | 5V, GND | | |
| Buzzer + | D9 | PC7 | TIM3_CH2 (AF2) |
| Green LED (through 330 Ohm) | D11 | PA7 | |
| Red LED (through 330 Ohm) | D12 | PA6 | |
| Exit button (to GND) | A4 | PC1 | Internal pull-up |
| Door reed contact (to GND) | A5 | PC0 | Internal pull-up: high means open |
| Keypad R1..R4 | A0, A1, A2, A3 | PA0, PA1, PA4, PB0 | Rows, open-drain outputs |
| Keypad C1..C4 | CN7 pins 1, 2, 3, 4 | PC10, PC11, PC12, PD2 | Columns, pulled up |

LD2 (PA5, D13) blinks as the heartbeat: it stops if the supervisor stops,
and the watchdog then resets the board.

## Notes

- **The servo's current.** An SG90 draws a few hundred milliamps when it
  moves. That's fine from the NUCLEO's 5V pin with a USB port that gives
  500 mA. A bigger servo needs its own supply, with its ground joined to
  the NUCLEO's.
- **A4 and A5** are PC1 and PC0 with the NUCLEO's default solder bridges
  (SB56 and SB51 on). They are not the I2C pins here, unlike on an Arduino
  Uno.
- **First boot:** the terminal formats a blank EEPROM and the admin PIN
  is `1234`. `status` and `login` warn until it is changed with `passwd`.
- **The console** is the ST-LINK's virtual COM port at 115200 8N1:
  `screen /dev/tty.usbmodem* 115200` on macOS, PuTTY on Windows.
