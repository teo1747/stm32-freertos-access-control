/*
 * The terminal's outputs: the servo lock, the green and red LEDs, and the
 * buzzer's tunes.
 */
#ifndef DOOR_H
#define DOOR_H

#include <stdbool.h>
#include "access.h"

void door_init(void);
void door_lock(bool open);
void door_leds(bool green, bool red);
/* Plays in the background; SOUND_ALARM repeats until another sound */
void door_sound(enum sound s);

#endif
