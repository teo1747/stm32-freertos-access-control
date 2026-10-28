/*
 * The OLED's task: shows the current screen, returns to the clock when a
 * timed one (granted, denied) expires.
 */
#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "screens.h"

void ui_start(void);
/* Show s; after ms (0: never) go back to the idle clock */
void ui_show(const struct screen *s, uint32_t ms);
void ui_idle(void);
/* What the panel shows, for the shell's "screen" command */
enum screen_kind ui_current(void);
uint32_t ui_frames_sent(void);

#endif
