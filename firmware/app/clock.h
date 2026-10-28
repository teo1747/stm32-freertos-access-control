/*
 * Wall-clock time: read from the DS3231 at boot and once a minute, and in
 * between counted from the FreeRTOS tick, so reading the time costs no
 * I2C transfer.
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <stdint.h>
#include "datetime.h"

/* From a task: the I2C bus must be up */
void clock_init(void);
/* Unix time; 0 if the RTC isn't answering */
uint32_t clock_now(void);
void clock_now_dt(struct datetime *dt);
/* False after a power loss without the battery, until the time is set */
bool clock_valid(void);
bool clock_present(void);
int clock_set(const struct datetime *dt);
/* Re-reads the RTC if it's been a minute */
void clock_poll(void);
/* Tenths of a degree Celsius, from the DS3231's sensor */
bool clock_temperature(int *tenths_c);

#endif
