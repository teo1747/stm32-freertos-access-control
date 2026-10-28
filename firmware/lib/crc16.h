/*
 * CRC-16/CCITT-FALSE (polynomial 0x1021, initial 0xFFFF): the check on
 * every record in the EEPROM.
 */
#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t crc16(const void *data, size_t n);

#endif
