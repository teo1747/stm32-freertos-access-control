#include <string.h>
#include "access.h"

#define ENROLL_MS       20000u
#define DENIED_MS       2000u
#define GRANTED_MS      3000u
#define DOOR_WARN_S     10u         /* the door-open countdown shows this long before the alarm */
#define MAX_BACKOFF     5           /* the lockout doubles at most 5 times: 32x */

/* Differences, so the millisecond counter may wrap */
static bool reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static uint32_t seconds_left(uint32_t now, uint32_t deadline)
{
    return reached(now, deadline) ? 0 : (deadline - now + 999u) / 1000u;
}

static void leds(struct access *a, uint32_t now)
{
    bool red = a->alarm != ALARM_NONE || a->state == ACCESS_LOCKOUT || !reached(now, a->red_until);
    a->io->leds(a->io->ctx, a->unlocked, red);
}

static void set_lock(struct access *a, bool open)
{
    if (a->unlocked != open) {
        a->unlocked = open;
        a->io->lock(a->io->ctx, open);
    }
}

static void show(struct access *a, const struct screen *s, uint32_t ms)
{
    a->io->show(a->io->ctx, s, ms);
}

static void show_idle(struct access *a)
{
    struct screen s = { .kind = SCREEN_IDLE };
    show(a, &s, 0);
}

static void show_alarm(struct access *a)
{
    struct screen s = { .kind = SCREEN_ALARM };
    strcpy(s.text, a->alarm == ALARM_FORCED ? "Door forced open" : "Door held open");
    show(a, &s, 0);
}

static void show_pin(struct access *a, uint32_t now)
{
    struct screen s = { .kind = SCREEN_PIN, .digits = a->pin_len };
    strcpy(s.name, a->user.name);
    s.seconds = (uint16_t)seconds_left(now, a->deadline);
    a->shown_seconds = s.seconds;
    show(a, &s, 0);
}

static void show_countdown(struct access *a, enum screen_kind kind, uint32_t now)
{
    struct screen s = { .kind = kind };
    s.seconds = (uint16_t)seconds_left(now, a->deadline);
    a->shown_seconds = s.seconds;
    show(a, &s, 0);
}

static void log_event(struct access *a, enum log_event e, int slot, const struct card_uid *uid)
{
    a->io->log(a->io->ctx, e, slot, uid);
}

/* Back to rest: the alarm if one is pending, idle otherwise. A timed screen
 * (granted, denied) goes back to the clock by itself; show_clock is for
 * the states whose screen stays up. */
static void rest(struct access *a, uint32_t now, bool show_clock)
{
    memset(a->pin, 0, sizeof a->pin);
    a->pin_len = 0;
    a->slot = -1;
    if (a->alarm != ALARM_NONE) {
        a->state = ACCESS_ALARM;
        show_alarm(a);
        a->io->sound(a->io->ctx, SOUND_ALARM);
    } else {
        a->state = ACCESS_IDLE;
        if (show_clock)
            show_idle(a);
    }
    leds(a, now);
}

static void deny(struct access *a, const char *reason, uint32_t now)
{
    struct screen s = { .kind = SCREEN_DENIED };
    strncpy(s.text, reason, sizeof s.text - 1);
    a->io->sound(a->io->ctx, SOUND_DENIED);
    a->red_until = now + DENIED_MS;
    rest(a, now, false);
    if (a->state == ACCESS_IDLE)
        show(a, &s, DENIED_MS);
    leds(a, now);
}

static void unlock(struct access *a, uint32_t now)
{
    set_lock(a, true);
    a->deadline = now + a->cfg.unlock_s * 1000u;
    a->state = a->door_open ? ACCESS_OPEN : ACCESS_UNLOCKED;
    if (a->door_open)
        a->deadline = now + a->cfg.door_held_s * 1000u;
    leds(a, now);
}

static void clear_alarm(struct access *a, int slot, uint32_t now)
{
    if (a->alarm == ALARM_NONE)
        return;
    a->alarm = ALARM_NONE;
    a->io->sound(a->io->ctx, SOUND_OFF);
    log_event(a, LOG_ALARM_CLEARED, slot, NULL);
    leds(a, now);
}

static void grant(struct access *a, uint32_t now)
{
    struct screen s = { .kind = SCREEN_GRANTED };
    strcpy(s.name, a->user.name);

    if (a->user.flags & USER_ADMIN)
        clear_alarm(a, a->slot, now);
    log_event(a, LOG_GRANTED, a->slot, &a->uid);
    a->failures = 0;
    a->lockouts = 0;
    a->io->sound(a->io->ctx, SOUND_OK);
    memset(a->pin, 0, sizeof a->pin);
    a->pin_len = 0;
    unlock(a, now);
    show(a, &s, GRANTED_MS);
}

static void start_alarm(struct access *a, enum alarm_kind kind, uint32_t now)
{
    a->alarm = kind;
    log_event(a, kind == ALARM_FORCED ? LOG_DOOR_FORCED : LOG_DOOR_HELD, -1, NULL);
    rest(a, now, true);
}

void access_init(struct access *a, const struct access_io *io, const struct settings *cfg,
                 bool door_open, uint32_t now)
{
    memset(a, 0, sizeof *a);
    a->io = io;
    a->cfg = *cfg;
    a->slot = -1;
    a->door_open = door_open;
    a->red_until = now;
    io->lock(io->ctx, false);
    rest(a, now, true);
}

void access_set_settings(struct access *a, const struct settings *cfg)
{
    a->cfg = *cfg;
}

void access_card(struct access *a, const struct card_uid *uid, uint32_t now)
{
    struct user u;

    switch (a->state) {
    case ACCESS_ENROLL:
        a->io->enrolled(a->io->ctx, uid);
        a->io->sound(a->io->ctx, SOUND_OK);
        rest(a, now, true);
        return;
    case ACCESS_LOCKOUT:
        a->io->sound(a->io->ctx, SOUND_DENIED);
        return;
    case ACCESS_UNLOCKED:
    case ACCESS_OPEN:
        return;     /* already open */
    case ACCESS_IDLE:
    case ACCESS_PIN:
    case ACCESS_ALARM:
        break;
    }

    a->uid = *uid;
    int slot = a->io->find(a->io->ctx, uid, &u);
    if (slot < 0) {
        log_event(a, LOG_DENIED_UNKNOWN, -1, uid);
        deny(a, "Unknown card", now);
        return;
    }
    if (u.flags & USER_DISABLED) {
        log_event(a, LOG_DENIED_DISABLED, slot, uid);
        deny(a, "Card disabled", now);
        return;
    }
    if (a->alarm != ALARM_NONE && !(u.flags & USER_ADMIN)) {
        deny(a, "Alarm: admin only", now);
        return;
    }
    /* Hours: a user with a schedule is refused while the time is unknown */
    int hour = a->io->hour(a->io->ctx);
    bool always = u.from_hour == u.to_hour || (u.from_hour == 0 && u.to_hour == 24);
    if (!always && (hour < 0 || !user_allowed_at(&u, (unsigned)hour))) {
        log_event(a, LOG_DENIED_SCHEDULE, slot, uid);
        deny(a, "Not allowed now", now);
        return;
    }

    a->slot = slot;
    a->user = u;
    if (u.flags & USER_PIN) {
        a->state = ACCESS_PIN;
        a->pin_len = 0;
        memset(a->pin, 0, sizeof a->pin);
        a->deadline = now + a->cfg.pin_timeout_s * 1000u;
        a->io->sound(a->io->ctx, SOUND_KEY);
        show_pin(a, now);
        return;
    }
    grant(a, now);
}

static void wrong_pin(struct access *a, uint32_t now)
{
    log_event(a, LOG_DENIED_PIN, a->slot, &a->uid);
    if (++a->failures < a->cfg.max_attempts) {
        deny(a, "Wrong PIN", now);
        return;
    }
    /* Lockout, twice as long as the last one */
    unsigned shift = a->lockouts < MAX_BACKOFF ? a->lockouts : MAX_BACKOFF;
    a->lockouts++;
    a->failures = 0;
    log_event(a, LOG_LOCKOUT, a->slot, &a->uid);
    a->io->sound(a->io->ctx, SOUND_DENIED);
    a->state = ACCESS_LOCKOUT;
    a->deadline = now + ((uint32_t)a->cfg.lockout_s << shift) * 1000u;
    memset(a->pin, 0, sizeof a->pin);
    a->pin_len = 0;
    show_countdown(a, SCREEN_LOCKOUT, now);
    leds(a, now);
}

void access_key(struct access *a, char key, uint32_t now)
{
    if (a->state != ACCESS_PIN)
        return;

    if (key >= '0' && key <= '9') {
        if (a->pin_len < PIN_MAX) {
            a->pin[a->pin_len++] = key;
            a->pin[a->pin_len] = '\0';
        }
        a->io->sound(a->io->ctx, SOUND_KEY);
    } else if (key == '*') {
        if (a->pin_len == 0) {
            rest(a, now, true);         /* '*' on an empty PIN: cancel */
            return;
        }
        a->pin[--a->pin_len] = '\0';
        a->io->sound(a->io->ctx, SOUND_KEY);
    } else if (key == '#') {
        if (a->pin_len >= PIN_MIN && a->io->check_pin(a->io->ctx, a->slot, a->pin))
            grant(a, now);
        else
            wrong_pin(a, now);
        return;
    } else {
        return;                         /* A to D: unused */
    }
    /* Each key gives the full time again */
    a->deadline = now + a->cfg.pin_timeout_s * 1000u;
    show_pin(a, now);
}

void access_door(struct access *a, bool open, uint32_t now)
{
    if (open == a->door_open)
        return;
    a->door_open = open;

    if (open) {
        if (a->unlocked) {
            a->state = ACCESS_OPEN;
            a->deadline = now + a->cfg.door_held_s * 1000u;
        } else if (a->alarm == ALARM_NONE) {
            start_alarm(a, ALARM_FORCED, now);
        }
        return;
    }

    /* Closed: relock at once */
    set_lock(a, false);
    if (a->alarm == ALARM_HELD)
        clear_alarm(a, -1, now);
    if (a->state == ACCESS_OPEN || a->state == ACCESS_UNLOCKED || a->state == ACCESS_ALARM)
        rest(a, now, a->state != ACCESS_UNLOCKED);
    leds(a, now);
}

void access_exit(struct access *a, uint32_t now)
{
    /* Egress is never refused, alarm or lockout */
    log_event(a, LOG_EXIT, -1, NULL);
    if (a->state == ACCESS_PIN || a->state == ACCESS_ENROLL)
        rest(a, now, true);
    bool was_alarm = a->state == ACCESS_ALARM || a->state == ACCESS_LOCKOUT;
    set_lock(a, true);
    a->state = a->door_open ? ACCESS_OPEN : ACCESS_UNLOCKED;
    a->deadline = now + (a->door_open ? a->cfg.door_held_s : a->cfg.unlock_s) * 1000u;
    if (!was_alarm) {
        struct screen s = { .kind = SCREEN_MESSAGE, .title = "Goodbye", .text = "Door unlocked" };
        show(a, &s, GRANTED_MS);
    }
    a->io->sound(a->io->ctx, a->alarm != ALARM_NONE ? SOUND_ALARM : SOUND_OK);
    leds(a, now);
}

void access_remote_unlock(struct access *a, uint32_t now)
{
    log_event(a, LOG_REMOTE_UNLOCK, -1, NULL);
    if (a->state == ACCESS_PIN || a->state == ACCESS_ENROLL || a->state == ACCESS_LOCKOUT)
        rest(a, now, true);
    unlock(a, now);
    struct screen s = { .kind = SCREEN_MESSAGE, .title = "Welcome", .text = "Opened by the admin" };
    show(a, &s, GRANTED_MS);
    a->io->sound(a->io->ctx, SOUND_OK);
}

void access_enroll(struct access *a, uint32_t now)
{
    if (a->state != ACCESS_IDLE && a->state != ACCESS_PIN) {
        a->io->enrolled(a->io->ctx, NULL);
        return;
    }
    a->state = ACCESS_ENROLL;
    a->deadline = now + ENROLL_MS;
    show_countdown(a, SCREEN_ENROLL, now);
}

void access_alarm_off(struct access *a, uint32_t now)
{
    if (a->alarm == ALARM_NONE)
        return;
    clear_alarm(a, -1, now);
    if (a->state == ACCESS_ALARM)
        rest(a, now, true);
}

void access_tick(struct access *a, uint32_t now)
{
    switch (a->state) {
    case ACCESS_PIN:
        if (reached(now, a->deadline)) {
            log_event(a, LOG_PIN_TIMEOUT, a->slot, &a->uid);
            deny(a, "Too slow", now);
        } else if (seconds_left(now, a->deadline) != a->shown_seconds) {
            show_pin(a, now);
        }
        break;
    case ACCESS_UNLOCKED:
        if (reached(now, a->deadline)) {
            set_lock(a, false);         /* nobody came through */
            rest(a, now, false);
        }
        break;
    case ACCESS_OPEN:
        if (reached(now, a->deadline)) {
            start_alarm(a, ALARM_HELD, now);
        } else if (seconds_left(now, a->deadline) <= DOOR_WARN_S
                   && seconds_left(now, a->deadline) != a->shown_seconds) {
            show_countdown(a, SCREEN_DOOR_OPEN, now);
            a->io->sound(a->io->ctx, SOUND_WARN);
        }
        break;
    case ACCESS_LOCKOUT:
        if (reached(now, a->deadline))
            rest(a, now, true);
        else if (seconds_left(now, a->deadline) != a->shown_seconds)
            show_countdown(a, SCREEN_LOCKOUT, now);
        break;
    case ACCESS_ENROLL:
        if (reached(now, a->deadline)) {
            a->io->enrolled(a->io->ctx, NULL);
            rest(a, now, true);
        } else if (seconds_left(now, a->deadline) != a->shown_seconds) {
            show_countdown(a, SCREEN_ENROLL, now);
        }
        break;
    case ACCESS_IDLE:
    case ACCESS_ALARM:
        break;
    }
    leds(a, now);
}

const char *access_state_name(enum access_state s)
{
    static const char *const names[] = {
        [ACCESS_IDLE] = "idle",
        [ACCESS_PIN] = "PIN entry",
        [ACCESS_UNLOCKED] = "unlocked",
        [ACCESS_OPEN] = "door open",
        [ACCESS_LOCKOUT] = "lockout",
        [ACCESS_ALARM] = "alarm",
        [ACCESS_ENROLL] = "enrolling",
    };
    return names[s];
}

const char *alarm_name(enum alarm_kind k)
{
    return k == ALARM_FORCED ? "door forced" : k == ALARM_HELD ? "door held open" : "none";
}
