#include "eeprom.h"
#include "board.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"

/* The datasheet's write cycle is 5 ms at most: give up well after */
#define WRITE_CYCLE_MS  10

int eeprom_read(uint16_t addr, void *buf, size_t len)
{
    uint8_t a[2] = { (uint8_t)(addr >> 8), (uint8_t)addr };
    return i2c_read(I2C_ADDR_EEPROM, a, 2, buf, len);
}

/* During its write cycle the chip doesn't acknowledge its address: poll
 * until it does (datasheet 7.0), rather than always waiting 5 ms */
static int wait_ready(void)
{
    TickType_t start = xTaskGetTickCount();
    do {
        if (i2c_probe(I2C_ADDR_EEPROM))
            return 0;
        vTaskDelay(1);
    } while (xTaskGetTickCount() - start < pdMS_TO_TICKS(WRITE_CYCLE_MS));
    return I2C_TIMEOUT;
}

int eeprom_write(uint16_t addr, const void *buf, size_t len)
{
    const uint8_t *p = buf;

    if ((size_t)addr + len > EEPROM_SIZE)
        return I2C_NACK;
    while (len) {
        /* A page write wraps inside its page: stop at the boundary */
        size_t room = EEPROM_PAGE - (addr % EEPROM_PAGE);
        size_t n = len < room ? len : room;
        uint8_t a[2] = { (uint8_t)(addr >> 8), (uint8_t)addr };

        int err = i2c_write(I2C_ADDR_EEPROM, a, 2, p, n);
        if (err)
            return err;
        err = wait_ready();
        if (err)
            return err;
        addr += (uint16_t)n;
        p += n;
        len -= n;
    }
    return 0;
}
