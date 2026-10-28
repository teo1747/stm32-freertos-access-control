#include "terminal.h"
#include "clock.h"
#include "door.h"
#include "storage.h"
#include "supervisor.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#define TICK_MS 100

enum event_kind { EV_CARD, EV_KEY, EV_DOOR, EV_EXIT, EV_REMOTE, EV_ENROLL, EV_ALARM_OFF, EV_SETTINGS };

struct event {
    enum event_kind kind;
    union {
        struct card_uid uid;
        char key;
        bool open;
    };
};

static QueueHandle_t queue;
static QueueHandle_t enrolled_queue;    /* to the admin shell: a card, or len 0 */
static struct access access;
static bool door_initially_open;

/* The callbacks the rules reach the world through */

static int io_find(void *ctx, const struct card_uid *uid, struct user *u)
{
    if (!storage_ready())
        return -1;
    storage_take();
    int slot = userdb_find(storage_users(), uid);
    if (slot >= 0)
        userdb_get(storage_users(), slot, u);
    storage_give();
    return slot;
}

static bool io_check_pin(void *ctx, int slot, const char *pin)
{
    storage_take();
    bool ok = userdb_check_pin(storage_users(), slot, pin);
    storage_give();
    return ok;
}

static void io_log(void *ctx, enum log_event e, int slot, const struct card_uid *uid)
{
    storage_log_event(e, slot, uid);
}

static void io_lock(void *ctx, bool open)
{
    door_lock(open);
}

static void io_leds(void *ctx, bool green, bool red)
{
    door_leds(green, red);
}

static void io_sound(void *ctx, enum sound s)
{
    door_sound(s);
}

static void io_show(void *ctx, const struct screen *s, uint32_t ms)
{
    ui_show(s, ms);
}

static void io_enrolled(void *ctx, const struct card_uid *uid)
{
    struct card_uid none = { 0 };
    xQueueOverwrite(enrolled_queue, uid ? uid : &none);
}

static int io_hour(void *ctx)
{
    if (!clock_valid())
        return -1;
    struct datetime dt;
    clock_now_dt(&dt);
    return dt.hour;
}

static const struct access_io io = {
    .find = io_find,
    .check_pin = io_check_pin,
    .log = io_log,
    .lock = io_lock,
    .leds = io_leds,
    .sound = io_sound,
    .show = io_show,
    .enrolled = io_enrolled,
    .hour = io_hour,
};

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static struct settings current_settings(void)
{
    struct settings s = { 5, 30, 20, 15, 3 };
    if (storage_ready()) {
        storage_take();
        userdb_settings(storage_users(), &s);
        storage_give();
    }
    return s;
}

static void terminal_task(void *arg)
{
    struct settings cfg = current_settings();
    access_init(&access, &io, &cfg, door_initially_open, now_ms());

    for (;;) {
        struct event ev;
        if (xQueueReceive(queue, &ev, pdMS_TO_TICKS(TICK_MS))) {
            uint32_t now = now_ms();
            switch (ev.kind) {
            case EV_CARD: access_card(&access, &ev.uid, now); break;
            case EV_KEY: access_key(&access, ev.key, now); break;
            case EV_DOOR: access_door(&access, ev.open, now); break;
            case EV_EXIT: access_exit(&access, now); break;
            case EV_REMOTE: access_remote_unlock(&access, now); break;
            case EV_ENROLL: access_enroll(&access, now); break;
            case EV_ALARM_OFF: access_alarm_off(&access, now); break;
            case EV_SETTINGS:
                cfg = current_settings();
                access_set_settings(&access, &cfg);
                break;
            }
        }
        access_tick(&access, now_ms());
        supervisor_checkin(CHECKIN_TERMINAL);
    }
}

void terminal_start(bool door_open)
{
    door_initially_open = door_open;
    queue = xQueueCreate(16, sizeof(struct event));
    enrolled_queue = xQueueCreate(1, sizeof(struct card_uid));
    configASSERT(queue && enrolled_queue);
    xTaskCreate(terminal_task, "terminal", 512, NULL, 3, NULL);
}

static void post(const struct event *ev)
{
    /* Inputs come at human speed: a full queue means the task is stuck,
     * and the watchdog deals with that */
    xQueueSend(queue, ev, 0);
}

void terminal_card(const struct card_uid *uid)
{
    struct event ev = { .kind = EV_CARD, .uid = *uid };
    post(&ev);
}

void terminal_key(char key)
{
    struct event ev = { .kind = EV_KEY, .key = key };
    post(&ev);
}

void terminal_door(bool open)
{
    struct event ev = { .kind = EV_DOOR, .open = open };
    post(&ev);
}

void terminal_exit(void)
{
    struct event ev = { .kind = EV_EXIT };
    post(&ev);
}

void terminal_remote_unlock(void)
{
    struct event ev = { .kind = EV_REMOTE };
    post(&ev);
}

void terminal_alarm_off(void)
{
    struct event ev = { .kind = EV_ALARM_OFF };
    post(&ev);
}

void terminal_settings_changed(void)
{
    struct event ev = { .kind = EV_SETTINGS };
    post(&ev);
}

bool terminal_enroll(struct card_uid *uid)
{
    struct event ev = { .kind = EV_ENROLL };
    xQueueReset(enrolled_queue);
    post(&ev);
    /* The terminal answers within its 20 s, timeout included */
    if (!xQueueReceive(enrolled_queue, uid, pdMS_TO_TICKS(25000)))
        return false;
    return uid->len != 0;
}

enum access_state terminal_state(void)
{
    return access.state;
}

enum alarm_kind terminal_alarm(void)
{
    return access.alarm;
}
