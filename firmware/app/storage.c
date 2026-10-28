#include "storage.h"
#include "clock.h"
#include "console.h"
#include "eeprom.h"
#include "fmt.h"
#include "i2c.h"
#include "system.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

static SemaphoreHandle_t lock;
static struct userdb users;
static struct accesslog events;
static bool ready;

static int ee_read(void *ctx, uint32_t addr, void *buf, size_t n)
{
    return eeprom_read((uint16_t)addr, buf, n);
}

static int ee_write(void *ctx, uint32_t addr, const void *buf, size_t n)
{
    return eeprom_write((uint16_t)addr, buf, n);
}

static const struct store eeprom_store = { ee_read, ee_write, NULL };

/* No RNG on the F446: the chip's unique ID and the cycle counter's jitter
 * against the tick, through xorshift. Good enough for salts, which must be
 * different, not secret. */
static uint32_t random32(void)
{
    static uint32_t x;
    if (!x)
        x = system_uid32() ^ 0x9E3779B9u;
    x ^= system_cycles() ^ (xTaskGetTickCount() << 16);
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

bool storage_init(void)
{
    lock = xSemaphoreCreateMutex();
    configASSERT(lock);

    int err = userdb_open(&users, &eeprom_store, random32);
    if (err) {
        console_log("storage", "EEPROM: %s; users and log unavailable", userdb_strerror(err));
        return false;
    }
    if (accesslog_open(&events, &eeprom_store)) {
        console_log("storage", "EEPROM: the access log can't be read");
        return false;
    }
    ready = true;
    if (users.formatted)
        console_log("storage", "blank EEPROM: formatted, admin PIN is the default (%s)", ADMIN_DEFAULT_PIN);
    console_log("storage", "%u user(s), %u log record(s)%s", userdb_count(&users), accesslog_count(&events),
                users.dropped || events.dropped ? ", damaged records dropped" : "");
    storage_log_event(LOG_BOOT, -1, NULL);
    return true;
}

bool storage_ready(void)
{
    return ready;
}

void storage_take(void)
{
    xSemaphoreTake(lock, portMAX_DELAY);
}

void storage_give(void)
{
    xSemaphoreGive(lock);
}

struct userdb *storage_users(void)
{
    return &users;
}

struct accesslog *storage_log(void)
{
    return &events;
}

void storage_log_event(enum log_event e, int slot, const struct card_uid *uid)
{
    char who[24] = "";
    struct user u;

    if (!ready)
        return;
    storage_take();
    if (slot >= 0 && userdb_get(&users, slot, &u) == 0) {
        fmt_snprintf(who, sizeof who, " (%s)", u.name);
    } else if (uid) {
        who[0] = ' ';
        card_uid_format(uid, who + 1, sizeof who - 1);
    }
    int err = accesslog_append(&events, clock_valid() ? clock_now() : 0, e, slot, uid);
    storage_give();

    console_log("access", "%s%s%s", log_event_name(e), who, err ? " [not logged: EEPROM]" : "");
}
