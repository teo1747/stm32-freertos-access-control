/*
 * GPIO on the registers: a pin is its port and its number, passed by value.
 */
#ifndef GPIO_H
#define GPIO_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32f4xx.h"

typedef struct {
    GPIO_TypeDef *port;
    uint8_t num;
} pin_t;

#define PIN(p, n) ((pin_t){ GPIO##p, (n) })

enum gpio_pull { GPIO_NOPULL, GPIO_PULLUP, GPIO_PULLDOWN };
enum gpio_drive { GPIO_PUSHPULL, GPIO_OPENDRAIN };

void gpio_input(pin_t p, enum gpio_pull pull);
void gpio_output(pin_t p, enum gpio_drive drive, bool level);
void gpio_alternate(pin_t p, unsigned af, enum gpio_drive drive, enum gpio_pull pull);

static inline void gpio_write(pin_t p, bool level)
{
    /* BSRR's set half or reset half: atomic, no read-modify-write */
    p.port->BSRR = level ? (1u << p.num) : (1u << (p.num + 16));
}

static inline bool gpio_read(pin_t p)
{
    return (p.port->IDR >> p.num) & 1u;
}

static inline void gpio_toggle(pin_t p)
{
    gpio_write(p, !((p.port->ODR >> p.num) & 1u));
}

#endif
