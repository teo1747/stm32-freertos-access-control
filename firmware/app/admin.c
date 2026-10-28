#include <string.h>
#include "admin.h"
#include "board.h"
#include "clock.h"
#include "console.h"
#include "fault.h"
#include "i2c.h"
#include "shell.h"
#include "system.h"
#include "uart.h"
#include "fmt.h"
#include "FreeRTOS.h"
#include "task.h"

#define MAX_TASKS 16

static struct shell sh;

static void shell_write(void *ctx, const char *s, size_t n)
{
    console_write(s, n);
}

static int cmd_status(struct shell *s, int argc, char **argv)
{
    uint32_t ms = xTaskGetTickCount();
    shell_printf(s, "firmware   access-terminal %s\r\n", FIRMWARE_VERSION);
    shell_printf(s, "uptime     %lu.%03lu s\r\n", (unsigned long)(ms / 1000), (unsigned long)(ms % 1000));
    shell_printf(s, "clock      %lu MHz\r\n", (unsigned long)(system_hclk() / 1000000u));
    shell_printf(s, "reset      %s\r\n", system_reset_name(system_reset_cause()));
    shell_printf(s, "heap       %lu bytes free, %lu at the lowest\r\n",
                 (unsigned long)xPortGetFreeHeapSize(), (unsigned long)xPortGetMinimumEverFreeHeapSize());
    shell_printf(s, "device id  %08lx\r\n", (unsigned long)system_uid32());
    return SHELL_OK;
}

static const char *state_name(eTaskState state)
{
    switch (state) {
    case eRunning: return "run";
    case eReady: return "ready";
    case eBlocked: return "wait";
    case eSuspended: return "susp";
    default: return "?";
    }
}

/* CPU use per task over one second: the counters wrap, differences don't */
static int cmd_top(struct shell *s, int argc, char **argv)
{
    static TaskStatus_t before[MAX_TASKS], after[MAX_TASKS];
    configRUN_TIME_COUNTER_TYPE t0, t1;

    UBaseType_t n0 = uxTaskGetSystemState(before, MAX_TASKS, &t0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    UBaseType_t n1 = uxTaskGetSystemState(after, MAX_TASKS, &t1);
    uint32_t total = t1 - t0;

    shell_print(s, "task          prio  state  stack free   cpu\r\n");
    for (UBaseType_t i = 0; i < n1; i++) {
        const TaskStatus_t *a = &after[i];
        uint32_t used = a->ulRunTimeCounter;
        for (UBaseType_t j = 0; j < n0; j++)
            if (before[j].xHandle == a->xHandle)
                used -= before[j].ulRunTimeCounter;
        uint32_t permille = total ? (uint32_t)((uint64_t)used * 1000u / total) : 0;
        shell_printf(s, "%-12s  %4lu  %-5s  %10lu  %3lu.%lu%%\r\n", a->pcTaskName,
                     (unsigned long)a->uxCurrentPriority, state_name(a->eCurrentState),
                     (unsigned long)a->usStackHighWaterMark * sizeof(StackType_t),
                     (unsigned long)(permille / 10), (unsigned long)(permille % 10));
    }
    shell_printf(s, "heap: %lu bytes free\r\n", (unsigned long)xPortGetFreeHeapSize());
    return SHELL_OK;
}

static int cmd_faults(struct shell *s, int argc, char **argv)
{
    const struct fault_record *r = fault_last();

    if (argc == 2 && strcmp(argv[1], "clear") == 0) {
        fault_clear();
        shell_print(s, "cleared\r\n");
        return SHELL_OK;
    }
    if (argc != 1)
        return SHELL_USAGE;
    if (!r) {
        shell_print(s, "no fault since power-on\r\n");
        return SHELL_OK;
    }
    char line[160];
    fault_describe(r, line, sizeof line);
    shell_printf(s, "%s\r\n%lu fault(s) since power-on\r\n", line, (unsigned long)r->count);
    return SHELL_OK;
}

static int cmd_time(struct shell *s, int argc, char **argv)
{
    struct datetime dt;
    char text[20];

    if (argc == 4 && strcmp(argv[1], "set") == 0) {
        char joined[24];
        fmt_snprintf(joined, sizeof joined, "%s %s", argv[2], argv[3]);
        if (!datetime_parse(joined, &dt)) {
            shell_print(s, "error: expected YYYY-MM-DD hh:mm:ss, 2000 to 2099\r\n");
            return SHELL_FAILED;
        }
        int err = clock_set(&dt);
        if (err) {
            shell_printf(s, "error: RTC: %s\r\n", i2c_strerror(err));
            return SHELL_FAILED;
        }
    } else if (argc != 1) {
        return SHELL_USAGE;
    }
    if (!clock_present()) {
        shell_print(s, "no RTC\r\n");
        return SHELL_FAILED;
    }
    clock_now_dt(&dt);
    datetime_format(&dt, text, sizeof text);
    int t;
    bool temp = clock_temperature(&t);
    shell_printf(s, "%s %s%s", datetime_weekday_name(dt.weekday), text,
                 clock_valid() ? "" : " (not set since power loss)");
    if (temp)
        shell_printf(s, ", %d.%d C", t / 10, (t < 0 ? -t : t) % 10);
    shell_print(s, "\r\n");
    return SHELL_OK;
}

static int cmd_i2c(struct shell *s, int argc, char **argv)
{
    static const struct { uint8_t addr; const char *what; } known[] = {
        { I2C_ADDR_EEPROM, "24LC256 EEPROM" },
        { I2C_ADDR_OLED, "SSD1306 OLED" },
        { I2C_ADDR_RTC, "DS3231 RTC" },
    };
    int found = 0;

    for (uint8_t a = 0x08; a < 0x78; a++) {
        if (!i2c_probe(a))
            continue;
        const char *what = "?";
        for (size_t i = 0; i < sizeof known / sizeof known[0]; i++)
            if (known[i].addr == a)
                what = known[i].what;
        shell_printf(s, "  0x%02x  %s\r\n", a, what);
        found++;
    }
    struct i2c_stats st;
    i2c_get_stats(&st);
    shell_printf(s, "%d device(s); %lu transfers, %lu NACKs, %lu bus errors, %lu timeouts, %lu recoveries\r\n",
                 found, (unsigned long)st.transfers, (unsigned long)st.nacks, (unsigned long)st.bus_errors,
                 (unsigned long)st.timeouts, (unsigned long)st.recoveries);
    return SHELL_OK;
}

static int cmd_reboot(struct shell *s, int argc, char **argv)
{
    shell_print(s, "rebooting\r\n");
    vTaskDelay(pdMS_TO_TICKS(20));      /* let the line out */
    system_reboot();
}

static const struct shell_cmd commands[] = {
    { "status", NULL, "firmware, uptime, clock, reset cause, heap", cmd_status },
    { "top", NULL, "CPU and stack use per task over one second", cmd_top },
    { "faults", "[clear]", "the fault before the last reset", cmd_faults },
    { "time", "[set YYYY-MM-DD hh:mm:ss]", "show or set the clock", cmd_time },
    { "i2c", NULL, "scan the bus, transfer statistics", cmd_i2c },
    { "reboot", NULL, "software reset", cmd_reboot },
};

static void admin_task(void *arg)
{
    shell_init(&sh, commands, sizeof commands / sizeof commands[0], shell_write, NULL);
    shell_prompt(&sh);
    for (;;) {
        char buf[16];
        size_t n = uart_read(&uart2, buf, sizeof buf, portMAX_DELAY);
        for (size_t i = 0; i < n; i++)
            shell_input(&sh, buf[i]);
    }
}

void admin_start(void)
{
    xTaskCreate(admin_task, "admin", 512, NULL, 1, NULL);
}
