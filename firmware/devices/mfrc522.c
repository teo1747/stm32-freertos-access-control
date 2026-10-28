/*
 * MFRC522 after NXP's datasheet (rev. 3.9) and ISO/IEC 14443-3 type A.
 */
#include "mfrc522.h"
#include "board.h"
#include "spi.h"
#include "system.h"
#include "FreeRTOS.h"
#include "task.h"

/* Registers */
#define CommandReg      0x01
#define ComIrqReg       0x04
#define DivIrqReg       0x05
#define ErrorReg        0x06
#define FIFODataReg     0x09
#define FIFOLevelReg    0x0A
#define ControlReg      0x0C
#define BitFramingReg   0x0D
#define CollReg         0x0E
#define ModeReg         0x11
#define TxControlReg    0x14
#define TxASKReg        0x15
#define CRCResultRegH   0x21
#define CRCResultRegL   0x22
#define TModeReg        0x2A
#define TPrescalerReg   0x2B
#define TReloadRegH     0x2C
#define TReloadRegL     0x2D
#define VersionReg      0x37

/* Commands */
#define CMD_IDLE        0x00
#define CMD_CALC_CRC    0x03
#define CMD_TRANSCEIVE  0x0C
#define CMD_SOFT_RESET  0x0F

/* ComIrqReg */
#define IRQ_TIMER       0x01
#define IRQ_ERR         0x02
#define IRQ_IDLE        0x10
#define IRQ_RX          0x20

/* ISO 14443-3 type A */
#define PICC_WUPA       0x52
#define PICC_HLTA       0x50
#define PICC_SEL_CL1    0x93
#define PICC_CASCADE_TAG 0x88

static void write_reg(uint8_t reg, uint8_t v)
{
    spi_begin(PIN_RFID_CS);
    spi_byte((uint8_t)(reg << 1) & 0x7E);
    spi_byte(v);
    spi_end(PIN_RFID_CS);
}

static uint8_t read_reg(uint8_t reg)
{
    spi_begin(PIN_RFID_CS);
    spi_byte((uint8_t)(0x80 | ((reg << 1) & 0x7E)));
    uint8_t v = spi_byte(0);
    spi_end(PIN_RFID_CS);
    return v;
}

static void write_fifo(const uint8_t *data, unsigned n)
{
    spi_begin(PIN_RFID_CS);
    spi_byte((uint8_t)(FIFODataReg << 1) & 0x7E);
    spi_transfer(data, NULL, n);
    spi_end(PIN_RFID_CS);
}

static void read_fifo(uint8_t *data, unsigned n)
{
    /* Each byte read sends the address of the next read */
    uint8_t addr = (uint8_t)(0x80 | ((FIFODataReg << 1) & 0x7E));
    spi_begin(PIN_RFID_CS);
    spi_byte(addr);
    for (unsigned i = 0; i < n; i++)
        data[i] = spi_byte(i + 1 < n ? addr : 0);
    spi_end(PIN_RFID_CS);
}

static void set_bits(uint8_t reg, uint8_t mask)
{
    write_reg(reg, read_reg(reg) | mask);
}

static void clear_bits(uint8_t reg, uint8_t mask)
{
    write_reg(reg, read_reg(reg) & (uint8_t)~mask);
}

/* CRC_A of data, by the chip's coprocessor */
static int crc_a(const uint8_t *data, unsigned n, uint8_t out[2])
{
    write_reg(CommandReg, CMD_IDLE);
    write_reg(DivIrqReg, 0x04);             /* clear CRCIRq */
    write_reg(FIFOLevelReg, 0x80);          /* flush */
    write_fifo(data, n);
    write_reg(CommandReg, CMD_CALC_CRC);
    for (int i = 0; i < 100; i++) {
        if (read_reg(DivIrqReg) & 0x04) {
            write_reg(CommandReg, CMD_IDLE);
            out[0] = read_reg(CRCResultRegL);
            out[1] = read_reg(CRCResultRegH);
            return MFRC522_OK;
        }
        delay_us(20);
    }
    return MFRC522_ERROR;
}

/*
 * Sends n bytes (the last one tx_bits long, 0 meaning 8) and receives the
 * answer. Returns its length in bytes, or an error; *rx_bits is the number
 * of valid bits in the last byte (0 meaning 8).
 */
static int transceive(const uint8_t *tx, unsigned n, uint8_t tx_bits,
                      uint8_t *rx, unsigned rx_max, uint8_t *rx_bits)
{
    write_reg(CommandReg, CMD_IDLE);
    write_reg(ComIrqReg, 0x7F);             /* clear every interrupt bit */
    write_reg(FIFOLevelReg, 0x80);
    write_fifo(tx, n);
    write_reg(BitFramingReg, tx_bits & 0x07);
    write_reg(CommandReg, CMD_TRANSCEIVE);
    set_bits(BitFramingReg, 0x80);          /* StartSend */

    /* The chip's timer (TAuto) ends a silent exchange after 10 ms */
    uint8_t irq = 0;
    for (int i = 0; i < 20; i++) {
        irq = read_reg(ComIrqReg);
        if (irq & (IRQ_RX | IRQ_IDLE | IRQ_TIMER))
            break;
        vTaskDelay(1);
    }
    clear_bits(BitFramingReg, 0x80);

    if (!(irq & (IRQ_RX | IRQ_IDLE)))
        return MFRC522_NO_CARD;
    /* BufferOvfl, CollErr, ParityErr, ProtocolErr */
    if (read_reg(ErrorReg) & 0x1B)
        return MFRC522_ERROR;

    unsigned len = read_reg(FIFOLevelReg);
    if (len > rx_max)
        return MFRC522_ERROR;
    read_fifo(rx, len);
    if (rx_bits)
        *rx_bits = read_reg(ControlReg) & 0x07;
    return (int)len;
}

int mfrc522_init(void)
{
    gpio_output(PIN_RFID_CS, GPIO_PUSHPULL, 1);
    /* NRSTPD: low powers down, the rising edge is a hard reset */
    gpio_output(PIN_RFID_RST, GPIO_PUSHPULL, 0);
    delay_us(100);
    gpio_write(PIN_RFID_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));          /* the oscillator's start-up */

    write_reg(CommandReg, CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t version = read_reg(VersionReg);
    if (version == 0x00 || version == 0xFF)
        return MFRC522_NOT_FOUND;

    /* Timer: 13.56 MHz / (2 x 0xA9 + 1) = 40 kHz, 400 ticks = 10 ms,
     * started at the end of each transmission (TAuto) */
    write_reg(TModeReg, 0x80);
    write_reg(TPrescalerReg, 0xA9);
    write_reg(TReloadRegH, 400 >> 8);
    write_reg(TReloadRegL, 400 & 0xFF);
    write_reg(TxASKReg, 0x40);              /* 100% ASK modulation */
    write_reg(ModeReg, 0x3D);               /* CRC preset 6363h, as type A wants */
    set_bits(TxControlReg, 0x03);           /* antenna drivers on */
    return version;
}

int mfrc522_read_uid(struct card_uid *uid)
{
    uint8_t buf[8];
    uint8_t bits;

    /* WUPA, 7 bits: wakes halted cards too, so a card left on the reader
     * keeps answering and its removal can be seen */
    write_reg(CollReg, 0x80);               /* bits received after a collision are cleared */
    uint8_t wupa = PICC_WUPA;
    int n = transceive(&wupa, 1, 7, buf, sizeof buf, &bits);
    if (n < 0)
        return n;
    if (n != 2)
        return MFRC522_ERROR;               /* ATQA is 2 bytes */

    /* Anticollision, cascade level 1: the 4 UID bytes and their BCC */
    uint8_t anticoll[2] = { PICC_SEL_CL1, 0x20 };
    n = transceive(anticoll, 2, 0, buf, sizeof buf, &bits);
    if (n != 5)
        return n < 0 ? n : MFRC522_ERROR;
    if ((buf[0] ^ buf[1] ^ buf[2] ^ buf[3]) != buf[4])
        return MFRC522_ERROR;
    if (buf[0] == PICC_CASCADE_TAG)
        return MFRC522_UNSUPPORTED;

    /* SELECT: the card answers its SAK */
    uint8_t sel[9] = { PICC_SEL_CL1, 0x70, buf[0], buf[1], buf[2], buf[3], buf[4] };
    if (crc_a(sel, 7, &sel[7]))
        return MFRC522_ERROR;
    uint8_t sak[3];
    n = transceive(sel, 9, 0, sak, sizeof sak, &bits);
    if (n != 3)
        return n < 0 ? n : MFRC522_ERROR;

    uid->len = 4;
    for (int i = 0; i < 4; i++)
        uid->bytes[i] = buf[i];

    /* HLTA: a halted card answers nothing, so silence is success */
    uint8_t hlta[4] = { PICC_HLTA, 0x00 };
    if (crc_a(hlta, 2, &hlta[2]) == MFRC522_OK)
        transceive(hlta, 4, 0, buf, sizeof buf, NULL);
    return MFRC522_OK;
}
