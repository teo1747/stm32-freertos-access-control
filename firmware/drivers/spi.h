/*
 * SPI1 master, mode 0, polled: the MFRC522's register accesses are a few
 * bytes at 5.6 MHz, shorter than an interrupt's entry and exit.
 */
#ifndef SPI_H
#define SPI_H

#include <stddef.h>
#include <stdint.h>
#include "gpio.h"

void spi_init(void);
/* Takes the bus and pulls cs low; spi_end releases both */
void spi_begin(pin_t cs);
void spi_end(pin_t cs);
uint8_t spi_byte(uint8_t out);
void spi_transfer(const uint8_t *tx, uint8_t *rx, size_t n);

#endif
