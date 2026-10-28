#include "ds3231.h"
#include "board.h"
#include "i2c.h"

#define REG_SECONDS     0x00
#define REG_CONTROL     0x0E
#define REG_STATUS      0x0F
#define REG_TEMP_MSB    0x11

#define CONTROL_INTCN   0x04    /* INT/SQW is the alarm interrupt, no square wave */
#define STATUS_OSF      0x80
#define STATUS_EN32KHZ  0x08

static uint8_t bcd2bin(uint8_t v)
{
    return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

static uint8_t bin2bcd(unsigned v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

static int read_reg(uint8_t reg, uint8_t *v)
{
    return i2c_read(I2C_ADDR_RTC, &reg, 1, v, 1);
}

static int write_reg(uint8_t reg, uint8_t v)
{
    return i2c_write(I2C_ADDR_RTC, &reg, 1, &v, 1);
}

int ds3231_init(void)
{
    uint8_t status;
    int err = read_reg(REG_STATUS, &status);
    if (err)
        return err;
    /* The 32 kHz output is on at power-up: nothing here uses it */
    if (status & STATUS_EN32KHZ) {
        err = write_reg(REG_STATUS, status & (uint8_t)~STATUS_EN32KHZ);
        if (err)
            return err;
    }
    return write_reg(REG_CONTROL, CONTROL_INTCN);
}

int ds3231_get(struct datetime *dt)
{
    uint8_t reg = REG_SECONDS, r[7];
    /* One burst: the DS3231 latches the time at the START, so the fields
     * can't tear across a second's rollover */
    int err = i2c_read(I2C_ADDR_RTC, &reg, 1, r, sizeof r);
    if (err)
        return err;

    dt->second = bcd2bin(r[0] & 0x7F);
    dt->minute = bcd2bin(r[1] & 0x7F);
    if (r[2] & 0x40) {
        /* 12-hour mode, set by someone else: convert */
        unsigned h = bcd2bin(r[2] & 0x1F) % 12;
        dt->hour = (uint8_t)(h + ((r[2] & 0x20) ? 12 : 0));
    } else {
        dt->hour = bcd2bin(r[2] & 0x3F);
    }
    dt->weekday = r[3] & 0x07;
    dt->day = bcd2bin(r[4] & 0x3F);
    dt->month = bcd2bin(r[5] & 0x1F);
    /* The century bit marks 2100, out of this clock's range here */
    dt->year = (uint16_t)(2000 + bcd2bin(r[6]));
    return 0;
}

int ds3231_set(const struct datetime *dt)
{
    uint8_t reg = REG_SECONDS;
    uint8_t r[7] = {
        bin2bcd(dt->second), bin2bcd(dt->minute), bin2bcd(dt->hour),
        (uint8_t)(dt->weekday ? dt->weekday : 1),
        bin2bcd(dt->day), bin2bcd(dt->month), bin2bcd(dt->year % 100u),
    };
    int err = i2c_write(I2C_ADDR_RTC, &reg, 1, r, sizeof r);
    if (err)
        return err;

    uint8_t status;
    err = read_reg(REG_STATUS, &status);
    if (err)
        return err;
    return write_reg(REG_STATUS, status & (uint8_t)~(STATUS_OSF | STATUS_EN32KHZ));
}

int ds3231_lost_power(bool *lost)
{
    uint8_t status;
    int err = read_reg(REG_STATUS, &status);
    if (!err)
        *lost = (status & STATUS_OSF) != 0;
    return err;
}

int ds3231_temperature(int *quarter_c)
{
    uint8_t reg = REG_TEMP_MSB, t[2];
    int err = i2c_read(I2C_ADDR_RTC, &reg, 1, t, 2);
    if (!err)
        *quarter_c = (int8_t)t[0] * 4 + (t[1] >> 6);
    return err;
}
