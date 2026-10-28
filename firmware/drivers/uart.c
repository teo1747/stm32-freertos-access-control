#include "uart.h"
#include "board.h"
#include "system.h"
#include "task.h"

uart_t uart1 = { .regs = USART1, .irq = USART1_IRQn };
uart_t uart2 = { .regs = USART2, .irq = USART2_IRQn };

void uart_init(uart_t *u, pin_t tx, pin_t rx, uint32_t baud, size_t tx_size, size_t rx_size)
{
    uint32_t pclk;

    if (u->regs == USART1) {
        RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
        pclk = system_pclk2();
    } else {
        RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
        pclk = system_pclk1();
    }
    (void)RCC->APB1ENR;

    /* USART1 and 2 are AF7 on every pin they have */
    gpio_alternate(tx, 7, GPIO_PUSHPULL, GPIO_NOPULL);
    gpio_alternate(rx, 7, GPIO_PUSHPULL, GPIO_PULLUP);

    u->tx = xStreamBufferCreate(tx_size, 1);
    u->rx = xStreamBufferCreate(rx_size, 1);
    configASSERT(u->tx && u->rx);

    /* 16x oversampling: BRR is PCLK / baud, rounded (mantissa.fraction) */
    u->regs->CR1 = 0;
    u->regs->BRR = (pclk + baud / 2) / baud;
    u->regs->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;

    NVIC_SetPriority(u->irq, IRQ_PRIO_UART);
    NVIC_EnableIRQ(u->irq);
}

void uart_write_polled(uart_t *u, const void *data, size_t len)
{
    const uint8_t *p = data;
    while (len--) {
        while (!(u->regs->SR & USART_SR_TXE)) { }
        u->regs->DR = *p++;
    }
    while (!(u->regs->SR & USART_SR_TC)) { }
}

void uart_write(uart_t *u, const void *data, size_t len)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
        /* Drain what the interrupt hasn't sent yet, then poll */
        uint8_t c;
        while (xStreamBufferReceive(u->tx, &c, 1, 0))
            uart_write_polled(u, &c, 1);
        uart_write_polled(u, data, len);
        return;
    }

    /* In chunks well under the buffer's size. Invariant: while the buffer
     * holds data TXEIE is on, so a send blocked on a full buffer is always
     * woken by the interrupt draining it. */
    const uint8_t *p = data;
    while (len) {
        size_t chunk = len < 32 ? len : 32;
        size_t sent = xStreamBufferSend(u->tx, p, chunk, portMAX_DELAY);
        taskENTER_CRITICAL();
        u->regs->CR1 |= USART_CR1_TXEIE;
        taskEXIT_CRITICAL();
        p += sent;
        len -= sent;
    }
}

size_t uart_read(uart_t *u, void *buf, size_t len, TickType_t wait)
{
    return xStreamBufferReceive(u->rx, buf, len, wait);
}

static void uart_isr(uart_t *u)
{
    BaseType_t woken = pdFALSE;
    uint32_t sr = u->regs->SR;

    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        /* Reading DR after SR clears RXNE and the overrun flag */
        uint8_t c = (uint8_t)u->regs->DR;
        if (sr & USART_SR_ORE)
            u->rx_dropped++;
        if (!xStreamBufferSendFromISR(u->rx, &c, 1, &woken))
            u->rx_dropped++;
    }

    if ((u->regs->CR1 & USART_CR1_TXEIE) && (sr & USART_SR_TXE)) {
        uint8_t c;
        if (xStreamBufferReceiveFromISR(u->tx, &c, 1, &woken))
            u->regs->DR = c;
        else
            u->regs->CR1 &= ~USART_CR1_TXEIE;
    }

    portYIELD_FROM_ISR(woken);
}

void USART1_IRQHandler(void);
void USART1_IRQHandler(void)
{
    uart_isr(&uart1);
}

void USART2_IRQHandler(void);
void USART2_IRQHandler(void)
{
    uart_isr(&uart2);
}
