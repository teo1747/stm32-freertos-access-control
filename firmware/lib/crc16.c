#include "crc16.h"

uint16_t crc16(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint16_t crc = 0xFFFF;

    while (n--) {
        crc ^= (uint16_t)(*p++ << 8);
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}
