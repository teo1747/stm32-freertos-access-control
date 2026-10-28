/*
 * The access rules, driven with fake hardware and a fake clock.
 */
#include "test.h"
#include "access.h"

/* Two users: Ada (admin, card + PIN 2468) and Alan (card alone, 8 to 18) */
static const struct card_uid ADA = { 4, { 0x04, 0xA1, 0xB2, 0xC3 } };
static const struct card_uid ALAN = { 4, { 0x04, 0x11, 0x22, 0x33 } };
static const struct card_uid STRANGER = { 4, { 0xDE, 0xAD, 0xBE, 0xEF } };

static struct {
    bool open;                  /* the lock */
    bool green, red;
    enum sound sound;
    struct screen screen;
    uint32_t screen_ms;
    enum log_event last_log;
    int last_slot;
    unsigned logs;
    int hour;
    bool enrolled_called;
    struct card_uid enrolled;
    bool alan_disabled;
} hw;

static int find(void *ctx, const struct card_uid *uid, struct user *u)
{
    memset(u, 0, sizeof *u);
    u->to_hour = 24;
    if (card_uid_equal(uid, &ADA)) {
        strcpy(u->name, "Ada Lovelace");
        u->flags = USER_PIN | USER_ADMIN;
        return 0;
    }
    if (card_uid_equal(uid, &ALAN)) {
        strcpy(u->name, "Alan Turing");
        u->flags = hw.alan_disabled ? USER_DISABLED : 0;
        u->from_hour = 8;
        u->to_hour = 18;
        return 1;
    }
    return -1;
}

static bool check_pin(void *ctx, int slot, const char *pin)
{
    return slot == 0 && strcmp(pin, "2468") == 0;
}

static void log_cb(void *ctx, enum log_event e, int slot, const struct card_uid *uid)
{
    hw.last_log = e;
    hw.last_slot = slot;
    hw.logs++;
}

static void lock(void *ctx, bool open) { hw.open = open; }
static void leds(void *ctx, bool g, bool r) { hw.green = g; hw.red = r; }
static void sound(void *ctx, enum sound s) { hw.sound = s; }
static int hour(void *ctx) { return hw.hour; }

static void show(void *ctx, const struct screen *s, uint32_t ms)
{
    hw.screen = *s;
    hw.screen_ms = ms;
}

static void enrolled(void *ctx, const struct card_uid *uid)
{
    hw.enrolled_called = true;
    if (uid)
        hw.enrolled = *uid;
    else
        hw.enrolled.len = 0;
}

static const struct access_io io = {
    .find = find, .check_pin = check_pin, .log = log_cb, .lock = lock, .leds = leds,
    .sound = sound, .show = show, .enrolled = enrolled, .hour = hour,
};

static const struct settings cfg = {
    .unlock_s = 5, .lockout_s = 30, .door_held_s = 20, .pin_timeout_s = 15, .max_attempts = 3,
};

static uint32_t now;

static void advance(struct access *a, uint32_t ms)
{
    /* Ticks every 100 ms, as the firmware's task does */
    for (uint32_t t = 0; t < ms; t += 100) {
        now += 100;
        access_tick(a, now);
    }
}

static void type(struct access *a, const char *keys)
{
    for (; *keys; keys++) {
        access_key(a, *keys, now);
        advance(a, 200);
    }
}

static void fresh(struct access *a)
{
    memset(&hw, 0, sizeof hw);
    hw.hour = 10;
    now = 1000;
    access_init(a, &io, &cfg, false, now);
}

int main(void)
{
    struct access a;

    /* Unknown card: denied, logged, red LED for a while */
    fresh(&a);
    CHECK_INT(a.state, ACCESS_IDLE);
    CHECK_INT(hw.screen.kind, SCREEN_IDLE);
    access_card(&a, &STRANGER, now);
    CHECK_INT(hw.last_log, LOG_DENIED_UNKNOWN);
    CHECK_INT(hw.screen.kind, SCREEN_DENIED);
    CHECK_STR(hw.screen.text, "Unknown card");
    CHECK(hw.red && !hw.open);
    advance(&a, 2500);
    CHECK(!hw.red);

    /* Card alone, within hours: granted, then relocked after unlock_s */
    access_card(&a, &ALAN, now);
    CHECK_INT(hw.last_log, LOG_GRANTED);
    CHECK_INT(hw.last_slot, 1);
    CHECK_INT(hw.screen.kind, SCREEN_GRANTED);
    CHECK_STR(hw.screen.name, "Alan Turing");
    CHECK(hw.open && hw.green);
    advance(&a, 4800);
    CHECK(hw.open);
    advance(&a, 300);
    CHECK(!hw.open && !hw.green);
    CHECK_INT(a.state, ACCESS_IDLE);

    /* Outside hours, or with the clock unknown */
    hw.hour = 20;
    access_card(&a, &ALAN, now);
    CHECK_INT(hw.last_log, LOG_DENIED_SCHEDULE);
    CHECK(!hw.open);
    hw.hour = -1;
    access_card(&a, &ALAN, now);
    CHECK_INT(hw.last_log, LOG_DENIED_SCHEDULE);
    hw.hour = 10;

    /* Disabled */
    hw.alan_disabled = true;
    access_card(&a, &ALAN, now);
    CHECK_INT(hw.last_log, LOG_DENIED_DISABLED);
    hw.alan_disabled = false;

    /* Card + PIN, with an erase; the screen shows the digits */
    fresh(&a);
    access_card(&a, &ADA, now);
    CHECK_INT(a.state, ACCESS_PIN);
    CHECK_INT(hw.screen.kind, SCREEN_PIN);
    CHECK_INT(hw.screen.seconds, 15);
    type(&a, "249");
    CHECK_INT(hw.screen.digits, 3);
    type(&a, "*68");
    CHECK_INT(hw.screen.digits, 4);
    CHECK(!hw.open);
    type(&a, "#");
    CHECK_INT(hw.last_log, LOG_GRANTED);
    CHECK(hw.open);

    /* The door opens and closes: relocked at once */
    access_door(&a, true, now);
    CHECK_INT(a.state, ACCESS_OPEN);
    advance(&a, 3000);
    access_door(&a, false, now);
    CHECK(!hw.open);
    CHECK_INT(a.state, ACCESS_IDLE);

    /* '*' on an empty PIN cancels */
    access_card(&a, &ADA, now);
    type(&a, "*");
    CHECK_INT(a.state, ACCESS_IDLE);
    CHECK_INT(hw.screen.kind, SCREEN_IDLE);

    /* The PIN times out; each key gives the full time again */
    access_card(&a, &ADA, now);
    advance(&a, 10000);
    type(&a, "1");
    advance(&a, 10000);
    CHECK_INT(a.state, ACCESS_PIN);
    advance(&a, 6000);
    CHECK_INT(a.state, ACCESS_IDLE);
    CHECK_INT(hw.last_log, LOG_PIN_TIMEOUT);

    /* Three wrong PINs: lockout 30 s, cards ignored meanwhile; the next
     * lockout lasts 60 s; a correct PIN resets the backoff */
    fresh(&a);
    for (int i = 0; i < 3; i++) {
        access_card(&a, &ADA, now);
        type(&a, "1111#");
    }
    CHECK_INT(a.state, ACCESS_LOCKOUT);
    CHECK_INT(hw.last_log, LOG_LOCKOUT);
    CHECK_INT(hw.screen.kind, SCREEN_LOCKOUT);
    CHECK_INT(hw.screen.seconds, 30);
    CHECK(hw.red);
    access_card(&a, &ADA, now);
    CHECK_INT(a.state, ACCESS_LOCKOUT);
    advance(&a, 29000);
    CHECK_INT(a.state, ACCESS_LOCKOUT);
    CHECK(hw.screen.seconds <= 2);
    advance(&a, 1100);
    CHECK_INT(a.state, ACCESS_IDLE);
    for (int i = 0; i < 3; i++) {
        access_card(&a, &ADA, now);
        type(&a, "1111#");
    }
    CHECK_INT(hw.screen.seconds, 60);
    advance(&a, 60100);
    access_card(&a, &ADA, now);
    type(&a, "2468#");
    CHECK_INT(hw.last_log, LOG_GRANTED);
    CHECK_INT(a.lockouts, 0);
    advance(&a, 6000);

    /* Too short a PIN counts as wrong */
    access_card(&a, &ADA, now);
    type(&a, "24#");
    CHECK_INT(hw.last_log, LOG_DENIED_PIN);
    CHECK_INT(a.failures, 1);

    /* Door forced: alarm until an admin clears it; the door closing
     * doesn't end it; non-admins are refused meanwhile */
    fresh(&a);
    access_door(&a, true, now);
    CHECK_INT(a.state, ACCESS_ALARM);
    CHECK_INT(a.alarm, ALARM_FORCED);
    CHECK_INT(hw.last_log, LOG_DOOR_FORCED);
    CHECK_INT(hw.sound, SOUND_ALARM);
    CHECK_INT(hw.screen.kind, SCREEN_ALARM);
    access_door(&a, false, now);
    CHECK_INT(a.state, ACCESS_ALARM);
    access_card(&a, &ALAN, now);
    CHECK(!hw.open);
    CHECK_INT(a.state, ACCESS_ALARM);
    CHECK_INT(hw.last_log, LOG_DENIED_ALARM);
    /* An admin's wrong PIN keeps the alarm */
    access_card(&a, &ADA, now);
    type(&a, "0000#");
    CHECK_INT(a.state, ACCESS_ALARM);
    access_card(&a, &ADA, now);
    type(&a, "2468#");
    CHECK_INT(a.alarm, ALARM_NONE);
    CHECK_INT(hw.sound, SOUND_OK);
    CHECK(hw.open);

    /* The exit button opens even during an alarm */
    fresh(&a);
    access_door(&a, true, now);
    access_door(&a, false, now);
    access_exit(&a, now);
    CHECK(hw.open);
    CHECK_INT(hw.last_log, LOG_EXIT);
    access_door(&a, true, now);
    access_door(&a, false, now);
    CHECK(!hw.open);
    CHECK_INT(a.state, ACCESS_ALARM);       /* still pending */
    access_alarm_off(&a, now);
    CHECK_INT(a.state, ACCESS_IDLE);
    CHECK_INT(hw.last_log, LOG_ALARM_CLEARED);

    /* Held open: countdown 10 s before, then an alarm that clears when the
     * door closes */
    fresh(&a);
    access_exit(&a, now);
    access_door(&a, true, now);
    advance(&a, 10500);
    CHECK_INT(hw.screen.kind, SCREEN_DOOR_OPEN);
    CHECK_INT(hw.screen.seconds, 10);
    advance(&a, 10000);
    CHECK_INT(a.alarm, ALARM_HELD);
    CHECK_INT(hw.last_log, LOG_DOOR_HELD);
    access_door(&a, false, now);
    CHECK_INT(a.alarm, ALARM_NONE);
    CHECK_INT(a.state, ACCESS_IDLE);
    CHECK(!hw.open);

    /* Remote unlock from the shell */
    fresh(&a);
    access_remote_unlock(&a, now);
    CHECK(hw.open);
    CHECK_INT(hw.last_log, LOG_REMOTE_UNLOCK);
    advance(&a, 5100);
    CHECK(!hw.open);

    /* Enrol: the next card goes to the admin, unknown or not; a timeout
     * reports none */
    fresh(&a);
    access_enroll(&a, now);
    CHECK_INT(hw.screen.kind, SCREEN_ENROLL);
    access_card(&a, &STRANGER, now);
    CHECK(hw.enrolled_called);
    CHECK(card_uid_equal(&hw.enrolled, &STRANGER));
    CHECK_INT(a.state, ACCESS_IDLE);
    CHECK(!hw.open);
    hw.enrolled_called = false;
    access_enroll(&a, now);
    advance(&a, 20100);
    CHECK(hw.enrolled_called);
    CHECK_INT(hw.enrolled.len, 0);
    CHECK_INT(a.state, ACCESS_IDLE);

    /* The millisecond counter wrapping doesn't confuse the time limits */
    fresh(&a);
    now = 0xFFFFF000u;
    access_card(&a, &ALAN, now);
    CHECK(hw.open);
    advance(&a, 4000);
    CHECK(hw.open);
    advance(&a, 1200);
    CHECK(!hw.open);

    return test_report("access");
}
