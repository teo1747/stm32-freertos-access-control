/*
 * 128x64 layouts. The top 9 rows are a status bar (date, temperature or a
 * title), then the screen's content.
 */
#include <string.h>
#include "screens.h"
#include "fmt.h"

static const char *const icon_locked[] = {
    "....#####....",
    "...##...##...",
    "..##.....##..",
    "..#.......#..",
    "..#.......#..",
    "..#.......#..",
    ".###########.",
    "#############",
    "######.######",
    "#####...#####",
    "#####...#####",
    "######.######",
    "######.######",
    "######.######",
    "#############",
    ".###########.",
};

static const char *const icon_unlocked[] = {
    "....#####....",
    "...##...##...",
    "..##.....##..",
    "..#.......#..",
    "..#.......#..",
    "..#..........",
    "..#..........",
    ".###########.",
    "#############",
    "######.######",
    "#####...#####",
    "#####...#####",
    "######.######",
    "######.######",
    "#############",
    ".###########.",
};

static const char *const icon_card[] = {
    ".################.",
    "#................#",
    "#..####..........#",
    "#..#..#..........#",
    "#..####....###...#",
    "#.........#...#..#",
    "#........#.....#.#",
    "#.........#...#..#",
    "#..........###...#",
    "#................#",
    "#..##########....#",
    "#................#",
    ".################.",
};

static const char *const month_names[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static void status_bar(gfx_t *g, const char *left, const char *right)
{
    gfx_text(g, 0, 0, left, 1);
    if (right)
        gfx_text(g, GFX_WIDTH - gfx_text_width(right, 1), 0, right, 1);
    gfx_hline(g, 0, 9, GFX_WIDTH);
}

static void date_bar(gfx_t *g, const struct screen_env *env)
{
    char date[16] = "";
    char temp[10] = "";

    if (env->time_valid)
        fmt_snprintf(date, sizeof date, "%s %u %s %u", datetime_weekday_name(env->now.weekday),
                     env->now.day, month_names[(env->now.month + 11) % 12], env->now.year);
    else
        strcpy(date, "Time not set");
    if (env->have_temperature) {
        int t = env->temperature;
        fmt_snprintf(temp, sizeof temp, "%s%d.%dC", t < 0 ? "-" : "", (t < 0 ? -t : t) / 10,
                     (t < 0 ? -t : t) % 10);
    }
    status_bar(g, date, temp);
}

static void idle(gfx_t *g, const struct screen_env *env)
{
    char hm[6], sec[4];

    date_bar(g, env);
    if (env->time_valid) {
        fmt_snprintf(hm, sizeof hm, "%02u:%02u", env->now.hour, env->now.minute);
        fmt_snprintf(sec, sizeof sec, "%02u", env->now.second);
    } else {
        strcpy(hm, "--:--");
        strcpy(sec, "--");
    }
    /* HH:MM large, the seconds small beside them */
    int w = gfx_text_width(hm, 3) + 2 + gfx_text_width(sec, 1);
    int x = (GFX_WIDTH - w) / 2;
    x = gfx_text(g, x, 15, hm, 3);
    gfx_text(g, x - 1, 29, sec, 1);

    gfx_picture(g, 14, 45, icon_card, 13);
    gfx_text(g, 40, 48, "Present card", 1);
}

static void pin_entry(gfx_t *g, const struct screen *s)
{
    char left[8];
    fmt_snprintf(left, sizeof left, "%us", s->seconds);
    status_bar(g, s->name, left);

    gfx_text_center(g, 14, "Enter PIN", 2);

    /* One box a digit, filled once typed */
    const int bw = 14, gap = 4;
    int x0 = (GFX_WIDTH - (SCREEN_PIN_SLOTS * bw + (SCREEN_PIN_SLOTS - 1) * gap)) / 2;
    for (int i = 0; i < SCREEN_PIN_SLOTS; i++) {
        int x = x0 + i * (bw + gap);
        gfx_frame(g, x, 35, bw, 16);
        if (i < s->digits)
            gfx_fill(g, x + 4, 40, bw - 8, 6, true);
    }
    gfx_text_center(g, 56, "# enter   * erase", 1);
}

/* A large two-line verdict in a frame, the icon on its left, and a line
 * under it */
static void verdict(gfx_t *g, const char *const *icon, const char *a, const char *b,
                    const char *line)
{
    const int text_left = 26;
    int room = GFX_WIDTH - 4 - text_left;

    gfx_frame(g, 0, 0, GFX_WIDTH, 46);
    gfx_frame(g, 2, 2, GFX_WIDTH - 4, 42);
    gfx_picture(g, 8, 15, icon, 16);
    gfx_text(g, text_left + (room - gfx_text_width(a, 2)) / 2, 7, a, 2);
    gfx_text(g, text_left + (room - gfx_text_width(b, 2)) / 2, 26, b, 2);
    gfx_text_center(g, 53, line, 1);
}

static void granted(gfx_t *g, const struct screen *s)
{
    verdict(g, icon_unlocked, "ACCESS", "GRANTED", s->name);
    /* White on black reads as "go" from across a corridor */
    gfx_invert(g, 0, 0, GFX_WIDTH, 46);
}

static void denied(gfx_t *g, const struct screen *s)
{
    verdict(g, icon_locked, "ACCESS", "DENIED", s->text);
}

static void lockout(gfx_t *g, const struct screen *s)
{
    char line[22];
    status_bar(g, "Too many attempts", NULL);
    gfx_picture(g, 57, 14, icon_locked, 16);
    gfx_text_center(g, 34, "LOCKED", 2);
    fmt_snprintf(line, sizeof line, "Try again in %us", s->seconds);
    gfx_text_center(g, 55, line, 1);
}

static void door_open(gfx_t *g, const struct screen *s, const struct screen_env *env)
{
    char line[22];
    date_bar(g, env);
    gfx_picture(g, 57, 13, icon_unlocked, 16);
    gfx_text_center(g, 33, "DOOR OPEN", 2);
    fmt_snprintf(line, sizeof line, "Close it within %us", s->seconds);
    gfx_text_center(g, 55, line, 1);
}

static void alarm(gfx_t *g, const struct screen *s, const struct screen_env *env)
{
    gfx_text_center(g, 8, "ALARM", 3);
    gfx_text_center(g, 38, s->text, 1);
    gfx_text_center(g, 52, "Admin: alarm off", 1);
    if (env->blink)
        gfx_invert(g, 0, 0, GFX_WIDTH, GFX_HEIGHT);
}

static void enroll(gfx_t *g, const struct screen *s)
{
    char line[22];
    status_bar(g, "Admin", NULL);
    gfx_picture(g, 55, 14, icon_card, 13);
    gfx_text_center(g, 32, "New card", 2);
    fmt_snprintf(line, sizeof line, "Present it (%us)", s->seconds);
    gfx_text_center(g, 54, line, 1);
}

static void message(gfx_t *g, const struct screen *s)
{
    gfx_text_center(g, 14, s->title, 2);
    gfx_text_center(g, 40, s->text, 1);
}

void screen_render(gfx_t *g, const struct screen *s, const struct screen_env *env)
{
    gfx_clear(g);
    switch (s->kind) {
    case SCREEN_IDLE: idle(g, env); break;
    case SCREEN_PIN: pin_entry(g, s); break;
    case SCREEN_GRANTED: granted(g, s); break;
    case SCREEN_DENIED: denied(g, s); break;
    case SCREEN_LOCKOUT: lockout(g, s); break;
    case SCREEN_DOOR_OPEN: door_open(g, s, env); break;
    case SCREEN_ALARM: alarm(g, s, env); break;
    case SCREEN_ENROLL: enroll(g, s); break;
    case SCREEN_MESSAGE: message(g, s); break;
    }
}
