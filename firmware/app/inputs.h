/*
 * The input tasks: the card reader, and the keypad, the exit button and the
 * door's contact. Each turns raw readings into debounced events for the
 * terminal.
 */
#ifndef INPUTS_H
#define INPUTS_H

#include <stdbool.h>

/* The pins; returns whether the door is open now */
bool inputs_init(void);
/* The tasks: start the terminal first, they post to it */
void inputs_start(void);
/* "MFRC522 v2.0", or why there is none */
const char *inputs_reader_status(void);

#endif
