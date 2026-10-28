/*
 * The terminal's screens, drawn from their state alone: no RTOS, no I/O, so
 * the unit tests render every screen on the host.
 */
#ifndef SCREENS_H
#define SCREENS_H

#include <stdbool.h>
#include <stdint.h>
#include "datetime.h"
#include "gfx.h"

enum screen_kind {
    SCREEN_IDLE,        /* clock, "Present card" */
    SCREEN_PIN,         /* a known card: its PIN being typed */
    SCREEN_GRANTED,
    SCREEN_DENIED,
    SCREEN_LOCKOUT,     /* too many wrong PINs: wait */
    SCREEN_DOOR_OPEN,   /* held open: close it */
    SCREEN_ALARM,       /* forced open, or held open too long */
    SCREEN_ENROLL,      /* the admin asked for the next card */
    SCREEN_MESSAGE,
};

#define SCREEN_PIN_SLOTS 6

struct screen {
    enum screen_kind kind;
    char name[17];      /* the user's */
    char text[22];      /* the reason, or the message's line */
    char title[11];     /* the message's title */
    uint8_t digits;     /* PIN digits typed so far */
    uint16_t seconds;   /* time left: PIN entry, lockout, door */
};

struct screen_env {
    struct datetime now;
    bool time_valid;
    bool have_temperature;
    int temperature;    /* tenths of a degree Celsius */
    bool blink;         /* toggles twice a second, for what flashes */
};

void screen_render(gfx_t *g, const struct screen *s, const struct screen_env *env);

#endif
