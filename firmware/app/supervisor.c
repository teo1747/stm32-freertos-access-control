#include "supervisor.h"
#include "admin.h"
#include "board.h"
#include "clock.h"
#include "console.h"
#include "door.h"
#include "fault.h"
#include "inputs.h"
#include "iwdg.h"
#include "storage.h"
#include "terminal.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "task.h"

#define WATCHDOG_MS     3000    /* the IWDG's timeout */
#define WINDOW_MS       1000    /* every task checks in within this */
#define MISSED_WINDOWS  3       /* a task silent this many windows is stuck */
#define ALL_CHECKINS    ((1u << CHECKIN_COUNT) - 1u)

static EventGroupHandle_t checkins;

void supervisor_checkin(enum checkin who)
{
    if (checkins)
        xEventGroupSetBits(checkins, 1u << who);
}

static const char *checkin_name(unsigned bit)
{
    static const char *const names[CHECKIN_COUNT] = { "terminal", "reader", "inputs", "ui" };
    return bit < CHECKIN_COUNT ? names[bit] : "?";
}

static void supervisor_task(void *arg)
{
    /* The devices need the scheduler: their drivers sleep on interrupts */
    clock_init();
    storage_init();
    door_init();
    bool door_open = inputs_init();
    terminal_start(door_open);
    inputs_start();
    ui_start();
    admin_start();

    iwdg_start(WATCHDOG_MS);
    console_log("supervisor", "running; watchdog %u ms", WATCHDOG_MS);

    int missed[CHECKIN_COUNT] = { 0 };
    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(WINDOW_MS / 2));
        gpio_toggle(PIN_LED_HEARTBEAT);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(WINDOW_MS / 2));
        gpio_toggle(PIN_LED_HEARTBEAT);

        EventBits_t seen = xEventGroupClearBits(checkins, ALL_CHECKINS);
        bool healthy = true;
        for (unsigned i = 0; i < CHECKIN_COUNT; i++) {
            if (seen & (1u << i)) {
                missed[i] = 0;
                continue;
            }
            healthy = false;
            if (++missed[i] >= MISSED_WINDOWS)
                fault_raise(FAULT_WATCHDOG, checkin_name(i), (uint32_t)missed[i]);
        }
        if (healthy)
            iwdg_feed();
    }
}

void supervisor_start(void)
{
    checkins = xEventGroupCreate();
    configASSERT(checkins);
    xTaskCreate(supervisor_task, "supervisor", 512, NULL, configMAX_PRIORITIES - 2, NULL);
}
