/*
 * The access terminal's wiring on a NUCLEO-F446RE. Every part sits on the
 * Arduino headers except the keypad's columns, on the top of CN7. Nothing
 * uses a pin the board already drives (LD2 on PA5 stays the heartbeat, B1
 * on PC13, the virtual COM port on PA2/PA3). docs/hardware.md has the
 * wiring table and the parts list.
 */
#ifndef BOARD_H
#define BOARD_H

#include "gpio.h"

/* Clocks: the ST-LINK's 8 MHz MCO drives OSC_IN (HSE in bypass) */
#define BOARD_HSE_HZ        8000000u

/* Console and admin shell: USART2 on the ST-LINK's virtual COM port */
#define PIN_CONSOLE_TX      PIN(A, 2)
#define PIN_CONSOLE_RX      PIN(A, 3)
#define CONSOLE_BAUD        115200u

/* Link to the ESP32-C3 gateway: USART1, D8 (TX) and D2 (RX) */
#define PIN_LINK_TX         PIN(A, 9)
#define PIN_LINK_RX         PIN(A, 10)

/* MFRC522 on SPI1: D3 SCK, D5 MISO, D4 MOSI, D10 chip select, D7 reset */
#define PIN_SPI_SCK         PIN(B, 3)
#define PIN_SPI_MISO        PIN(B, 4)
#define PIN_SPI_MOSI        PIN(B, 5)
#define PIN_RFID_CS         PIN(B, 6)
#define PIN_RFID_RST        PIN(A, 8)

/* I2C1 on D15 (SCL) and D14 (SDA): the OLED, the RTC and the EEPROM */
#define PIN_I2C_SCL         PIN(B, 8)
#define PIN_I2C_SDA         PIN(B, 9)
#define I2C_ADDR_EEPROM     0x50
#define I2C_ADDR_OLED       0x3C
#define I2C_ADDR_RTC        0x68

/* The lock's servo on D6 (TIM2_CH3), the buzzer on D9 (TIM3_CH2) */
#define PIN_SERVO           PIN(B, 10)
#define PIN_BUZZER          PIN(C, 7)

/* Indicators: LD2 on D13, a green and a red LED on D11 and D12 */
#define PIN_LED_HEARTBEAT   PIN(A, 5)
#define PIN_LED_GREEN       PIN(A, 7)
#define PIN_LED_RED         PIN(A, 6)

/* The exit button (inside) on A4, the door's reed contact on A5: both close
 * to ground, so low means pressed or door closed */
#define PIN_EXIT_BUTTON     PIN(C, 1)
#define PIN_DOOR_CONTACT    PIN(C, 0)

/* B1, the blue USER button */
#define PIN_USER_BUTTON     PIN(C, 13)

/* 4x4 keypad: rows on A0..A3 driven open-drain, columns on CN7 1..4 read
 * with pull-ups */
#define KEYPAD_ROWS         { PIN(A, 0), PIN(A, 1), PIN(A, 4), PIN(B, 0) }
#define KEYPAD_COLS         { PIN(C, 10), PIN(C, 11), PIN(C, 12), PIN(D, 2) }

/* Interrupt priorities (0 most urgent). Everything that calls FreeRTOS's
 * FromISR API is at configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5) or
 * below in urgency. */
#define IRQ_PRIO_I2C        5
#define IRQ_PRIO_UART       6
#define IRQ_PRIO_SPI        6

#endif
