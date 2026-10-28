#include <stdlib.h>
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
#include "inputs.h"
#include "storage.h"
#include "terminal.h"
#include "FreeRTOS.h"
#include "task.h"

#define MAX_TASKS       16
#define SESSION_MS      (5u * 60u * 1000u)  /* logged out after 5 idle minutes */
#define LOGIN_TRIES     3
#define LOGIN_LOCK_MS   30000u

static struct shell sh;
static TickType_t session_until;
static bool logged_in;
static int login_failures;
static TickType_t login_locked_until;

/* Commands that change or reveal the terminal's secrets need a login */
static bool authorized(struct shell *s)
{
    TickType_t now = xTaskGetTickCount();
    if (logged_in && (int32_t)(session_until - now) > 0) {
        session_until = now + pdMS_TO_TICKS(SESSION_MS);
        return true;
    }
    logged_in = false;
    shell_print(s, "login first (login <admin PIN>)\r\n");
    return false;
}

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
    shell_printf(s, "terminal   %s%s%s\r\n", access_state_name(terminal_state()),
                 terminal_alarm() != ALARM_NONE ? ", ALARM: " : "",
                 terminal_alarm() != ALARM_NONE ? alarm_name(terminal_alarm()) : "");
    shell_printf(s, "reader     %s\r\n", inputs_reader_status());
    if (storage_ready()) {
        storage_take();
        shell_printf(s, "storage    %u/%u users, %u log records\r\n", userdb_count(storage_users()),
                     USERS_MAX, accesslog_count(storage_log()));
        bool default_pin = userdb_admin_is_default(storage_users());
        storage_give();
        if (default_pin)
            shell_print(s, "warning    the admin PIN is the default: change it (passwd)\r\n");
    } else {
        shell_print(s, "storage    unavailable\r\n");
    }
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
        if (!authorized(s))
            return SHELL_FAILED;
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
        storage_log_event(LOG_TIME_SET, -1, NULL);
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
    if (st.timeouts || st.bus_errors)
        shell_printf(s, "last failure: 0x%02x, %s, %lu byte(s) left, SR1 %04x SR2 %04x CR1 %04x\r\n",
                     st.last_addr, st.last_phase == 0 ? "writing" : st.last_phase == 1 ? "restarting" : "reading",
                     (unsigned long)st.last_left, st.last_sr1, st.last_sr2, st.last_cr1);
    return SHELL_OK;
}

static int cmd_reboot(struct shell *s, int argc, char **argv)
{
    if (!authorized(s))
        return SHELL_FAILED;
    shell_print(s, "rebooting\r\n");
    vTaskDelay(pdMS_TO_TICKS(20));      /* let the line out */
    system_reboot();
}

static int cmd_login(struct shell *s, int argc, char **argv)
{
    TickType_t now = xTaskGetTickCount();

    if (argc != 2)
        return SHELL_USAGE;
    if ((int32_t)(login_locked_until - now) > 0) {
        shell_printf(s, "too many failures: wait %lu s\r\n",
                     (unsigned long)((login_locked_until - now) / configTICK_RATE_HZ + 1));
        return SHELL_FAILED;
    }
    if (!storage_ready()) {
        shell_print(s, "storage unavailable\r\n");
        return SHELL_FAILED;
    }
    storage_take();
    bool ok = userdb_check_admin(storage_users(), argv[1]);
    bool default_pin = ok && userdb_admin_is_default(storage_users());
    storage_give();
    if (!ok) {
        storage_log_event(LOG_ADMIN_FAILED, -1, NULL);
        if (++login_failures >= LOGIN_TRIES) {
            login_failures = 0;
            login_locked_until = now + pdMS_TO_TICKS(LOGIN_LOCK_MS);
        }
        shell_print(s, "wrong PIN\r\n");
        return SHELL_FAILED;
    }
    login_failures = 0;
    logged_in = true;
    session_until = now + pdMS_TO_TICKS(SESSION_MS);
    storage_log_event(LOG_ADMIN_LOGIN, -1, NULL);
    shell_print(s, "logged in\r\n");
    if (default_pin)
        shell_print(s, "warning: the admin PIN is the default: change it (passwd)\r\n");
    return SHELL_OK;
}

static int cmd_logout(struct shell *s, int argc, char **argv)
{
    logged_in = false;
    shell_print(s, "logged out\r\n");
    return SHELL_OK;
}

static int cmd_passwd(struct shell *s, int argc, char **argv)
{
    if (argc != 2)
        return SHELL_USAGE;
    if (!authorized(s))
        return SHELL_FAILED;
    storage_take();
    int err = userdb_set_admin(storage_users(), argv[1]);
    storage_give();
    shell_printf(s, "%s\r\n", err ? userdb_strerror(err) : "admin PIN changed");
    return err ? SHELL_FAILED : SHELL_OK;
}

static int slot_arg(struct shell *s, const char *arg)
{
    char *end;
    long n = strtol(arg, &end, 10);
    if (*end || n < 0 || n >= USERS_MAX) {
        shell_printf(s, "no user %s\r\n", arg);
        return -1;
    }
    return (int)n;
}

static void flags_text(const struct user *u, char *buf, unsigned size)
{
    fmt_snprintf(buf, size, "%s%s%s", (u->flags & USER_PIN) ? "card+PIN" : "card",
                 (u->flags & USER_ADMIN) ? ",admin" : "", (u->flags & USER_DISABLED) ? ",DISABLED" : "");
}

static void user_list(struct shell *s)
{
    struct user u;
    int n = 0;

    shell_print(s, "  #  card                  name              access          hours\r\n");
    storage_take();
    for (int i = 0; i < USERS_MAX; i++) {
        if (userdb_get(storage_users(), i, &u))
            continue;
        char card[32], flags[24], hours[8];
        card_uid_format(&u.uid, card, sizeof card);
        flags_text(&u, flags, sizeof flags);
        if (u.from_hour == u.to_hour || (u.from_hour == 0 && u.to_hour == 24))
            strcpy(hours, "any");
        else
            fmt_snprintf(hours, sizeof hours, "%02u-%02u", u.from_hour, u.to_hour);
        shell_printf(s, " %2d  %-20s  %-16s  %-14s  %s\r\n", i, card, u.name, flags, hours);
        n++;
    }
    storage_give();
    shell_printf(s, "%d user(s)\r\n", n);
}

static int user_add(struct shell *s, int argc, char **argv)
{
    /* user add <uid|card> <name> [PIN] */
    if (argc < 4 || argc > 5)
        return SHELL_USAGE;
    struct user u = { .from_hour = 0, .to_hour = 24 };
    if (strcmp(argv[2], "card") == 0) {
        shell_print(s, "present the new card (20 s)...\r\n");
        if (!terminal_enroll(&u.uid)) {
            shell_print(s, "no card\r\n");
            return SHELL_FAILED;
        }
    } else if (!card_uid_parse(argv[2], &u.uid)) {
        shell_print(s, "bad card UID: 4, 7 or 10 hex bytes\r\n");
        return SHELL_FAILED;
    }
    if (strlen(argv[3]) > USER_NAME_MAX) {
        shell_printf(s, "name too long (%d characters at most)\r\n", USER_NAME_MAX);
        return SHELL_FAILED;
    }
    strcpy(u.name, argv[3]);
    const char *pin = argc == 5 ? argv[4] : NULL;
    if (pin)
        u.flags |= USER_PIN;

    storage_take();
    int slot = userdb_add(storage_users(), &u, pin);
    storage_give();
    char card[32];
    card_uid_format(&u.uid, card, sizeof card);
    if (slot < 0) {
        shell_printf(s, "%s: %s\r\n", card, userdb_strerror(slot));
        return SHELL_FAILED;
    }
    storage_log_event(LOG_USER_ADDED, slot, &u.uid);
    shell_printf(s, "user %d: %s, card %s, %s\r\n", slot, u.name, card, pin ? "card+PIN" : "card");
    return SHELL_OK;
}

/* Changes one thing on a user under the lock */
static int user_change(struct shell *s, int slot, int argc, char **argv)
{
    const char *what = argv[1];
    struct user u;
    int err;

    storage_take();
    err = userdb_get(storage_users(), slot, &u);
    if (err) {
        storage_give();
        shell_printf(s, "no user %d\r\n", slot);
        return SHELL_FAILED;
    }
    if (strcmp(what, "pin") == 0 && argc == 4 && strcmp(argv[3], "none") == 0) {
        u.flags &= (uint8_t)~USER_PIN;
        err = userdb_update(storage_users(), slot, &u);
    } else if (strcmp(what, "pin") == 0 && argc == 4) {
        err = userdb_set_pin(storage_users(), slot, argv[3]);
    } else if (strcmp(what, "admin") == 0 && argc == 4) {
        if (strcmp(argv[3], "on") == 0)
            u.flags |= USER_ADMIN;
        else
            u.flags &= (uint8_t)~USER_ADMIN;
        err = userdb_update(storage_users(), slot, &u);
    } else if ((strcmp(what, "enable") == 0 || strcmp(what, "disable") == 0) && argc == 3) {
        if (what[0] == 'd')
            u.flags |= USER_DISABLED;
        else
            u.flags &= (uint8_t)~USER_DISABLED;
        err = userdb_update(storage_users(), slot, &u);
    } else if (strcmp(what, "hours") == 0 && argc == 5) {
        u.from_hour = (uint8_t)atoi(argv[3]);
        u.to_hour = (uint8_t)atoi(argv[4]);
        err = userdb_update(storage_users(), slot, &u);
    } else if (strcmp(what, "del") == 0 && argc == 3) {
        err = userdb_remove(storage_users(), slot);
    } else {
        storage_give();
        return SHELL_USAGE;
    }
    storage_give();

    if (err) {
        shell_printf(s, "%s\r\n", userdb_strerror(err));
        return SHELL_FAILED;
    }
    if (strcmp(what, "del") == 0)
        storage_log_event(LOG_USER_REMOVED, -1, &u.uid);
    shell_print(s, "done\r\n");
    return SHELL_OK;
}

static int cmd_user(struct shell *s, int argc, char **argv)
{
    if (!authorized(s))
        return SHELL_FAILED;
    if (!storage_ready()) {
        shell_print(s, "storage unavailable\r\n");
        return SHELL_FAILED;
    }
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "list") == 0)) {
        user_list(s);
        return SHELL_OK;
    }
    if (strcmp(argv[1], "add") == 0)
        return user_add(s, argc, argv);
    if (argc < 3)
        return SHELL_USAGE;
    int slot = slot_arg(s, argv[2]);
    return slot < 0 ? SHELL_FAILED : user_change(s, slot, argc, argv);
}

static int cmd_log(struct shell *s, int argc, char **argv)
{
    if (!authorized(s))
        return SHELL_FAILED;
    if (argc == 2 && strcmp(argv[1], "clear") == 0) {
        storage_take();
        int err = accesslog_clear(storage_log());
        storage_give();
        shell_print(s, err ? "error: EEPROM\r\n" : "log cleared\r\n");
        return err ? SHELL_FAILED : SHELL_OK;
    }
    unsigned count = argc == 2 ? (unsigned)atoi(argv[1]) : 10;
    if (argc > 2 || count == 0)
        return SHELL_USAGE;

    storage_take();
    unsigned total = accesslog_count(storage_log());
    for (unsigned back = count < total ? count : total; back-- > 0;) {
        struct log_record r;
        if (accesslog_get(storage_log(), back, &r)) {
            shell_print(s, "  (damaged record)\r\n");
            continue;
        }
        char when[20] = "time not set";
        if (r.time) {
            struct datetime dt;
            datetime_from_unix(r.time, &dt);
            datetime_format(&dt, when, sizeof when);
        }
        char who[24] = "";
        struct user u;
        if (r.user != LOG_NO_USER && userdb_get(storage_users(), r.user, &u) == 0)
            fmt_snprintf(who, sizeof who, "%s", u.name);
        else if (r.uid[0] | r.uid[1] | r.uid[2] | r.uid[3])
            fmt_snprintf(who, sizeof who, "%02X:%02X:%02X:%02X", r.uid[0], r.uid[1], r.uid[2], r.uid[3]);
        shell_printf(s, "%5lu  %s  %-22s %s\r\n", (unsigned long)r.seq, when, log_event_name(r.event), who);
    }
    storage_give();
    return SHELL_OK;
}

static int cmd_door(struct shell *s, int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "open") != 0)
        return SHELL_USAGE;
    if (!authorized(s))
        return SHELL_FAILED;
    terminal_remote_unlock();
    shell_print(s, "unlocked\r\n");
    return SHELL_OK;
}

static int cmd_alarm(struct shell *s, int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "off") != 0)
        return SHELL_USAGE;
    if (!authorized(s))
        return SHELL_FAILED;
    if (terminal_alarm() == ALARM_NONE) {
        shell_print(s, "no alarm\r\n");
        return SHELL_OK;
    }
    terminal_alarm_off();
    shell_print(s, "alarm off\r\n");
    return SHELL_OK;
}

static int cmd_config(struct shell *s, int argc, char **argv)
{
    static const char *const keys[] = { "unlock", "lockout", "held", "pintimeout", "attempts" };
    struct settings cfg;

    if (!authorized(s))
        return SHELL_FAILED;
    storage_take();
    userdb_settings(storage_users(), &cfg);
    storage_give();
    if (argc == 1) {
        shell_printf(s, "unlock      %u s   the lock stays open\r\n", cfg.unlock_s);
        shell_printf(s, "lockout     %u s   after wrong PINs, doubling\r\n", cfg.lockout_s);
        shell_printf(s, "held        %u s   door open before the alarm\r\n", cfg.door_held_s);
        shell_printf(s, "pintimeout  %u s   to type the PIN\r\n", cfg.pin_timeout_s);
        shell_printf(s, "attempts    %u     wrong PINs before the lockout\r\n", cfg.max_attempts);
        return SHELL_OK;
    }
    if (argc != 3)
        return SHELL_USAGE;
    unsigned v = (unsigned)atoi(argv[2]);
    if (strcmp(argv[1], keys[0]) == 0)
        cfg.unlock_s = (uint16_t)v;
    else if (strcmp(argv[1], keys[1]) == 0)
        cfg.lockout_s = (uint16_t)v;
    else if (strcmp(argv[1], keys[2]) == 0)
        cfg.door_held_s = (uint16_t)v;
    else if (strcmp(argv[1], keys[3]) == 0)
        cfg.pin_timeout_s = (uint8_t)v;
    else if (strcmp(argv[1], keys[4]) == 0)
        cfg.max_attempts = (uint8_t)v;
    else
        return SHELL_USAGE;
    storage_take();
    int err = userdb_set_settings(storage_users(), &cfg);
    storage_give();
    if (err) {
        shell_printf(s, "%s: out of range\r\n", argv[1]);
        return SHELL_FAILED;
    }
    terminal_settings_changed();
    shell_print(s, "saved\r\n");
    return SHELL_OK;
}

static int cmd_wipe(struct shell *s, int argc, char **argv)
{
    if (argc != 2 || strcmp(argv[1], "confirm") != 0) {
        shell_print(s, "erases every user and the log, and resets the settings and the admin PIN:\r\n"
                       "type 'wipe confirm'\r\n");
        return SHELL_FAILED;
    }
    if (!authorized(s))
        return SHELL_FAILED;
    storage_take();
    int err = userdb_wipe(storage_users());
    if (!err)
        err = accesslog_clear(storage_log());
    storage_give();
    logged_in = false;
    shell_print(s, err ? "error: EEPROM\r\n" : "wiped; the admin PIN is the default again\r\n");
    terminal_settings_changed();
    return err ? SHELL_FAILED : SHELL_OK;
}

static const struct shell_cmd commands[] = {
    { "status", NULL, "firmware, terminal, storage, uptime, heap", cmd_status },
    { "time", "[set YYYY-MM-DD hh:mm:ss]", "show or set the clock", cmd_time },
    { "login", "<PIN>", "log in as the admin (5 idle minutes)", cmd_login },
    { "logout", NULL, "end the admin session", cmd_logout },
    { "passwd", "<new PIN>", "change the admin PIN", cmd_passwd },
    { "user", "[list]", "the users", cmd_user },
    { "user", "add <uid|card> <name> [PIN]", "enrol (card: present it)", cmd_user },
    { "user", "del|enable|disable <n>", "remove, or switch a user", cmd_user },
    { "user", "pin <n> <PIN|none>", "set or drop a user's PIN", cmd_user },
    { "user", "admin <n> on|off", "admins clear alarms", cmd_user },
    { "user", "hours <n> <from> <to>", "hours of access, 0 24: any", cmd_user },
    { "log", "[count|clear]", "the access log, newest last", cmd_log },
    { "door", "open", "unlock the door now", cmd_door },
    { "alarm", "off", "clear an alarm", cmd_alarm },
    { "config", "[key value]", "show or change the settings", cmd_config },
    { "wipe", "confirm", "factory reset of users, log, settings", cmd_wipe },
    { "top", NULL, "CPU and stack use per task over one second", cmd_top },
    { "faults", "[clear]", "the fault before the last reset", cmd_faults },
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
    xTaskCreate(admin_task, "admin", 768, NULL, 1, NULL);
}
