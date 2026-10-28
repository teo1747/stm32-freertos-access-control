#include "keypad.h"
#include "board.h"
#include "system.h"

static const pin_t rows[4] = KEYPAD_ROWS;
static const pin_t cols[4] = KEYPAD_COLS;

void keypad_init(void)
{
    /* Open drain: two keys in one column never short a high row to a low
     * one */
    for (int r = 0; r < 4; r++)
        gpio_output(rows[r], GPIO_OPENDRAIN, 1);
    for (int c = 0; c < 4; c++)
        gpio_input(cols[c], GPIO_PULLUP);
}

uint16_t keypad_scan(void)
{
    uint16_t down = 0;

    for (int r = 0; r < 4; r++) {
        gpio_write(rows[r], 0);
        /* The pull-ups (40 kOhm) against the wiring's capacitance */
        delay_us(10);
        for (int c = 0; c < 4; c++)
            if (!gpio_read(cols[c]))
                down |= (uint16_t)(1u << (r * 4 + c));
        gpio_write(rows[r], 1);
    }
    return down;
}

char keypad_char(int key)
{
    static const char map[16] = {
        '1', '2', '3', 'A',
        '4', '5', '6', 'B',
        '7', '8', '9', 'C',
        '*', '0', '#', 'D',
    };
    return key >= 0 && key < 16 ? map[key] : '?';
}
