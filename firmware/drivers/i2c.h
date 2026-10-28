/*
 * I2C1 master, interrupt-driven: the task that starts a transfer sleeps
 * until the event interrupt has run it to its STOP. One transfer at a time
 * (a mutex inside), so the OLED, the RTC and the EEPROM share the bus from
 * any task.
 */
#ifndef I2C_H
#define I2C_H

#include <stddef.h>
#include <stdint.h>

enum {
    I2C_OK = 0,
    I2C_NACK = -1,          /* no device at the address, or a byte refused */
    I2C_BUS_ERROR = -2,     /* misplaced START/STOP or arbitration lost */
    I2C_TIMEOUT = -3,       /* the transfer never ended: the bus is reset */
};

void i2c_init(void);

/* hdr then data in one write (a register or memory address, then the
 * bytes), so callers don't copy into a single buffer */
int i2c_write(uint8_t addr, const void *hdr, size_t hdr_len, const void *data, size_t len);
/* Writes hdr, then a repeated START and reads len bytes */
int i2c_read(uint8_t addr, const void *hdr, size_t hdr_len, void *data, size_t len);
/* Is a device answering at addr? */
int i2c_probe(uint8_t addr);

struct i2c_stats {
    uint32_t transfers, nacks, bus_errors, timeouts, recoveries;
    /* The last transfer that timed out or hit a bus error: what it was
     * doing and where the peripheral stood */
    uint8_t last_addr;
    uint8_t last_phase;         /* 0 writing, 1 restarting, 2 reading */
    uint16_t last_sr1, last_sr2, last_cr1;
    uint32_t last_left;         /* bytes still to write and read */
};
void i2c_get_stats(struct i2c_stats *s);
const char *i2c_strerror(int err);

#endif
