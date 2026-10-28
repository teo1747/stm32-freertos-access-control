/*
 * 24LC256: 32 KB of EEPROM on I2C, written in 64-byte pages.
 */
#ifndef EEPROM_H
#define EEPROM_H

#include <stddef.h>
#include <stdint.h>

#define EEPROM_SIZE     32768u
#define EEPROM_PAGE     64u

int eeprom_read(uint16_t addr, void *buf, size_t len);
/* Splits at page boundaries and waits out each write cycle. Returns an i2c
 * error, or 0. */
int eeprom_write(uint16_t addr, const void *buf, size_t len);

#endif
