/*
 * The access rules, as a state machine fed with events: a card, a key, the
 * door's contact, the exit button, the passing of time, the admin's
 * requests. It reaches the hardware, the storage and the screen only
 * through the callbacks in access_io, so the unit tests drive it with
 * fakes and a fake clock.
 *
 *   idle --card--> (PIN) --> unlocked --door opens--> open --door closes--> idle
 *
 * - An unknown, disabled or out-of-hours card is denied.
 * - max_attempts wrong PINs in a row lock the keypad out for lockout_s,
 *   doubling with each lockout until a correct PIN.
 * - The door opening while locked is a forced-door alarm: only an admin
 *   (card, with PIN if the admin has one, or the shell) clears it.
 * - The door held open longer than door_held_s raises an alarm that clears
 *   when it closes.
 * - The exit button always opens: nobody is ever locked in.
 */
#ifndef ACCESS_H
#define ACCESS_H

#include <stdbool.h>
#include <stdint.h>
#include "accesslog.h"
#include "card.h"
#include "screens.h"
#include "userdb.h"

enum sound { SOUND_OFF, SOUND_KEY, SOUND_OK, SOUND_DENIED, SOUND_WARN, SOUND_ALARM };

struct access_io {
    void *ctx;
    /* The user with this card: its slot, or -1 */
    int (*find)(void *ctx, const struct card_uid *uid, struct user *u);
    bool (*check_pin)(void *ctx, int slot, const char *pin);
    void (*log)(void *ctx, enum log_event e, int slot, const struct card_uid *uid);
    void (*lock)(void *ctx, bool open);
    void (*leds)(void *ctx, bool green, bool red);
    void (*sound)(void *ctx, enum sound s);
    /* Show a screen; after ms (0: never) the UI goes back to the clock */
    void (*show)(void *ctx, const struct screen *s, uint32_t ms);
    /* The card presented in enrol mode, or NULL when it timed out */
    void (*enrolled)(void *ctx, const struct card_uid *uid);
    /* The local hour, 0..23, or -1 if the clock can't be trusted */
    int (*hour)(void *ctx);
};

enum access_state {
    ACCESS_IDLE,
    ACCESS_PIN,
    ACCESS_UNLOCKED,
    ACCESS_OPEN,
    ACCESS_LOCKOUT,
    ACCESS_ALARM,
    ACCESS_ENROLL,
};

enum alarm_kind { ALARM_NONE, ALARM_FORCED, ALARM_HELD };

struct access {
    const struct access_io *io;
    struct settings cfg;
    enum access_state state;
    enum alarm_kind alarm;
    uint32_t deadline;          /* ms: the current state's time limit */
    uint32_t red_until;         /* ms: the red LED after a denial */
    uint32_t shown_seconds;     /* the countdown on screen, to redraw on change */
    bool unlocked;
    bool door_open;
    int slot;                   /* the user whose PIN is being typed */
    struct user user;
    struct card_uid uid;
    char pin[PIN_MAX + 1];
    uint8_t pin_len;
    uint8_t failures;           /* wrong PINs in a row */
    uint8_t lockouts;           /* lockouts since the last correct PIN */
};

void access_init(struct access *a, const struct access_io *io, const struct settings *cfg,
                 bool door_open, uint32_t now);
void access_card(struct access *a, const struct card_uid *uid, uint32_t now);
void access_key(struct access *a, char key, uint32_t now);
void access_door(struct access *a, bool open, uint32_t now);
void access_exit(struct access *a, uint32_t now);
void access_remote_unlock(struct access *a, uint32_t now);
/* The next card presented goes to io->enrolled (20 s) */
void access_enroll(struct access *a, uint32_t now);
void access_alarm_off(struct access *a, uint32_t now);
/* Every 100 ms or so: time limits and countdowns */
void access_tick(struct access *a, uint32_t now);
void access_set_settings(struct access *a, const struct settings *cfg);

const char *access_state_name(enum access_state s);
const char *alarm_name(enum alarm_kind k);

#endif
