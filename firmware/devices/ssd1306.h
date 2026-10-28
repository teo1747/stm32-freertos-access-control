/*
 * SSD1306 128x64 OLED on I2C: the panel shows a gfx_t framebuffer.
 */
#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include "gfx.h"

int ssd1306_init(void);
/* The whole frame, 1 KB in one transfer (23 ms at 400 kHz) */
int ssd1306_show(const gfx_t *g);
int ssd1306_contrast(uint8_t level);
int ssd1306_power(int on);

#endif
