/*
 * 4x4 membrane keypad: one row driven low at a time, the columns read with
 * pull-ups. Debouncing is the caller's: scan every few milliseconds and
 * keep a key once it reads the same several times.
 */
#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

void keypad_init(void);
/* Bit (row * 4 + col) set for each key down */
uint16_t keypad_scan(void);
/* '1'..'9', '0', 'A'..'D', '*', '#' for a key's bit number */
char keypad_char(int key);

#endif
