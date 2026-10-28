/*
 * USART driver: interrupt-driven, with a FreeRTOS stream buffer each way.
 * Before the scheduler starts (the boot banner), writes poll the register.
 *
 * One task at a time may write (the caller holds a lock), one task reads.
 */
#ifndef UART_H
#define UART_H

#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "stream_buffer.h"
#include "stm32f4xx.h"
#include "gpio.h"

typedef struct {
    USART_TypeDef *regs;
    IRQn_Type irq;
    StreamBufferHandle_t tx;
    StreamBufferHandle_t rx;
    volatile uint32_t rx_dropped;   /* bytes lost to a full buffer or an overrun */
} uart_t;

extern uart_t uart1;    /* the ESP32-C3 link */
extern uart_t uart2;    /* the console */

void uart_init(uart_t *u, pin_t tx, pin_t rx, uint32_t baud, size_t tx_size, size_t rx_size);
void uart_write(uart_t *u, const void *data, size_t len);
/* Waits up to `wait` for the first byte, then returns what is there */
size_t uart_read(uart_t *u, void *buf, size_t len, TickType_t wait);
/* With interrupts off, for the fault path */
void uart_write_polled(uart_t *u, const void *data, size_t len);

#endif
