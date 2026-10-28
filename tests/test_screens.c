/*
 * Renders every screen. Besides the checks, it writes each one as ASCII art
 * (screens.txt) and as a PBM image (screen-*.pbm) for the documentation.
 */
#include "test.h"
#include "screens.h"

static int lit(const gfx_t *g, int x, int y, int w, int h)
{
    int n = 0;
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            n += gfx_get(g, i, j);
    return n;
}

static void dump(FILE *txt, const char *name, const gfx_t *g)
{
    fprintf(txt, "== %s\n", name);
    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++)
            fputc(gfx_get(g, x, y) ? '#' : '.', txt);
        fputc('\n', txt);
    }

    char path[64];
    snprintf(path, sizeof path, "screen-%s.pbm", name);
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    fprintf(f, "P1\n%d %d\n", GFX_WIDTH, GFX_HEIGHT);
    for (int y = 0; y < GFX_HEIGHT; y++) {
        for (int x = 0; x < GFX_WIDTH; x++)
            fputs(gfx_get(g, x, y) ? "1 " : "0 ", f);
        fputc('\n', f);
    }
    fclose(f);
}

/* Nothing drawn outside the panel: gfx clips, so this checks that the
 * layouts keep their text inside (a cut-off line shows as a lit edge) */
static void check_margins(const gfx_t *g, const char *name)
{
    int edge = lit(g, GFX_WIDTH - 1, 10, 1, GFX_HEIGHT - 10);
    if (edge > 0 && edge < GFX_HEIGHT - 10)
        fprintf(stderr, "note: %s touches the right edge (%d pixels)\n", name, edge);
}

int main(void)
{
    gfx_t g;
    FILE *txt = fopen("screens.txt", "w");
    CHECK(txt != NULL);

    /* The font: 'A' as the table draws it */
    gfx_clear(&g);
    gfx_text(&g, 0, 0, "A", 1);
    static const char *const a[7] = {
        ".###.", "#...#", "#...#", "#...#", "#####", "#...#", "#...#",
    };
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 5; x++)
            CHECK_INT(gfx_get(&g, x, y), a[y][x] == '#');
    CHECK_INT(gfx_text_width("AB", 1), 11);
    CHECK_INT(gfx_text_width("AB", 2), 22);

    /* Clipping */
    gfx_clear(&g);
    gfx_pixel(&g, -1, 0, true);
    gfx_pixel(&g, 128, 64, true);
    CHECK_INT(lit(&g, 0, 0, GFX_WIDTH, GFX_HEIGHT), 0);

    struct screen_env env = {
        .now = { 2026, 10, 28, 17, 42, 5, 3 },
        .time_valid = true,
        .have_temperature = true,
        .temperature = 235,
    };
    struct screen s = { .kind = SCREEN_IDLE };

    screen_render(&g, &s, &env);
    dump(txt, "idle", &g);
    CHECK(lit(&g, 0, 0, GFX_WIDTH, 9) > 50);         /* the date bar */
    CHECK(lit(&g, 0, 15, GFX_WIDTH, 21) > 150);      /* the time */
    CHECK(lit(&g, 0, 45, GFX_WIDTH, 13) > 60);       /* card and prompt */

    env.time_valid = false;
    screen_render(&g, &s, &env);
    dump(txt, "idle-no-time", &g);
    env.time_valid = true;

    s = (struct screen){ .kind = SCREEN_PIN, .name = "Ada Lovelace", .digits = 3, .seconds = 9 };
    screen_render(&g, &s, &env);
    dump(txt, "pin", &g);
    /* Three boxes filled, three empty */
    int x0 = (GFX_WIDTH - (6 * 14 + 5 * 4)) / 2;
    CHECK_INT(lit(&g, x0 + 2 * 18 + 4, 40, 6, 6), 36);
    CHECK_INT(lit(&g, x0 + 3 * 18 + 4, 40, 6, 6), 0);

    s = (struct screen){ .kind = SCREEN_GRANTED, .name = "Ada Lovelace" };
    screen_render(&g, &s, &env);
    dump(txt, "granted", &g);
    CHECK(lit(&g, 0, 0, GFX_WIDTH, 46) > GFX_WIDTH * 46 / 2);   /* inverted */

    s = (struct screen){ .kind = SCREEN_DENIED, .text = "Unknown card" };
    screen_render(&g, &s, &env);
    dump(txt, "denied", &g);

    s = (struct screen){ .kind = SCREEN_LOCKOUT, .seconds = 27 };
    screen_render(&g, &s, &env);
    dump(txt, "lockout", &g);

    s = (struct screen){ .kind = SCREEN_DOOR_OPEN, .seconds = 12 };
    screen_render(&g, &s, &env);
    dump(txt, "door-open", &g);

    s = (struct screen){ .kind = SCREEN_ALARM, .text = "Door forced open" };
    screen_render(&g, &s, &env);
    dump(txt, "alarm", &g);

    s = (struct screen){ .kind = SCREEN_ENROLL, .seconds = 20 };
    screen_render(&g, &s, &env);
    dump(txt, "enroll", &g);
    check_margins(&g, "enroll");

    if (txt)
        fclose(txt);
    return test_report("screens");
}
