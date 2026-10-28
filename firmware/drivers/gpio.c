#include "gpio.h"

/* The ports sit 0x400 apart from GPIOA, in the order of AHB1ENR's bits */
static void clock_on(GPIO_TypeDef *port)
{
    unsigned index = ((uintptr_t)port - GPIOA_BASE) / 0x400u;
    RCC->AHB1ENR |= 1u << index;
    (void)RCC->AHB1ENR;     /* the clock is on two cycles after the write */
}

static void set_field(volatile uint32_t *reg, unsigned pin, unsigned width, uint32_t value)
{
    uint32_t mask = ((1u << width) - 1u) << (pin * width);
    *reg = (*reg & ~mask) | (value << (pin * width));
}

static void configure(pin_t p, unsigned mode, enum gpio_drive drive, enum gpio_pull pull)
{
    clock_on(p.port);
    set_field(&p.port->OTYPER, p.num, 1, drive == GPIO_OPENDRAIN);
    set_field(&p.port->OSPEEDR, p.num, 2, 2);  /* fast: enough for 10 MHz SPI */
    set_field(&p.port->PUPDR, p.num, 2, pull);
    set_field(&p.port->MODER, p.num, 2, mode);
}

void gpio_input(pin_t p, enum gpio_pull pull)
{
    configure(p, 0, GPIO_PUSHPULL, pull);
}

void gpio_output(pin_t p, enum gpio_drive drive, bool level)
{
    /* The level first, so the pin never glitches the other way: ODR needs
     * the port's clock, MODER isn't switched yet */
    clock_on(p.port);
    gpio_write(p, level);
    configure(p, 1, drive, GPIO_NOPULL);
}

void gpio_alternate(pin_t p, unsigned af, enum gpio_drive drive, enum gpio_pull pull)
{
    clock_on(p.port);
    set_field(&p.port->AFR[p.num / 8], p.num % 8, 4, af);
    configure(p, 2, drive, pull);
}
