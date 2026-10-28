#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

enum reset_cause {
    RESET_POWER_ON,
    RESET_PIN,
    RESET_SOFTWARE,
    RESET_WATCHDOG,
    RESET_WINDOW_WATCHDOG,
    RESET_LOW_POWER,
    RESET_BROWNOUT,
};

/* 180 MHz from the PLL, the flash, the caches and the bus prescalers.
 * Returns the PLL's source: "HSE" or, if the ST-LINK's clock never came,
 * "HSI". */
const char *system_clock_init(void);

uint32_t system_hclk(void);
uint32_t system_pclk1(void);
uint32_t system_pclk2(void);
/* What clocks TIM2..7 and 12..14 (APB1's timers) */
uint32_t system_timclk1(void);

/* Read once at boot: RCC_CSR's flags are cleared afterwards */
enum reset_cause system_reset_cause(void);
const char *system_reset_name(enum reset_cause cause);

/* Busy wait on the DWT's cycle counter, for the short delays parts need */
void delay_us(uint32_t us);
uint32_t system_cycles(void);

void system_reboot(void) __attribute__((noreturn));

/* The chip's 96-bit unique ID, folded to 32 bits */
uint32_t system_uid32(void);

#endif
