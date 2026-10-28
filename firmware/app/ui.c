#include <string.h>
#include "ui.h"
#include "clock.h"
#include "console.h"
#include "i2c.h"
#include "ssd1306.h"
#include "supervisor.h"
#include "FreeRTOS.h"
#include "task.h"

#define REFRESH_MS  250

static TaskHandle_t task;
static struct screen current = { .kind = SCREEN_IDLE };
static TickType_t expires;          /* 0: the screen stays */
static uint32_t frames;
static gfx_t frame, shown;          /* drawn, and last sent to the panel */

void ui_show(const struct screen *s, uint32_t ms)
{
    taskENTER_CRITICAL();
    current = *s;
    expires = ms ? xTaskGetTickCount() + pdMS_TO_TICKS(ms) : 0;
    if (ms && !expires)
        expires = 1;
    taskEXIT_CRITICAL();
    if (task)
        xTaskNotifyGive(task);
}

void ui_idle(void)
{
    struct screen s = { .kind = SCREEN_IDLE };
    ui_show(&s, 0);
}

enum screen_kind ui_current(void)
{
    return current.kind;
}

uint32_t ui_frames_sent(void)
{
    return frames;
}

static void ui_task(void *arg)
{
    int err = ssd1306_init();
    if (err)
        console_log("ui", "no OLED (%s)", i2c_strerror(err));
    memset(&shown, 0xFF, sizeof shown);     /* differs from any first frame */

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(REFRESH_MS));
        supervisor_checkin(CHECKIN_UI);
        clock_poll();

        struct screen s;
        taskENTER_CRITICAL();
        if (expires && (int32_t)(xTaskGetTickCount() - expires) >= 0) {
            current = (struct screen){ .kind = SCREEN_IDLE };
            expires = 0;
        }
        s = current;
        taskEXIT_CRITICAL();

        struct screen_env env = { .time_valid = clock_valid() };
        clock_now_dt(&env.now);
        env.have_temperature = clock_temperature(&env.temperature);
        env.blink = (xTaskGetTickCount() / pdMS_TO_TICKS(500)) & 1;

        screen_render(&frame, &s, &env);
        /* Only changed frames go out: the bus is shared with the EEPROM */
        if (memcmp(&frame, &shown, sizeof frame) != 0) {
            if (ssd1306_show(&frame) == 0) {
                shown = frame;
                frames++;
            }
        }
    }
}

void ui_start(void)
{
    xTaskCreate(ui_task, "ui", 384, NULL, 2, &task);
}
