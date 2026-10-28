#include "supervisor.h"
#include "admin.h"
#include "board.h"
#include "clock.h"
#include "ui.h"
#include "FreeRTOS.h"
#include "task.h"

static void supervisor_task(void *arg)
{
    /* The devices need the scheduler: their drivers sleep on interrupts */
    clock_init();
    ui_start();
    admin_start();

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        gpio_toggle(PIN_LED_HEARTBEAT);
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(500));
    }
}

void supervisor_start(void)
{
    xTaskCreate(supervisor_task, "supervisor", 384, NULL, configMAX_PRIORITIES - 2, NULL);
}
