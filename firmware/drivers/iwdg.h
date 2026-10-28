/*
 * The independent watchdog: once started, only a reset stops it. It runs
 * from the LSI (32 kHz), so it still bites if the main clock dies.
 */
#ifndef IWDG_H
#define IWDG_H

#include <stdint.h>

void iwdg_start(uint32_t timeout_ms);
void iwdg_feed(void);

#endif
