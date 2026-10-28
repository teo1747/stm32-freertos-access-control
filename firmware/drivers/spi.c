#include "spi.h"
#include "board.h"
#include "FreeRTOS.h"
#include "semphr.h"

static SemaphoreHandle_t bus;

void spi_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    gpio_alternate(PIN_SPI_SCK, 5, GPIO_PUSHPULL, GPIO_NOPULL);
    gpio_alternate(PIN_SPI_MISO, 5, GPIO_PUSHPULL, GPIO_PULLUP);
    gpio_alternate(PIN_SPI_MOSI, 5, GPIO_PUSHPULL, GPIO_NOPULL);

    bus = xSemaphoreCreateMutex();
    configASSERT(bus);

    /* Master, mode 0, MSB first, 8 bits, software NSS; PCLK2 (90 MHz) / 16
     * = 5.6 MHz, under the MFRC522's 10 MHz */
    SPI1->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (3u << SPI_CR1_BR_Pos);
    SPI1->CR1 |= SPI_CR1_SPE;
}

void spi_begin(pin_t cs)
{
    xSemaphoreTake(bus, portMAX_DELAY);
    gpio_write(cs, 0);
}

void spi_end(pin_t cs)
{
    /* The last byte must be out before the chip select goes up */
    while (SPI1->SR & SPI_SR_BSY) { }
    gpio_write(cs, 1);
    xSemaphoreGive(bus);
}

uint8_t spi_byte(uint8_t out)
{
    while (!(SPI1->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&SPI1->DR = out;
    while (!(SPI1->SR & SPI_SR_RXNE)) { }
    return *(volatile uint8_t *)&SPI1->DR;
}

void spi_transfer(const uint8_t *tx, uint8_t *rx, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        uint8_t in = spi_byte(tx ? tx[i] : 0xFF);
        if (rx)
            rx[i] = in;
    }
}
