#include <string.h>
#include "gfx.h"

void gfx_clear(gfx_t *g)
{
    memset(g->buf, 0, sizeof g->buf);
}

void gfx_pixel(gfx_t *g, int x, int y, bool on)
{
    if (x < 0 || x >= GFX_WIDTH || y < 0 || y >= GFX_HEIGHT)
        return;
    uint8_t *b = &g->buf[(y / 8) * GFX_WIDTH + x];
    uint8_t bit = (uint8_t)(1u << (y % 8));
    *b = on ? (uint8_t)(*b | bit) : (uint8_t)(*b & ~bit);
}

bool gfx_get(const gfx_t *g, int x, int y)
{
    if (x < 0 || x >= GFX_WIDTH || y < 0 || y >= GFX_HEIGHT)
        return false;
    return (g->buf[(y / 8) * GFX_WIDTH + x] >> (y % 8)) & 1u;
}

void gfx_fill(gfx_t *g, int x, int y, int w, int h, bool on)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            gfx_pixel(g, i, j, on);
}

void gfx_hline(gfx_t *g, int x, int y, int w)
{
    gfx_fill(g, x, y, w, 1, true);
}

void gfx_frame(gfx_t *g, int x, int y, int w, int h)
{
    gfx_fill(g, x, y, w, 1, true);
    gfx_fill(g, x, y + h - 1, w, 1, true);
    gfx_fill(g, x, y, 1, h, true);
    gfx_fill(g, x + w - 1, y, 1, h, true);
}

void gfx_invert(gfx_t *g, int x, int y, int w, int h)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            gfx_pixel(g, i, j, !gfx_get(g, i, j));
}

static void glyph(gfx_t *g, int x, int y, char c, int scale)
{
    if (c < ' ' || c > '~')
        c = '?';
    const uint8_t *cols = font5x7[c - ' '];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if ((cols[col] >> row) & 1u)
                gfx_fill(g, x + col * scale, y + row * scale, scale, scale, true);
}

int gfx_text(gfx_t *g, int x, int y, const char *s, int scale)
{
    for (; *s; s++) {
        glyph(g, x, y, *s, scale);
        x += 6 * scale;
    }
    return x;
}

int gfx_text_width(const char *s, int scale)
{
    int n = (int)strlen(s);
    return n ? (6 * n - 1) * scale : 0;
}

void gfx_text_center(gfx_t *g, int y, const char *s, int scale)
{
    gfx_text(g, (GFX_WIDTH - gfx_text_width(s, scale)) / 2, y, s, scale);
}

void gfx_picture(gfx_t *g, int x, int y, const char *const *rows, int nrows)
{
    for (int j = 0; j < nrows; j++)
        for (int i = 0; rows[j][i]; i++)
            if (rows[j][i] == '#')
                gfx_pixel(g, x + i, y + j, true);
}
