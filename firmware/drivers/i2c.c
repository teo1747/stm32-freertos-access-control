/*
 * The STM32F4's I2C (v1) in master mode, from its event and error
 * interrupts, after RM0390 section 24.3.3.
 *
 * Receiving is the delicate part: the NACK and the STOP must be set before
 * the last byte is clocked in, so the ending depends on the length:
 *   N = 1: NACK and STOP right after ADDR, then RXNE.
 *   N = 2: POS and NACK before ADDR is cleared; at BTF both bytes are in,
 *          STOP, then read both.
 *   N > 2: RXNE reads until 3 bytes remain; at BTF (N-2 in DR, N-1 in the
 *          shift register) NACK and read N-2; at the next BTF STOP and read
 *          the last two.
 */
#include "i2c.h"
#include "board.h"
#include "gpio.h"
#include "system.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#define BUS_HZ      400000u

/* RESTARTING: the write part is done and a repeated START is requested.
 * BTF stays set until the START goes out, re-entering the interrupt: until
 * SB, nothing is to be done. */
enum phase { WRITING, RESTARTING, READING };

static struct {
    uint8_t addr;
    enum phase phase;
    const uint8_t *seg[2];
    size_t seg_len[2];
    int seg_i;
    size_t seg_pos;
    uint8_t *rx;
    size_t rx_len;
    size_t rx_pos;
    volatile int result;
    TaskHandle_t waiter;
} x;

static SemaphoreHandle_t bus;
static struct i2c_stats stats;

static void configure(void)
{
    uint32_t pclk = system_pclk1();
    uint32_t mhz = pclk / 1000000u;

    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;
    I2C1->CR2 = mhz;
    /* Fast mode, duty 2:1: the period is 3 x CCR clocks of PCLK1 */
    I2C1->CCR = I2C_CCR_FS | ((pclk + 3u * BUS_HZ - 1u) / (3u * BUS_HZ));
    /* 300 ns maximum rise time in fast mode, plus one */
    I2C1->TRISE = (mhz * 300u) / 1000u + 1u;
    I2C1->CR1 = I2C_CR1_PE;
}

/* A slave reset mid-read can hold SDA low forever. Clocking SCL until it
 * lets go, then a STOP, frees it (I2C spec, 3.1.16). */
static void bus_recover(void)
{
    pin_t scl = PIN_I2C_SCL, sda = PIN_I2C_SDA;

    gpio_output(scl, GPIO_OPENDRAIN, 1);
    gpio_output(sda, GPIO_OPENDRAIN, 1);
    gpio_input(sda, GPIO_PULLUP);
    delay_us(5);
    if (!gpio_read(sda)) {
        stats.recoveries++;
        for (int i = 0; i < 9 && !gpio_read(sda); i++) {
            gpio_write(scl, 0);
            delay_us(5);
            gpio_write(scl, 1);
            delay_us(5);
        }
    }
    /* STOP: SDA rises while SCL is high */
    gpio_output(sda, GPIO_OPENDRAIN, 0);
    delay_us(5);
    gpio_write(sda, 1);
    delay_us(5);

    gpio_alternate(scl, 4, GPIO_OPENDRAIN, GPIO_NOPULL);
    gpio_alternate(sda, 4, GPIO_OPENDRAIN, GPIO_NOPULL);
}

void i2c_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;

    bus = xSemaphoreCreateMutex();
    configASSERT(bus);

    bus_recover();
    configure();

    NVIC_SetPriority(I2C1_EV_IRQn, IRQ_PRIO_I2C);
    NVIC_SetPriority(I2C1_ER_IRQn, IRQ_PRIO_I2C);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
}

static void finish_from_isr(int result)
{
    BaseType_t woken = pdFALSE;

    I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
    I2C1->CR1 &= ~I2C_CR1_POS;
    x.result = result;
    vTaskNotifyGiveIndexedFromISR(x.waiter, NOTIFY_DRIVER, &woken);
    portYIELD_FROM_ISR(woken);
}

static size_t tx_left(void)
{
    size_t left = x.seg_len[x.seg_i] - x.seg_pos;
    if (x.seg_i == 0)
        left += x.seg_len[1];
    return left;
}

static uint8_t tx_next(void)
{
    while (x.seg_pos == x.seg_len[x.seg_i]) {
        x.seg_i++;
        x.seg_pos = 0;
    }
    return x.seg[x.seg_i][x.seg_pos++];
}

/* ADDR clears on a read of SR1 then SR2. SR1 is read again right before:
 * the CR1 writes that must come first (the NACK, POS) may come between,
 * and some implementations take a CR1 write as breaking the sequence. */
static void clear_addr(void)
{
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

/* After the address is acknowledged in read mode, set up the ending */
static void read_started(void)
{
    if (x.rx_len == 1) {
        I2C1->CR1 &= ~I2C_CR1_ACK;
        clear_addr();
        I2C1->CR1 |= I2C_CR1_STOP;
        I2C1->CR2 |= I2C_CR2_ITBUFEN;
    } else if (x.rx_len == 2) {
        I2C1->CR1 = (I2C1->CR1 & ~I2C_CR1_ACK) | I2C_CR1_POS;
        clear_addr();
        I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
    } else {
        I2C1->CR1 |= I2C_CR1_ACK;
        clear_addr();
        if (x.rx_len == 3)
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
        else
            I2C1->CR2 |= I2C_CR2_ITBUFEN;
    }
}

void I2C1_EV_IRQHandler(void);
void I2C1_EV_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;

    if (sr1 & I2C_SR1_SB) {
        if (x.phase == RESTARTING)
            x.phase = READING;
        I2C1->DR = (uint8_t)(x.addr << 1) | (x.phase == READING);
        return;
    }
    if (x.phase == RESTARTING)
        return;

    if (sr1 & I2C_SR1_ADDR) {
        if (x.phase == READING) {
            read_started();
        } else {
            clear_addr();
            if (tx_left()) {
                I2C1->CR2 |= I2C_CR2_ITBUFEN;
            } else if (x.rx_len) {
                x.phase = RESTARTING;
                I2C1->CR1 |= I2C_CR1_START;
            } else {
                I2C1->CR1 |= I2C_CR1_STOP;      /* a probe */
                finish_from_isr(I2C_OK);
            }
        }
        return;
    }

    if (x.phase == WRITING) {
        if ((sr1 & I2C_SR1_TXE) && (I2C1->CR2 & I2C_CR2_ITBUFEN) && tx_left()) {
            I2C1->DR = tx_next();
            if (!tx_left())
                I2C1->CR2 &= ~I2C_CR2_ITBUFEN;  /* wait for BTF */
        } else if (sr1 & I2C_SR1_BTF) {
            if (x.rx_len) {
                x.phase = RESTARTING;
                I2C1->CR1 |= I2C_CR1_START;
            } else {
                I2C1->CR1 |= I2C_CR1_STOP;
                finish_from_isr(I2C_OK);
            }
        }
        return;
    }

    /* Reading */
    if ((sr1 & I2C_SR1_RXNE) && (I2C1->CR2 & I2C_CR2_ITBUFEN)) {
        x.rx[x.rx_pos++] = (uint8_t)I2C1->DR;
        size_t left = x.rx_len - x.rx_pos;
        if (left == 0)
            finish_from_isr(I2C_OK);            /* N = 1 */
        else if (left == 3)
            I2C1->CR2 &= ~I2C_CR2_ITBUFEN;
    } else if (sr1 & I2C_SR1_BTF) {
        size_t left = x.rx_len - x.rx_pos;
        if (left == 3) {
            I2C1->CR1 &= ~I2C_CR1_ACK;
            x.rx[x.rx_pos++] = (uint8_t)I2C1->DR;
        } else if (left == 2) {
            I2C1->CR1 |= I2C_CR1_STOP;
            x.rx[x.rx_pos++] = (uint8_t)I2C1->DR;
            x.rx[x.rx_pos++] = (uint8_t)I2C1->DR;
            finish_from_isr(I2C_OK);
        }
    }
}

void I2C1_ER_IRQHandler(void);
void I2C1_ER_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;

    I2C1->SR1 = ~(sr1 & (I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR)) & 0xFFFFu;
    if (sr1 & I2C_SR1_AF) {
        I2C1->CR1 |= I2C_CR1_STOP;
        finish_from_isr(I2C_NACK);
    } else if (sr1 & (I2C_SR1_BERR | I2C_SR1_ARLO)) {
        finish_from_isr(I2C_BUS_ERROR);
    }
}

static int wait_idle(void)
{
    /* The previous STOP may still be going out */
    for (int i = 0; i < 1000; i++) {
        if (!(I2C1->SR2 & I2C_SR2_BUSY) && !(I2C1->CR1 & I2C_CR1_STOP))
            return 1;
        delay_us(1);
    }
    return 0;
}

static int transfer(uint8_t addr, const void *hdr, size_t hdr_len, const void *data,
                    size_t len, void *rx, size_t rx_len)
{
    xSemaphoreTake(bus, portMAX_DELAY);

    if (!wait_idle()) {
        bus_recover();
        configure();
    }

    x.addr = addr;
    x.seg[0] = hdr;
    x.seg_len[0] = hdr_len;
    x.seg[1] = data;
    x.seg_len[1] = len;
    x.seg_i = 0;
    x.seg_pos = 0;
    x.rx = rx;
    x.rx_len = rx_len;
    x.rx_pos = 0;
    x.phase = (hdr_len + len == 0 && rx_len) ? READING : WRITING;
    x.result = I2C_TIMEOUT;
    x.waiter = xTaskGetCurrentTaskHandle();
    /* A late give from a transfer that timed out must not end this one */
    ulTaskNotifyValueClearIndexed(NULL, NOTIFY_DRIVER, ~0u);

    I2C1->CR1 |= I2C_CR1_ACK;
    I2C1->CR2 |= I2C_CR2_ITEVTEN | I2C_CR2_ITERREN;
    I2C1->CR1 |= I2C_CR1_START;

    /* 400 kHz: 23 us a byte; allow ten times that, and 20 ms at least */
    uint32_t ms = 20u + (uint32_t)(hdr_len + len + rx_len) / 4u;
    int result;
    if (ulTaskNotifyTakeIndexed(NOTIFY_DRIVER, pdTRUE, pdMS_TO_TICKS(ms))) {
        result = x.result;
    } else {
        I2C1->CR2 &= ~(I2C_CR2_ITEVTEN | I2C_CR2_ITBUFEN | I2C_CR2_ITERREN);
        result = I2C_TIMEOUT;
    }

    stats.transfers++;
    if (result == I2C_NACK)
        stats.nacks++;
    if (result == I2C_BUS_ERROR || result == I2C_TIMEOUT) {
        stats.last_addr = addr;
        stats.last_phase = (uint8_t)x.phase;
        stats.last_sr1 = (uint16_t)I2C1->SR1;
        stats.last_sr2 = (uint16_t)I2C1->SR2;
        stats.last_cr1 = (uint16_t)I2C1->CR1;
        stats.last_left = (uint32_t)(tx_left() + x.rx_len - x.rx_pos);
        if (result == I2C_BUS_ERROR)
            stats.bus_errors++;
        else
            stats.timeouts++;
        bus_recover();
        configure();
    }

    xSemaphoreGive(bus);
    return result;
}

int i2c_write(uint8_t addr, const void *hdr, size_t hdr_len, const void *data, size_t len)
{
    return transfer(addr, hdr, hdr_len, data, len, NULL, 0);
}

int i2c_read(uint8_t addr, const void *hdr, size_t hdr_len, void *data, size_t len)
{
    return transfer(addr, hdr, hdr_len, NULL, 0, data, len);
}

int i2c_probe(uint8_t addr)
{
    return transfer(addr, NULL, 0, NULL, 0, NULL, 0) == I2C_OK;
}

void i2c_get_stats(struct i2c_stats *s)
{
    *s = stats;
}

const char *i2c_strerror(int err)
{
    switch (err) {
    case I2C_OK: return "ok";
    case I2C_NACK: return "no acknowledge";
    case I2C_BUS_ERROR: return "bus error";
    case I2C_TIMEOUT: return "timeout";
    default: return "?";
    }
}
