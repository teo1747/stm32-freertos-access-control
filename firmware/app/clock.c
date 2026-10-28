#include "clock.h"
#include "console.h"
#include "ds3231.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"

#define RESYNC_MS 60000u

static struct {
    uint32_t base_unix;     /* the time at base_tick */
    TickType_t base_tick;
    TickType_t last_sync;
    bool present;
    bool valid;
    bool have_temperature;
    int temperature;        /* tenths of a degree; the DS3231 updates it every 64 s */
} c;

static void sync(void)
{
    struct datetime dt;
    int err = ds3231_get(&dt);
    TickType_t now = xTaskGetTickCount();

    if (err || !datetime_valid(&dt)) {
        c.present = false;
        return;
    }
    taskENTER_CRITICAL();
    c.base_unix = datetime_to_unix(&dt);
    c.base_tick = now;
    c.last_sync = now;
    c.present = true;
    taskEXIT_CRITICAL();

    int q;
    c.have_temperature = ds3231_temperature(&q) == 0;
    if (c.have_temperature)
        c.temperature = q * 10 / 4;
}

void clock_init(void)
{
    int err = ds3231_init();
    if (err) {
        console_log("clock", "no DS3231 (%s): the time is unknown", i2c_strerror(err));
        return;
    }
    bool lost = true;
    ds3231_lost_power(&lost);
    c.valid = !lost;
    sync();

    char text[20];
    struct datetime dt;
    clock_now_dt(&dt);
    datetime_format(&dt, text, sizeof text);
    console_log("clock", "%s%s", text, c.valid ? "" : " (lost power: set the time)");
}

uint32_t clock_now(void)
{
    taskENTER_CRITICAL();
    uint32_t t = c.base_unix + (xTaskGetTickCount() - c.base_tick) / configTICK_RATE_HZ;
    taskEXIT_CRITICAL();
    return c.present ? t : 0;
}

void clock_now_dt(struct datetime *dt)
{
    datetime_from_unix(clock_now(), dt);
}

bool clock_valid(void)
{
    return c.present && c.valid;
}

bool clock_present(void)
{
    return c.present;
}

int clock_set(const struct datetime *dt)
{
    int err = ds3231_set(dt);
    if (err)
        return err;
    c.valid = true;
    sync();
    return 0;
}

void clock_poll(void)
{
    if (xTaskGetTickCount() - c.last_sync >= pdMS_TO_TICKS(RESYNC_MS))
        sync();
}

bool clock_temperature(int *tenths_c)
{
    *tenths_c = c.temperature;
    return c.present && c.have_temperature;
}
