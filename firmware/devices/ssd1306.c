#include "ssd1306.h"
#include "board.h"
#include "i2c.h"

/* Control byte: Co = 0 (a stream follows), D/C# = 0 commands, 1 data */
#define CTRL_COMMANDS   0x00
#define CTRL_DATA       0x40

static int commands(const uint8_t *cmd, size_t n)
{
    uint8_t ctrl = CTRL_COMMANDS;
    return i2c_write(I2C_ADDR_OLED, &ctrl, 1, cmd, n);
}

int ssd1306_init(void)
{
    /* The datasheet's application note sequence, for a 128x64 module with
     * the internal charge pump */
    static const uint8_t init[] = {
        0xAE,               /* display off */
        0xD5, 0x80,         /* clock divide ratio and oscillator */
        0xA8, 0x3F,         /* multiplex 1/64 */
        0xD3, 0x00,         /* no display offset */
        0x40,               /* start line 0 */
        0x8D, 0x14,         /* charge pump on */
        0x20, 0x00,         /* horizontal addressing: one write fills the screen */
        0xA1,               /* segment remap: column 127 is SEG0 */
        0xC8,               /* COM scan from COM63: with A1, the image upright */
        0xDA, 0x12,         /* alternative COM pins, as 128x64 modules are wired */
        0x81, 0xCF,         /* contrast */
        0xD9, 0xF1,         /* pre-charge, for the charge pump */
        0xDB, 0x40,         /* VCOMH deselect level */
        0xA4,               /* show the RAM */
        0xA6,               /* normal, not inverted */
        0x2E,               /* no scrolling */
        0xAF,               /* display on */
    };
    return commands(init, sizeof init);
}

int ssd1306_show(const gfx_t *g)
{
    static const uint8_t window[] = {
        0x21, 0, GFX_WIDTH - 1,         /* columns */
        0x22, 0, GFX_HEIGHT / 8 - 1,     /* pages */
    };
    int err = commands(window, sizeof window);
    if (err)
        return err;
    uint8_t ctrl = CTRL_DATA;
    return i2c_write(I2C_ADDR_OLED, &ctrl, 1, g->buf, sizeof g->buf);
}

int ssd1306_contrast(uint8_t level)
{
    uint8_t cmd[2] = { 0x81, level };
    return commands(cmd, sizeof cmd);
}

int ssd1306_power(int on)
{
    uint8_t cmd = on ? 0xAF : 0xAE;
    return commands(&cmd, 1);
}
