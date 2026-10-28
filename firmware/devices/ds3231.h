/*
 * DS3231 real-time clock on I2C: the time in 24-hour mode, the oscillator
 * stop flag (the time is not to be trusted after a power loss without the
 * battery) and the die temperature.
 */
#ifndef DS3231_H
#define DS3231_H

#include <stdbool.h>
#include "datetime.h"

/* Turns the 32 kHz output off and the square wave into the alarm
 * interrupt. Returns an i2c error, or 0. */
int ds3231_init(void);
int ds3231_get(struct datetime *dt);
/* Also clears the oscillator stop flag: the time is valid from now on */
int ds3231_set(const struct datetime *dt);
/* The oscillator stopped since the time was last set (OSF) */
int ds3231_lost_power(bool *lost);
/* Quarter degrees Celsius */
int ds3231_temperature(int *quarter_c);

#endif
