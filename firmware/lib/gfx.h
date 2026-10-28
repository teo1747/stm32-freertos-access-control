/*
 * Drawing on a 128x64 monochrome framebuffer laid out as the SSD1306's
 * memory: 8 pages of 128 bytes, each byte a column of 8 pixels, bit 0 at
 * the top. The whole buffer goes to the panel in one transfer.
 */
#ifndef GFX_H
#define GFX_H

#include <stdbool.h>
#include <stdint.h>

#define GFX_WIDTH  128
#define GFX_HEIGHT 64

typedef struct {
    uint8_t buf[GFX_WIDTH * GFX_HEIGHT / 8];
} gfx_t;

extern const uint8_t font5x7[95][5];

void gfx_clear(gfx_t *g);
void gfx_pixel(gfx_t *g, int x, int y, bool on);
bool gfx_get(const gfx_t *g, int x, int y);
void gfx_fill(gfx_t *g, int x, int y, int w, int h, bool on);
void gfx_frame(gfx_t *g, int x, int y, int w, int h);
void gfx_invert(gfx_t *g, int x, int y, int w, int h);
void gfx_hline(gfx_t *g, int x, int y, int w);

/* Text in the 5x7 font, each glyph scale x scale pixels a dot, one column
 * of space after it. Returns the x after the text. */
int gfx_text(gfx_t *g, int x, int y, const char *s, int scale);
int gfx_text_width(const char *s, int scale);
void gfx_text_center(gfx_t *g, int y, const char *s, int scale);

/* A picture drawn as text, one string a row: '#' lit, anything else not */
void gfx_picture(gfx_t *g, int x, int y, const char *const *rows, int nrows);

#endif
